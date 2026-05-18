/*
 * RECONSTRUCTED LOGIC FOR INTEL ME v1 PRELOADER
 * Based on static analysis of PRELOADER.bin (ARC Architecture, 16-bit instructions)
 * 
 * Target Hardware: ARC Core (ARCompact ISA)
 * Module Base: 0x01000000 (Flash Mapped)
 * Entry Point: 0x01012210 (Virtual) -> 0x0000 (Physical Offset)
 * 
 * SUMMARY:
 * 1. Setup Stack (SP) and Global Pointer (GP).
 * 2. Initialize Memory Controller (SRAM Banks) via polling loop.
 * 3. Decompress next stage (BRINGUP) from Flash to SRAM.
 * 4. Write signature "$MEM" to SRAM start.
 * 5. Jump to BRINGUP entry point (0x00100000).
 */

#include <stdint.h>

// ARC Special Registers (Auxiliary Space)
#define AUX_SRAM_CTRL_BASE  0x2C00
#define AUX_MMU_CTRL        0x10005

// Global Pointers (Located at end of binary)
#define GP_VAL              0x010010fc
#define SP_VAL              0x01012210

// Next Stage Configuration
#define TARGET_SRAM_ADDR    0x00100000 // Where BRINGUP is loaded
#define MAGIC_SIGNATURE     0x4D454D24 // "$MEM" in Little Endian

// --- Helper Functions (Assembly Wrappers) ---
uint32_t aux_read(uint32_t reg);   // lr r0, [reg]
void aux_write(uint32_t reg, uint32_t val); // sr val, [reg]
void jump_to(uint32_t addr);       // jl [addr]

// --- Main Entry Point ---
void _start() {
    // 1. CRT0 Initialization
    // Set Stack Pointer to top of module to avoid overwriting code
    // asm: mov sp, 0x1012210
    uint32_t *sp = (uint32_t*)SP_VAL;
    
    // Set Global Pointer to data section
    // asm: mov gp, 0x10010fc
    uint32_t *gp = (uint32_t*)GP_VAL;

    // 2. Hardware Initialization Phase
    // Critical for enabling SRAM before unpacking code
    init_memory_controller();

    // 3. Unpacking Phase
    // Decompress BRINGUP from Flash to SRAM
    if (!unpack_next_stage()) {
        // Unpacking failed -> Halt or Reset
        while(1);
    }

    // 4. Handoff Phase
    // Enable features in MMU/Cache controller
    uint32_t ctrl = aux_read(AUX_MMU_CTRL);
    aux_write(AUX_MMU_CTRL, ctrl | 0x1000); // Enable bit 12 (Cache/Map?)

    // Jump to the entry point of BRINGUP (0x00100000)
    // The address is read from the global data table
    uint32_t *target_ptr = (uint32_t*)0x1001100;
    jump_to(*target_ptr);
}

// --- Memory Controller Initialization ---
// Corresponds to function at offset 0x0bb0
void init_memory_controller() {
    uint32_t loop_count = 0x10; // Check 16 banks/registers
    uint32_t current_aux = AUX_SRAM_CTRL_BASE;

    do {
        // Read status from pair of registers
        uint32_t status_low = aux_read(current_aux);     // e.g., 0x2C00
        uint32_t status_high = aux_read(current_aux + 1); // e.g., 0x2C01

        // Check if bank is ready (Bitwise logic reconstructed from asm)
        // asm: sub.f 0, r4, r1 -> bnc loop
        // If (status_low & MASK) < (status_high + OFFSET), wait.
        if (!is_bank_ready(status_low, status_high)) {
            continue; // Spin wait
        }

        // Move to next bank pair
        current_aux += 2;
        loop_count--;
    } while (loop_count > 0);
}

// --- Decompression Logic ---
// Corresponds to function at offset 0x0c78 (Main) and 0x0654 (Algo)
int unpack_next_stage() {
    // Setup decompression parameters from Global Data
    // Source: Flash Address (implicit)
    // Dest: SRAM Address (0x00100000)
    
    // Call custom LZ/Huffman decompressor
    // asm: bl 0x0970
    int size = decompress_lz();

    if (size > 0) {
        // Success! Mark memory as initialized
        // Write "$MEM" signature to start of SRAM status area
        // asm: st 0x4d454d24, [r0]
        uint32_t *mem_marker = (uint32_t*)0x00100000; // Simplified address
        *mem_marker = MAGIC_SIGNATURE;
        return 1;
    }
    return 0;
}
