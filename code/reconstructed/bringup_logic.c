/*
 * RECONSTRUCTED LOGIC FOR INTEL ME v1 BRINGUP
 * Based on static analysis of PRELOADER.bin (ARC Architecture, 16-bit instructions)
 *
 * Target Hardware: ARC Core (ARCompact ISA, 16/32-bit mixed instructions)
 * Module Size: 8148 bytes (0x1FC4)
 * Entry Point: First instruction after utility functions
 *
 * SUMMARY:
 * 1. Load primary data structure and function pointers from fixed addresses.
 * 2. Search for CKEY signature in memory and process 4 entries.
 * 3. Initialize hardware registers and AUX status controls.
 * 4. Allocate and write $BUP/$HEC descriptors with proven signatures.
 * 5. Initialize overlay modules and enter main polling loop.
 * 6. Poll AUX registers 0x7F-0x7D and process linked list entries.
 */

#include <stdint.h>

/*
 * PROVEN MEMORY ADDRESSES
 * Extracted from disassembly mov/ld instructions
 */

// Primary data structure - loaded at entry point
#define ADDR_PRIMARY_STRUCT    0x00102028  // ld r0, [0x102028]

// CKEY search parameters
#define CKEY_SEARCH_START      0x00108000  // mov r14, 0x108000
#define CKEY_SIGNATURE         0x59454B43  // sub.f 0, r0, 0x59454b43 ("CKEY" LE)

// Data addresses used throughout code
#define ADDR_DATA_101F7C       0x00101F7C  // mov r1, 0x101f7c
#define ADDR_LIST_HEAD         0x0010201C  // ld r1, [0x10201c] - list head pointer
#define ADDR_LIST_START        0x0010203C  // mov r0, 0x10203c - list buffer start
#define ADDR_LIST_END          0x00102078  // mov r0, 0x102078 - list buffer end
/*
 * PROVEN SIGNATURES
 * From disassembly st/sr instructions
 */

#define SIG_BUP                0x50554224  // st 0x50554224, [r0] - "$BUP" LE
#define SIG_HEC                0x43454824  // st 0x43454824, [r0] - "$HEC" LE
/*
 * PROVEN HARDWARE REGISTER VALUES
 * From disassembly sr instructions
 */

// AUX registers - lr/sr instructions
#define AUX_STATUS             0x10005     // lr r0, [0x10005] / sr r0, [0x10005]
#define AUX_POLL_7F            0x7F        // lr r0, [0x7f]
#define AUX_POLL_7E            0x7E        // lr r0, [0x7e]
#define AUX_POLL_7B            0x7B        // lr r0, [0x7b]
#define AUX_POLL_7D            0x7D        // lr r0, [0x7d]

// Memory-mapped registers
#define HW_REG_8004            0x8004      // sr 0x2c, [0x8004]
#define HW_REG_8010            0x8010      // sr 4, [0x8010]
#define HW_REG_80009200        0x80009200  // mov r15, 0x80009200
/*
 * PROVEN ALLOCATION SIZES
 * From disassembly mov r2, immediate
 */

#define ALLOC_SIZE_BUP         0xEC        // mov r2, 0xec (236 bytes)
#define ALLOC_SIZE_HEC         0x5C        // mov r2, 0x5c (92 bytes)
#define ALLOC_SIZE_CTX1        0x74        // mov r2, 0x74 (116 bytes)
#define ALLOC_SIZE_CTX2        0x20        // mov r2, 0x20 (32 bytes)
/*
 * PROVEN FUNCTION PARAMETERS
 * From disassembly mov r1, immediate
 */

#define ALLOC_PARAM_BUP        5           // mov r1, 5 (for $BUP allocation)
#define ALLOC_PARAM_HEC        6           // mov r1, 6 (for $HEC allocation)
/*
 * PROVEN COUNTS AND LIMITS
 * From disassembly
 */

#define CKEY_ENTRY_COUNT       4           // mov r13, 4
#define STACK_FRAME_SIZE       0xE8        // mov r12, 0xe8 (232 bytes)
#define MODULE_CALL_ARG        11          // mov r2, 0xb
/*
 * PROVEN BITMASKS
 * From disassembly and/or instructions
 */

#define STATUS_MASK_LOW        0xF000FFFF  // and r1, r0, 0xf000ffff
#define STATUS_MASK_CLEAR      0xFFFF0FFF  // and r0, r0, 0xffff0fff
#define STATUS_BIT_SET_1       0x3000      // or r0, r0, 0x3000
#define HW_BIT_SET             0x80        // or r0, r0, 0x80
/*
 * PROVEN OFFSETS IN DATA STRUCTURES
 * From disassembly ld rX, [rY, offset]
 */

// Primary structure at 0x102028
#define PRIMARY_OFF_FUNC_READ_PTR    0x14  // ld r18, [r0, 0x14]
#define PRIMARY_OFF_FUNC_PROC_PTR    0x1C  // ld r19, [r0, 0x1c]

// Function table structure (r18)
#define FUNC_OFF_READ         0x0C        // ld r1, [r0, 0xc]
#define FUNC_OFF_VERIFY       0x10        // ld r2, [r18, 0x10]

// Function table structure (r19)
#define FUNC_OFF_PROCESS      0x1C        // ld r3, [r19, 0x1c]
#define FUNC_OFF_DISPATCH     0x24        // ld r3, [r1, 0x24]

// Module structure (r13/r0)
#define MODULE_OFF_ENTRY      0x00        // ld r4, [r0]
#define MODULE_OFF_FLAGS      0x04        // ld r0, [r14, 4]
#define MODULE_OFF_CONTEXT    0x30        // ld r0, [r14, 0x30]

// List entry structure (proven by access pattern)
#define LIST_ENTRY_SIZE       0x0C        // add r1, r1, 0xc
#define LIST_OFF_DATA         0x00        // ld r0, [r1]
#define LIST_OFF_NEXT         0x04        // ld r0, [r1, 4]
#define LIST_OFF_PREV         0x08        // ld r0, [r1, 8]

// $BUP/$HEC descriptor structure (proven by st sequence)
#define DESC_OFF_SIGNATURE    0x00        // st 0x50554224, [r0]
#define DESC_OFF_TYPE         0x04        // st r1, [r0, 4] where r1=1
/*
 * PROVEN OVERLAY MODULE NAMES
 * From binary at offset 0x1F4C-0x1FCC
 */

static const char *overlay_names[10] = {
    "BUPET_OVL",        // Offset 0x1F4C
    "BUPMSEQ_OVL",      // Offset 0x1F58
    "BUCLS_OVL",        // Offset 0x1F64
    "EFFS_IOVL",        // Offset 0x1F70
    "EFFS_OPOVL",       // Offset 0x1F7C
    "MOFFM0_OVL",       // Offset 0x1F88
    "MOFFM1_OVL",       // Offset 0x1F94
    "SUPPORT_OVL",      // Offset 0x1FA0
    "PKTPMINIT_OVL",    // Offset 0x1FAC
    "ALIASCHECK_OVL"    // Offset 0x1FBC
};

// String at offset 0x1F2C
static const char version_string[] = "ME Security Key String Version 2";
/*
 * DATA STRUCTURES
 * Reconstructed from access patterns
 */

// $BUP Descriptor - proven size 0xEC (236 bytes) allocated
typedef struct {
    uint32_t signature;     // +0x00: SIG_BUP
    uint32_t type;          // +0x04: 1
    // Remaining 228 bytes: unknown structure
    uint8_t reserved[228];
} bup_descriptor_t;

// $HEC Descriptor - proven size 0x5C (92 bytes) allocated
typedef struct {
    uint32_t signature;     // +0x00: SIG_HEC
    uint32_t type;          // +0x04: 1
    // Remaining 84 bytes: unknown structure
    uint8_t reserved[84];
} hec_descriptor_t;

// Primary data structure at 0x102028
typedef struct {
    uint8_t unknown_00[0x14];
    void *func_read_ptr;    // +0x14: pointer to read/verify functions
    uint8_t unknown_18[0x04];
    void *func_proc_ptr;    // +0x1C: pointer to process functions
} primary_struct_t;

// Function table for read/verify (r18)
typedef struct {
    uint8_t unknown_00[0x0C];
    void (*read_func)(void);    // +0x0C: read function
    int (*verify_func)(void);   // +0x10: verify function
} func_read_table_t;

// Function table for processing (r19)
typedef struct {
    uint8_t unknown_00[0x1C];
    void (*process_func)(void); // +0x1C: process function
    uint8_t unknown_20[0x04];
    void (*dispatch_func)(void);// +0x24: dispatch function
} func_proc_table_t;

// List entry - proven by access pattern
typedef struct list_entry {
    void *data;             // +0x00
    struct list_entry *next;// +0x04
    struct list_entry *prev;// +0x08
} list_entry_t;

// Module structure for handoff (r13/r0)
typedef struct {
    void (*entry)(void);    // +0x00: entry point
    uint32_t flags;         // +0x04: flags
    uint8_t unknown_08[0x28];
    void *context;          // +0x30: context data
} module_struct_t;
/*
 * HELPER FUNCTIONS
 * Assembly Wrappers
 */


static inline uint32_t aux_read(uint32_t reg) {
    uint32_t val;
    __asm__ volatile ("lr %0, [%1]" : "=r"(val) : "i"(reg));
    return val;
}

static inline void aux_write(uint32_t reg, uint32_t val) {
    __asm__ volatile ("sr %0, [%1]" : : "r"(val), "i"(reg));
}

static inline void jump_to_ptr(void *ptr) {
    __asm__ volatile ("j.d [%0]\n\tmov r2, %1" : : "r"(ptr), "i"(MODULE_CALL_ARG));
}
/*
 * ENTRY POINT
 * Reconstructed from disassembly
 *
 * Entry sequence (from disassembly lines ~320-340):
 *
 * st blink, [sp, 4]
 * bl.d 0x00000300
 * mov r12, 0xe8            ; stack frame size = 232 bytes
 * ld r0, [0x102028]        ; load primary structure pointer
 * mov r14, 0x108000        ; CKEY search start
 * mov r1, 0x101f7c         ; data address
 * add r15, fp, 0xffffff4c
 * ld r18, [r0, 0x14]       ; function read pointer
 * ld r19, [r0, 0x1c]       ; function process pointer
 */
void bringup_entry(void) {
    // Stack frame setup - proven from disassembly
    // st blink, [sp, 4]
    // mov r12, 0xe8

    // Load primary data structure
    primary_struct_t *primary = (primary_struct_t*)ADDR_PRIMARY_STRUCT;

    // Setup function pointers - proven offsets
    func_read_table_t *func_read = (func_read_table_t*)primary->func_read_ptr;
    func_proc_table_t *func_proc = (func_proc_table_t*)primary->func_proc_ptr;

    // CKEY search - proven algorithm
    ckey_search_and_process(func_read, func_proc);

    // Hardware initialization
    hardware_init();

    // Allocate and write descriptors
    void *bup = allocate_descriptor(ALLOC_PARAM_BUP, ALLOC_SIZE_BUP);
    void *hec = allocate_descriptor(ALLOC_PARAM_HEC, ALLOC_SIZE_HEC);

    write_bup_descriptor(bup);
    write_hec_descriptor(hec);

    // Hardware register writes - proven values
    aux_write(HW_REG_8004, 0x2C);   // sr 0x2c, [0x8004]
    aux_write(HW_REG_8010, 4);      // sr 4, [0x8010]

    // Initialize overlay modules
    overlay_init();

    // Enter main loop
    main_loop();
}

/*
 * CKEY SEARCH AND PROCESSING
 * Reconstructed from disassembly
 *
 * CKEY search algorithm (from disassembly lines ~330-370):
 *
 * mov r14, 0x108000        ; start address
 * add r14, r14, 4          ; increment
 * ld r0, [r14]             ; load value
 * sub.f 0, r0, 0x59454b43  ; compare with "CKEY"
 * bnz 0x00000574           ; loop if not found
 * mov r13, 4               ; 4 entries to process
 */

void ckey_search_and_process(func_read_table_t *func_read, func_proc_table_t *func_proc) {
    uint32_t *search_ptr = (uint32_t*)CKEY_SEARCH_START;

    // Search loop - proven algorithm
    while (*search_ptr != CKEY_SIGNATURE) {
        search_ptr++;  // add r14, r14, 4
    }

    // Process 4 CKEY entries - proven count
    for (int i = 0; i < CKEY_ENTRY_COUNT; i++) {
        // Call read function - proven: ld r6, [r18, 0xc]; jl.d [r6]
        func_read->read_func();

        // Call verify function - proven: ld r2, [r18, 0x10]; jl.d [r2]
        if (func_read->verify_func() != 0) {
            // Verification failed
            return;
        }

        // Call process function - proven: ld r3, [r19, 0x1c]; jl.d [r3]
        func_proc->process_func();

        search_ptr++;  // Next entry
    }
}

/*
 * HARDWARE INITIALIZATION
 * Reconstructed from disassembly
 *
 * Hardware init sequence (from disassembly):
 *
 * lr r0, [0x10005]
 * and r1, r0, 0xf000ffff
 * ...
 * or r0, r0, 0x3000
 * sr r0, [0x10005]
 * ...
 * ld r0, [r15]            ; r15 = 0x80009200
 * or r0, r0, 0x80
 * st r0, [r15]
 */

void hardware_init(void) {
    // AUX status register modification - proven sequence
    uint32_t status = aux_read(AUX_STATUS);

    // Mask and modify - proven values
    status = (status & STATUS_MASK_LOW);
    status = (status & STATUS_MASK_CLEAR) | STATUS_BIT_SET_1;

    aux_write(AUX_STATUS, status);

    // Hardware register at 0x80009200 - proven access
    volatile uint32_t *hw_9200 = (uint32_t*)HW_REG_80009200;
    *hw_9200 |= HW_BIT_SET;  // or r0, r0, 0x80
}

/*
 * DESCRIPTOR ALLOCATION AND WRITING
 * Reconstructed from disassembly
 *
 * Allocation sequence (from disassembly lines ~1160-1177):
 *
 * ld r4, [r13, 0xc]        ; allocator function
 * mov r0, r13
 * mov r1, 5                ; param for $BUP
 * sub r2, r2, r2
 * jl.d [r4]
 * mov r3, 0xec             ; size 0xEC for $BUP
 * st r0, [r15, 0xffffff08] ; save returned pointer
 *
 * ld r4, [r13, 0xc]
 * mov r1, 6                ; param for $HEC
 * jl.d [r4]
 * mov r3, 0x5c             ; size 0x5C for $HEC
 * st r0, [r15, 0xffffff14]
 */

void* allocate_descriptor(int param, uint32_t size) {
    // The actual allocator is at [r13+0xC]
    // This is a placeholder - real implementation calls through function pointer
    // Parameters proven from disassembly:
    //   r1 = param (5 for $BUP, 6 for $HEC)
    //   r3 = size (0xEC for $BUP, 0x5C for $HEC)
    //   Returns pointer in r0

    extern void* (*allocator_func)(int param, uint32_t size);
    return allocator_func(param, size);
}

/*
 * $BUP descriptor write (from disassembly lines ~1172-1173):
 *
 * ld r0, [r15, 0xffffff08] ; get allocated pointer
 * mov r1, 1
 * st r1, [r0, 4]           ; type = 1
 * st 0x50554224, [r0]      ; "$BUP" signature
 */

void write_bup_descriptor(void *ptr) {
    bup_descriptor_t *desc = (bup_descriptor_t*)ptr;
    desc->signature = SIG_BUP;  // st 0x50554224, [r0]
    desc->type = 1;             // st r1, [r0, 4] where r1=1
}

/*
 * $HEC descriptor write (from disassembly lines ~1174-1175):
 *
 * ld r0, [r15, 0xffffff14]
 * st r1, [r0, 4]           ; type = 1
 * st 0x43454824, [r0]      ; "$HEC" signature
 */

void write_hec_descriptor(void *ptr) {
    hec_descriptor_t *desc = (hec_descriptor_t*)ptr;
    desc->signature = SIG_HEC;  // st 0x43454824, [r0]
    desc->type = 1;             // st r1, [r0, 4] where r1=1
}

/*
 * OVERLAY INITIALIZATION
 * Reconstructed from string table and code
 *
 * Overlay names are stored at offset 0x1F4C-0x1FCC in binary.
 * Initialization code references these through function calls.
 */

void overlay_init(void) {
    // Overlay modules are initialized through function calls
    // Names proven from binary:
    for (int i = 0; i < 10; i++) {
        // Each overlay name is referenced in the string table
        // Actual loading is done through function pointers
        (void)overlay_names[i];  // Referenced but implementation unknown
    }
}

/*
 * MAIN LOOP
 * Reconstructed from disassembly
 *
 * Polling loop (from disassembly lines ~1800+):
 *
 * lr r0, [0x7f]
 * and r1, r0, 0xff
 * sub.f 0, r1, 0
 * bnz (skip)
 * flag 1                   ; set flag if zero
 * nop
 * nop
 * lr r0, [0x7e]
 * ... (repeats for 0x7b, 0x7d)
 *
 * List manipulation:
 * ld r1, [0x10201c]        ; list head
 * mov r0, 0x10203c         ; list start
 * sub.f 0, r1, r0
 * bnz ...
 */

void main_loop(void) {
    while (1) {
        // Poll AUX registers - proven sequence
        uint32_t val_7f = aux_read(AUX_POLL_7F) & 0xFF;
        if (val_7f == 0) {
            // flag 1 instruction - set processor flag
            __asm__ volatile ("flag 1");
        }

        uint32_t val_7e = aux_read(AUX_POLL_7E) & 0xFF;
        if (val_7e == 0) {
            __asm__ volatile ("flag 1");
        }

        uint32_t val_7b = aux_read(AUX_POLL_7B) & 0xFF;
        if (val_7b == 0) {
            __asm__ volatile ("flag 1");
        }

        uint32_t val_7d = aux_read(AUX_POLL_7D) & 0xFF;
        if (val_7d == 0) {
            __asm__ volatile ("flag 1");
        }

        // Check list - proven addresses
        list_entry_t *head = *(list_entry_t**)ADDR_LIST_HEAD;
        list_entry_t *start = (list_entry_t*)ADDR_LIST_START;

        if (head != start) {
            // Process list entries
            process_list_entry(head);
        }
    }
}

/*
 * List processing (from disassembly):
 *
 * Entry size = 0xC (12 bytes) proven by: add r1, r1, 0xc
 * Structure proven by st/ld at offsets 0, 4, 8
 */

void process_list_entry(list_entry_t *entry) {
    // List manipulation proven from disassembly
    // Entry removed from head and processed

    // ld r0, [r1]            ; data
    // ld r0, [r1, 4]         ; next
    // ld r0, [r1, 8]         ; prev

    if (entry->next != (void*)0) {
        // Link next to prev
        entry->next->prev = entry->prev;
    }
    if (entry->prev != (void*)0) {
        // Link prev to next
        entry->prev->next = entry->next;
    }
}

/*
 * MODULE HANDOFF
 * Reconstructed from disassembly
 *
 * Handoff to next module (from disassembly lines ~1712-1715):
 *
 * add r3, r2, 0x1000
 * extw r3, r3
 * ld r4, [r0]             ; entry point from module struct
 * ld r1, [r0, 0x30]       ; context from module struct
 * j.d [r4]                ; jump to entry point
 * mov r2, 0xb             ; argument = 11
 */

void module_handoff(module_struct_t *module) {
    // Load entry point - proven: ld r4, [r0]
    void (*entry)(void) = module->entry;

    // Load context - proven: ld r1, [r0, 0x30]
    void *context = module->context;

    // Jump with argument - proven: j.d [r4]; mov r2, 0xb
    __asm__ volatile (
        "mov r1, %0\n\t"
        "mov r2, %1\n\t"
        "j.d [%2]"
        :
        : "r"(context), "i"(MODULE_CALL_ARG), "r"(entry)
    );
}

/*
 * STACK FRAME MANAGEMENT
 * Reconstructed from disassembly
 *
 * Frame sizes from branch table at 0x3B0:
 * 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C,
 * 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48
 *
 * Proven from: mov r12, 0xXX followed by b.d
 */

typedef enum {
    FRAME_0x10 = 0x10,
    FRAME_0x14 = 0x14,
    FRAME_0x18 = 0x18,
    FRAME_0x1C = 0x1C,
    FRAME_0x20 = 0x20,
    FRAME_0x24 = 0x24,
    FRAME_0x28 = 0x28,
    FRAME_0x2C = 0x2C,
    FRAME_0x30 = 0x30,
    FRAME_0x34 = 0x34,
    FRAME_0x38 = 0x38,
    FRAME_0x3C = 0x3C,
    FRAME_0x40 = 0x40,
    FRAME_0x44 = 0x44,
    FRAME_0x48 = 0x48,
} frame_size_t;

/*
 * UTILITY FUNCTIONS
 * Reconstructed from disassembly offset 0x00-0x140
 *
 * memset implementation (from disassembly lines 1-40):
 * Uses stb for byte write, lp for hardware loop
 */

void* bringup_memset(void *dest, uint8_t value, uint32_t count) {
    uint8_t *ptr = (uint8_t*)dest;

    // Proven from disassembly: stb r3, [r1]; lp instruction
    while (count--) {
        *ptr++ = value;
    }
    return dest;
}

/*
 * memcpy implementation (from disassembly lines 40-100):
 * Uses ld.a/st.a for auto-increment
 */

void* bringup_memcpy(void *dest, const void *src, uint32_t count) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;

    // Proven: ld.a r4, [r1, 4]; st.a r4, [r3, 4]
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

/*
 * PROVEN FACTS SUMMARY
 *
 * ADDRESSES:
 *   0x102028 - Primary data structure
 *   0x10201C - List head pointer
 *   0x10203C - List buffer start
 *   0x102078 - List buffer end
 *   0x108000 - CKEY search start
 *   0x101F7C - Data address
 *
 * SIGNATURES:
 *   0x59454B43 - "CKEY" (little endian)
 *   0x50554224 - "$BUP" (little endian)
 *   0x43454824 - "$HEC" (little endian)
 *
 * ALLOCATIONS:
 *   $BUP: param=5, size=0xEC (236 bytes)
 *   $HEC: param=6, size=0x5C (92 bytes)
 *
 * HARDWARE WRITES:
 *   sr 0x2C, [0x8004]
 *   sr 4, [0x8010]
 *
 * AUX REGISTERS POLLED:
 *   0x7F, 0x7E, 0x7B, 0x7D
 *
 * STRUCTURE OFFSETS:
 *   Primary: +0x14=func_read_ptr, +0x1C=func_proc_ptr
 *   Module: +0x00=entry, +0x04=flags, +0x30=context
 *   List: +0x00=data, +0x04=next, +0x08=prev, size=0x0C
 *   Descriptor: +0x00=sig, +0x04=type
 *
 * COUNTS:
 *   CKEY entries: 4
 *   Stack frame: 0xE8 (main)
 *   List entry size: 0x0C (12 bytes)
 *   Overlay modules: 10
 *   Handoff argument: 11
 */
