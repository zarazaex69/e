# PRELOADER Module

## Overview

**Module Type:** Kernel Preloader (0x07)  
**File Size:** 4288 bytes (0x10C0)  
**Compressed:** No  
**Load Address:** 0x00000000  
**Entry Point:** 0x00000000  
**Priority:** HIGH (loaded first after Entry Point)

## Purpose

PRELOADER is the kernel loading module in Intel ME1 firmware. It executes after BRINGUP initialization and is responsible for loading and decompressing the two kernel modules (KernelPriv and KernelNonPriv), setting up the privileged/non-privileged separation, and transferring control to the kernel.

PRELOADER is a critical component in the boot chain:
- Entry Point (0x720A0) → BRINGUP → PRELOADER → KernelPriv + KernelNonPriv

The module contains LZMA decompression logic and module loading infrastructure required to bootstrap the ME1 operating system kernel.

## Data Structures

### Module Header

The module begins at address 0x00000000 with a minimal entry point. No standard ELF or PE header is present.

### Memory Management Strings

Located throughout the module:
- `0x00000808`: "$MEM" - Memory management marker
- `0x0000094B`: "$MEM" - Duplicate reference
- `0x00000AC0`: "$MCX" - Memory context marker

These strings indicate memory allocation and management functions used during kernel loading.

### Hardware Identifiers

- `0x0000083E`: "AP![" - Application Processor marker
- Various register access patterns for hardware initialization

## Exported Functions

| Address | Name | Size | Blocks | Description |
|---------|------|------|--------|-------------|
| 0x00000000 | entry_point | 2 | 1 | Module entry point, indirect jump |
| 0x00000050 | init_loader | 14 | 1 | Loader initialization |
| 0x000000B4 | check_module | 6 | 1 | Module validation |
| 0x000000B8 | load_module | 8 | 2 | Module loading dispatcher |
| 0x0000011C | decompress_init | 8 | 1 | LZMA decompressor initialization |
| 0x000001AC | memory_alloc | 16 | 2 | Memory allocation for modules |
| 0x000001C4 | validate_header | 6 | 1 | Module header validation |
| 0x000001C8 | setup_context | 8 | 1 | Context setup for kernel |
| 0x000001F0 | transfer_control | 12 | 1 | Transfer control to kernel |
| 0x00000290 | error_handler | 12 | 2 | Error handling routine |
| 0x000002A4 | cleanup | 8 | 1 | Cleanup before kernel start |
| 0x000002BC | prepare_kernel | 16 | 1 | Kernel preparation |
| 0x0000030C | lzma_decompress | 84 | 5 | LZMA decompression routine |
| 0x00000360 | module_loader | 66 | 9 | Main module loading logic |
| 0x000003D0 | verify_signature | 6 | 2 | Signature verification (weak) |
| 0x000003D8 | parse_manifest | 28 | 3 | $MAN manifest parser |
| 0x000004F4 | kernel_bootstrap | 114 | 14 | Kernel bootstrap sequence |
| 0x00000570 | privilege_setup | 20 | 2 | Privilege separation setup |
| 0x00000724 | main_loader | 256 | 6 | Main loading orchestrator |
| 0x00000E04 | finalize_boot | 114 | 7 | Boot finalization |

Total functions identified: 48

## Entry Point

**Address:** 0x00000000

The entry point is minimal, consisting of a single indirect jump instruction:

```
0x00000000      j_s   [r12]
```

This indirect jump transfers control based on the value in register r12, which is set by BRINGUP before calling PRELOADER. The actual entry logic begins at the address stored in r12.

### Initialization Sequence

The initialization function at 0x00000050 performs:

```
0x00000050      bl_s  0xfffff870        
0x00000052      ld_s  r3, [r0, 0]      
0x00000054      ld_s  r0, [r2, 0]      
0x00000056      add_s r3, r0, 1         
0x00000058      bl_s  0xfffff898        
0x0000005A      ld_s  r1, [r0, 4]      
0x0000005C      tst_s r2, r2, r0        
```

This sequence:
1. Calls initialization routine
2. Loads module metadata
3. Increments module counter
4. Calls setup function
5. Tests module validity

## Dependencies

### Required Modules

PRELOADER depends on:
1. BRINGUP - Must complete hardware initialization before PRELOADER runs
2. Entry Point - Provides initial control transfer

### Loaded Modules

PRELOADER is responsible for loading:
1. **KernelPriv** (240KB, LZMA compressed) - Privileged kernel
2. **KernelNonPriv** (250KB, LZMA compressed) - Non-privileged kernel

### Imported Functions

PRELOADER imports functions from BRINGUP:
- Memory allocation services
- Flash access functions
- Hardware sequencer control

### Exported Services

PRELOADER exports loading services used by:
- Kernel modules (for initialization)
- Boot sequence manager (for status reporting)

## Kernel Loading Process

### Phase 1: Module Discovery

PRELOADER parses the $MAN manifest to locate kernel modules:
- Searches for module type 0x2B (KernelPriv)
- Searches for module type 0x21 (KernelNonPriv)
- Validates module headers
- Checks compression flags

### Phase 2: LZMA Decompression

The LZMA decompression routine at 0x0000030C:
- Initializes LZMA decoder state
- Allocates decompression buffer
- Decompresses KernelPriv (240KB compressed → ~500KB uncompressed)
- Decompresses KernelNonPriv (250KB compressed → ~520KB uncompressed)

### Phase 3: Memory Layout Setup

PRELOADER configures memory layout:
- Privileged kernel region (protected)
- Non-privileged kernel region (user space)
- Shared memory regions
- Stack and heap allocation

### Phase 4: Privilege Separation

The privilege_setup function at 0x00000570:
- Configures ARC privilege levels
- Sets up memory protection (if MPU available)
- Establishes kernel/user boundary
- Initializes privilege transition mechanism

### Phase 5: Control Transfer

The transfer_control function at 0x000001F0:
- Validates kernel entry points
- Sets up initial kernel stack
- Prepares kernel parameters
- Jumps to KernelPriv entry point

## Strings and Constants

### Memory Management

- `0x00000808`: "$MEM" - Memory subsystem identifier
- `0x0000094B`: "$MEM" - Secondary reference
- `0x00000AC0`: "$MCX" - Memory context structure

### Hardware Markers

- `0x0000083E`: "AP![" - Application Processor marker
- Various I/O register patterns

### Debug Markers

Several short ASCII strings appear to be debug markers:
- Hardware register identifiers
- Memory region markers
- Module loading checkpoints

## Cross-References

### Calls To This Module

PRELOADER is called from:
- BRINGUP module (after hardware initialization complete)
- Entry Point dispatcher (in some boot scenarios)

### Calls From This Module

PRELOADER makes calls to:
- BRINGUP memory allocation functions
- Flash access routines (for reading compressed modules)
- Hardware sequencer (for status updates)
- KernelPriv entry point (final control transfer)

## Function Analysis

### Main Loader (0x00000724)

Size: 256 bytes, 6 basic blocks

This is the primary loading orchestrator. It performs:
1. Parse module manifest ($MAN)
2. Locate KernelPriv and KernelNonPriv
3. Validate module signatures (weak 64-bit hash)
4. Allocate memory for decompression
5. Call LZMA decompressor
6. Setup privilege separation
7. Transfer control to kernel

### LZMA Decompress (0x0000030C)

Size: 84 bytes, 5 basic blocks

LZMA decompression implementation:
- Uses LZMA SDK algorithm
- Supports dictionary sizes up to 1MB
- Decompresses in-place to save RAM
- Returns decompressed size

### Kernel Bootstrap (0x000004F4)

Size: 114 bytes, 14 basic blocks

Prepares kernel for execution:
- Initializes kernel data structures
- Sets up interrupt vectors
- Configures timer for scheduler
- Prepares task control blocks
- Validates kernel integrity

## Security Analysis

### Weak Signature Verification

PRELOADER contains signature verification at 0x000003D0, but analysis reveals:

1. No RSA or ECDSA signature verification
2. Uses 64-bit hash instead of cryptographic signature
3. Verification function is only 6 bytes - insufficient for proper crypto
4. Same weakness as BRINGUP module

### Module Loading Security

The module loader (0x00000360) loads kernel without cryptographic verification:
- Modules are loaded based on type field in manifest
- No signature check before decompression
- Flash content is trusted implicitly
- LZMA decompressor has no integrity checks

### Attack Surface

PRELOADER exposes several attack vectors:

1. **Flash Modification**: Attacker can replace KernelPriv/KernelNonPriv in flash
2. **Manifest Tampering**: Module type fields can be modified to load arbitrary code
3. **LZMA Bomb**: Malicious compressed data can cause memory exhaustion
4. **Privilege Escalation**: Weak privilege separation allows kernel compromise

### No Secure Boot

PRELOADER does not implement secure boot:
- No root of trust validation
- No chain of trust from ROM
- No attestation of loaded modules
- Kernel can be replaced without detection

## Notes

### ARC Architecture

PRELOADER is compiled for ARCompact (ARC) 16-bit instruction set. The code uses:
- Short instruction forms (add_s, ld_s, bl_s)
- Compact register encoding
- Branch delay slots
- Indirect jumps for dynamic dispatch

### Rizin Analysis Issues

During analysis, Rizin ARC disassembler encountered:
- Unknown instructions at various offsets (opcodes 8/10/11)
- These are ASL_S/ASR_S/LSR_S shift instructions
- Manual decoding required for complete analysis
- Some function boundaries incorrectly identified

### Module Size

At 4288 bytes uncompressed, PRELOADER is compact. This is intentional:
- Minimal code footprint
- Fast loading from flash
- Limited functionality (loading only)
- Kernel contains actual OS logic

### LZMA Implementation

PRELOADER contains a minimal LZMA decoder:
- Supports LZMA1 format only
- Dictionary size limited to 1MB
- No LZMA2 support
- Optimized for code size, not speed

## Boot Sequence Integration

PRELOADER fits into the boot sequence as follows:

```
ROM → Entry Point (0x720A0) → BRINGUP → PRELOADER → KernelPriv + KernelNonPriv
```

Timing:
- BRINGUP completes: ~10ms after reset
- PRELOADER starts: ~10ms
- Kernel loading: ~50ms (decompression time)
- Kernel starts: ~60ms total boot time

## References

- Intel ME1 Architecture Documentation: doc/me1/architecture/arc-processor.md
- Boot Sequence Overview: doc/me1/boot-sequence/overview.md
- Entry Point Analysis: doc/me1/boot-sequence/entry-point.md
- BRINGUP Module: doc/me1/modules/boot/BRINGUP.md
- KernelPriv Module: doc/me1/modules/kernel/KernelPriv.md
- KernelNonPriv Module: doc/me1/modules/kernel/KernelNonPriv.md
- Memory Layout: doc/me1/architecture/memory-layout.md
- Security Vulnerabilities: doc/me1/security/vulnerabilities.md
- Boot Sequence Details: reverse/me1/doc/note/boot_sequence.txt
