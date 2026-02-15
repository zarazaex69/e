#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <lzma.h>

#pragma pack(push, 1)
typedef struct {
    char name[4];
    char owner[4];
    uint32_t offset;
    uint32_t size;
    uint32_t tokens;
    uint32_t reserved[2];
    uint32_t flags;
} FPT_Entry;

typedef struct {
    uint32_t signature;
    uint32_t module_id;
    uint32_t module_type;
    uint32_t flags1;
    uint32_t flags2;
    uint32_t reserved;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t load_address;
    uint32_t entry_point;
    uint32_t hash[2];
    char module_name[16];
    uint8_t padding[8];
} ME_Module_Entry;
#pragma pack(pop)

uint32_t find_fpt(uint8_t *firmware, size_t size) {
    const uint8_t sig[] = {'$', 'F', 'P', 'T'};
    for (size_t i = 0; i < size - 4; i++) {
        if (memcmp(&firmware[i], sig, 4) == 0) {
            return i;
        }
    }
    return 0;
}

FPT_Entry* find_code_partition(uint8_t *firmware, uint32_t fpt_offset) {
    FPT_Entry *entry = (FPT_Entry*)(firmware + fpt_offset + 0x20);
    for (int i = 0; i < 16; i++) {
        if (memcmp(entry[i].name, "CODE", 4) == 0) {
            return &entry[i];
        }
    }
    return NULL;
}

uint32_t find_manifest(uint8_t *firmware, uint32_t code_offset, uint32_t code_size) {
    const uint8_t sig[] = {'$', 'M', 'A', 'N'};
    for (uint32_t i = code_offset; i < code_offset + code_size - 4; i++) {
        if (memcmp(&firmware[i], sig, 4) == 0) {
            return i;
        }
    }
    return 0;
}

ME_Module_Entry* find_module(uint8_t *firmware, size_t firmware_size, const char *module_name) {
    const uint8_t sig[] = {'$', 'M', 'O', 'D'};
    int count = 0;
    
    for (size_t offset = 0; offset < firmware_size - 0x50; offset++) {
        if (memcmp(&firmware[offset], sig, 4) == 0) {
            ME_Module_Entry *entry = (ME_Module_Entry*)(firmware + offset);
            
            if (entry->module_type > 0x100 || entry->compressed_size > 0x1000000) {
                continue;
            }
            
            fprintf(stderr, "DEBUG: $MOD #%d at 0x%lx: type=0x%02x name='%.16s' comp=%u uncomp=%u\n",
                    count++, offset, entry->module_type, entry->module_name, 
                    entry->compressed_size, entry->uncompressed_size);
            
            if (strncmp(entry->module_name, module_name, 16) == 0) {
                fprintf(stderr, "DEBUG: MATCH FOUND!\n");
                return entry;
            }
        }
    }
    
    fprintf(stderr, "DEBUG: Module '%s' not found after checking %d $MOD entries\n", module_name, count);
    return NULL;
}

int find_all_modules(uint8_t *firmware, size_t firmware_size, ME_Module_Entry ***modules_out, int *count_out) {
    const uint8_t sig[] = {'$', 'M', 'O', 'D'};
    int capacity = 32;
    int count = 0;
    
    ME_Module_Entry **modules = malloc(capacity * sizeof(ME_Module_Entry*));
    if (!modules) {
        return -1;
    }
    
    for (size_t offset = 0; offset < firmware_size - 0x50; offset++) {
        if (memcmp(&firmware[offset], sig, 4) == 0) {
            ME_Module_Entry *entry = (ME_Module_Entry*)(firmware + offset);
            
            if (entry->module_type > 0x100 || entry->compressed_size > 0x1000000) {
                continue;
            }
            
            if (count >= capacity) {
                capacity *= 2;
                ME_Module_Entry **new_modules = realloc(modules, capacity * sizeof(ME_Module_Entry*));
                if (!new_modules) {
                    free(modules);
                    return -1;
                }
                modules = new_modules;
            }
            
            modules[count++] = entry;
        }
    }
    
    *modules_out = modules;
    *count_out = count;
    return 0;
}

int decompress_module(uint8_t *compressed_data, uint32_t compressed_size,
                     uint32_t uncompressed_size, uint8_t **output) {
    
    uint8_t props = compressed_data[0];
    uint32_t dict_size;
    memcpy(&dict_size, compressed_data + 1, 4);
    
    size_t data_size = compressed_size - 5;
    uint8_t *fixed_stream = malloc(13 + data_size);
    if (!fixed_stream) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }
    
    fixed_stream[0] = props;
    memcpy(fixed_stream + 1, &dict_size, 4);
    uint64_t uncompressed_size_64 = uncompressed_size;
    memcpy(fixed_stream + 5, &uncompressed_size_64, 8);
    memcpy(fixed_stream + 13, compressed_data + 5, data_size);
    
    *output = malloc(uncompressed_size);
    if (!*output) {
        free(fixed_stream);
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }
    
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_alone_decoder(&strm, UINT64_MAX);
    
    if (ret != LZMA_OK) {
        free(fixed_stream);
        free(*output);
        fprintf(stderr, "LZMA decoder initialization failed: %d\n", ret);
        return -1;
    }
    
    strm.next_in = fixed_stream;
    strm.avail_in = 13 + data_size;
    strm.next_out = *output;
    strm.avail_out = uncompressed_size;
    
    ret = lzma_code(&strm, LZMA_FINISH);
    
    lzma_end(&strm);
    free(fixed_stream);
    
    if (ret != LZMA_STREAM_END) {
        free(*output);
        fprintf(stderr, "LZMA decompression failed: %d\n", ret);
        return -1;
    }
    
    return 0;
}

int extract_single_module(uint8_t *firmware, size_t firmware_size, ME_Module_Entry *module, const char *module_name);

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <firmware.bin> [module_name]\n", argv[0]);
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s me1x200.bin              - Extract ALL modules\n", argv[0]);
        fprintf(stderr, "  %s me1x200.bin KernelPriv   - Extract specific module\n", argv[0]);
        return 1;
    }
    
    const char *firmware_path = argv[1];
    const char *module_name = (argc == 3) ? argv[2] : NULL;
    
    FILE *f = fopen(firmware_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open firmware file '%s'\n", firmware_path);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t firmware_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *firmware = malloc(firmware_size);
    if (!firmware) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return 1;
    }
    
    if (fread(firmware, 1, firmware_size, f) != firmware_size) {
        fprintf(stderr, "Error: Failed to read firmware file\n");
        free(firmware);
        fclose(f);
        return 1;
    }
    fclose(f);
    
    uint32_t fpt_offset = find_fpt(firmware, firmware_size);
    if (!fpt_offset) {
        fprintf(stderr, "Error: FPT signature not found\n");
        free(firmware);
        return 1;
    }
    
    FPT_Entry *code_part = find_code_partition(firmware, fpt_offset);
    if (!code_part) {
        fprintf(stderr, "Error: CODE partition not found\n");
        free(firmware);
        return 1;
    }
    
    uint32_t man_offset = find_manifest(firmware, code_part->offset, code_part->size);
    if (!man_offset) {
        fprintf(stderr, "Error: $MAN manifest not found\n");
        free(firmware);
        return 1;
    }
    
    system("mkdir -p out");
    
    if (module_name == NULL) {
        ME_Module_Entry **modules;
        int module_count;
        
        if (find_all_modules(firmware, firmware_size, &modules, &module_count) != 0) {
            fprintf(stderr, "Error: Failed to find modules\n");
            free(firmware);
            return 1;
        }
        
        printf("Found %d modules. Extracting all...\n\n", module_count);
        
        int success_count = 0;
        for (int i = 0; i < module_count; i++) {
            char name[17];
            memcpy(name, modules[i]->module_name, 16);
            name[16] = '\0';
            
            for (int j = 15; j >= 0; j--) {
                if (name[j] == ' ' || name[j] == '\0') {
                    name[j] = '\0';
                } else {
                    break;
                }
            }
            
            printf("[%d/%d] Extracting '%s'...\n", i + 1, module_count, name);
            
            if (extract_single_module(firmware, firmware_size, modules[i], name) == 0) {
                success_count++;
            }
        }
        
        free(modules);
        free(firmware);
        
        printf("Extraction complete: %d/%d modules extracted successfully\n", success_count, module_count);
        return 0;
    }
    
    ME_Module_Entry *module = find_module(firmware, firmware_size, module_name);
    if (!module) {
        fprintf(stderr, "Error: Module '%s' not found\n", module_name);
        free(firmware);
        return 1;
    }
    
    int result = extract_single_module(firmware, firmware_size, module, module_name);
    free(firmware);
    return result;
}

int extract_single_module(uint8_t *firmware, size_t firmware_size, ME_Module_Entry *module, const char *module_name) {
    uint32_t entry_offset = (uint8_t*)module - firmware;
    uint32_t data_offset = entry_offset + 0x50;
    uint8_t *data = firmware + data_offset;
    
    int compressed = (module->compressed_size != module->uncompressed_size) &&
                     (module->compressed_size > 0) &&
                     (module->uncompressed_size > 0);
    
    uint8_t *output_data;
    size_t output_size;
    
    if (compressed) {
        output_size = module->uncompressed_size;
        
        fprintf(stderr, "DEBUG: First 32 bytes of compressed data:\n");
        for (int i = 0; i < 32 && i < module->compressed_size; i++) {
            fprintf(stderr, "%02x ", data[i]);
            if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
        }
        fprintf(stderr, "\n");
        
        if (decompress_module(data, module->compressed_size, module->uncompressed_size, &output_data) != 0) {
            fprintf(stderr, "Error: Decompression failed\n");
            free(firmware);
            return 1;
        }
    } else {
        output_size = module->compressed_size;
        output_data = malloc(output_size);
        if (!output_data) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(firmware);
            return 1;
        }
        memcpy(output_data, data, output_size);
    }
    
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "out/%s.bin", module_name);
    
    FILE *out = fopen(output_filename, "wb");
    if (!out) {
        system("mkdir -p out");
        out = fopen(output_filename, "wb");
        if (!out) {
            fprintf(stderr, "Error: Cannot create output file '%s'\n", output_filename);
            free(output_data);
            free(firmware);
            return 1;
        }
    }
    
    if (fwrite(output_data, 1, output_size, out) != output_size) {
        fprintf(stderr, "Error: Failed to write output file\n");
        fclose(out);
        free(output_data);
        free(firmware);
        return 1;
    }
    
    fclose(out);
    free(output_data);
    
    printf("  -> '%s' (%zu bytes, %s)\n", output_filename, output_size, compressed ? "decompressed" : "uncompressed");
    
    return 0;
}
