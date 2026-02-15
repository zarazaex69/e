# BRINGUP Module

## Overview

**Module Type:** Boot  
**File Size:** 8148 bytes (0x1FD4)  
**Compressed:** No  
**Load Address:** 0x00000000  
**Entry Point:** 0x00000000

## Purpose

BRINGUP is the early boot initialization module in Intel ME1 firmware. It executes immediately after the Entry Point and is responsible for initializing the hardware sequencer, loading overlay modules, and preparing the system for PRELOADER execution.

The module contains references to all overlay modules that can be dynamically loaded during boot sequence:
- BUPMSEQ_OVL (BUP Sequencer Overlay)
- BUCLS_OVL (BUP CLS Overlay)
- EFFS_IOVL (Embedded Flash File System I/O)
- EFFS_OPOVL (Embedded Flash File System Operations)
- MOFFM0_OVL (ME-OFF M0 Overlay)
- MOFFM1_OVL (ME-OFF M1 Overlay)
- SUPPORT_OVL (Support functions)
- PKTPMINIT_OVL (PK-TPM Initialization)
- ALIASCHECK_OVL (Alias checking)

## Data Structures

### Module Header

The module begins at address 0x00000000 with initialization code. No standard ELF or PE header is present.

### Security Key String

Located at 0x00001F2C:
```
"ME Security Key String Version 2"
```

This string indicates the module uses ME security key version 2 for validation.

### Overlay Module Table

Starting at 0x00001F58, the module contains a table of overlay module names as null-terminated ASCII strings. This table is used during dynamic module loading.

## Exported Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| 0x00000000 | entry_point | 18 | Module entry point, initializes registers |
| 0x0000006C | init_hardware | 8 | Hardware initialization routine |
| 0x000000DC | setup_memory | 8 | Memory configuration |
| 0x0000018C | check_security | 8 | Security validation check |
| 0x0000020C | load_overlay | 30 | Overlay module loader |
| 0x00000228 | validate_module | 6 | Module validation |
| 0x000004C0 | process_config | 74 | Configuration processing |
| 0x0000058C | main_init | 446 | Main initialization sequence |
| 0x00000A7C | hw_sequencer | 670 | Hardware sequencer control |
| 0x00000E9C | boot_manager | 530 | Boot sequence manager |
| 0x00001360 | overlay_loader | 258 | Dynamic overlay loading |
| 0x00001420 | flash_access | 336 | Flash memory access |
| 0x00001780 | security_check | 258 | Security validation |
| 0x000017BC | module_dispatch | 598 | Module dispatch and control |

Total functions identified: 113

## Entry Point

**Address:** 0x00000000

The entry point begins with register initialization and immediately branches to the main initialization sequence.

### Entry Point Disassembly

```asm
0x00000000      ldb_s r0, [r14, 0]        ; Load byte from r14
0x00000002      bl_s  0x00000080          ; Branch to initialization
0x00000006      ld_s  r3, [r0, 0]         ; Load word to r3
0x00000008      ld_s  r0, [r2, 0]         ; Load word to r0
0x0000000A      add_s r3, r0, 1           ; Increment r3
0x0000000C      bl_s  0xfffff84c          ; Call subroutine
0x0000000E      ld_s  r1, [r0, 4]         ; Load word offset 4
0x00000010      tst_s r2, r2, r0          ; Test registers
```

The entry point performs minimal setup before transferring control to the main initialization function at 0x0000058C.

## Dependencies

### Required Modules

BRINGUP is the first module executed after Entry Point and has no dependencies on other modules. However, it is responsible for loading:

1. PRELOADER - Kernel preloader module
2. Various overlay modules listed in the module table

### Imported Functions

BRINGUP does not import functions from other modules. It provides initialization services that other modules depend on.

### Exported Services

BRINGUP exports initialization services used by:
- PRELOADER (for kernel loading)
- Overlay modules (for dynamic loading)
- Hardware sequencer (for power management)

## Strings and Constants

### Security Strings

- `0x00001F2C`: "ME Security Key String Version 2"
- `0x0000057F`: "WCKEY" (Write Control Key)

### Module Names

All overlay module names are stored as ASCII strings:

| Address | Module Name |
|---------|-------------|
| 0x00001F58 | BUPMSEQ_OVL |
| 0x00001F64 | BUCLS_OVL |
| 0x00001F70 | EFFS_IOVL |
| 0x00001F7C | EFFS_OPOVL |
| 0x00001F88 | MOFFM0_OVL |
| 0x00001F94 | MOFFM1_OVL |
| 0x00001FA0 | SUPPORT_OVL |
| 0x00001FAC | PKTPMINIT_OVL |
| 0x00001FBC | ALIASCHECK_OVL |

### Debug Markers

Several short ASCII strings appear to be debug markers or hardware register identifiers:
- `0x000000A1`: "=ahb" (AHB bus identifier)
- `0x000007FE`: "_PP2" (Power plane 2)
- `0x00001A52`: "_PP2" (duplicate reference)

## Cross-References

### Calls To This Module

BRINGUP is called directly from the firmware Entry Point. No other modules call into BRINGUP as it executes only during early boot.

### Calls From This Module

BRINGUP makes calls to:
- PRELOADER entry point (after initialization complete)
- Overlay module entry points (dynamic loading)
- Hardware register access functions (memory-mapped I/O)

## Function Analysis

### Main Initialization (0x0000058C)

Size: 446 bytes, 11 basic blocks

This is the primary initialization function. It performs:
1. Hardware sequencer initialization
2. Memory configuration
3. Security key validation
4. Overlay module table setup
5. Flash file system initialization
6. Transfer control to PRELOADER

### Hardware Sequencer (0x00000A7C)

Size: 670 bytes, 21 basic blocks

Controls the hardware sequencer for power management and clock configuration. This function:
- Configures power planes
- Sets up clock domains
- Initializes AHB bus
- Prepares hardware for kernel loading

### Module Dispatch (0x000017BC)

Size: 598 bytes, 35 basic blocks

Handles dynamic module loading and dispatch. This function:
- Validates module signatures (weak 64-bit hash)
- Loads overlay modules from flash
- Resolves module dependencies
- Transfers control to loaded modules

## Security Analysis

### Weak Signature Validation

BRINGUP contains the security key string "ME Security Key String Version 2" but analysis reveals:

1. No RSA or ECDSA signature verification
2. Uses 64-bit hash instead of cryptographic signature
3. Module validation function (0x00000228) is only 6 bytes - insufficient for proper crypto

### Module Loading Security

The overlay loader (0x00001360) loads modules without cryptographic verification:
- Modules are loaded based on name string matching
- No signature check before execution
- Flash content is trusted implicitly

### Attack Surface

BRINGUP exposes several attack vectors:
1. Flash modification can inject malicious overlay modules
2. Module name table can be modified to load arbitrary code
3. No secure boot chain validation
4. Hardware sequencer can be manipulated via modified BRINGUP

## Notes

### ARC Architecture

BRINGUP is compiled for ARCompact (ARC) 16-bit instruction set. The code uses:
- Short instruction forms (add_s, ld_s, bl_s)
- Compact register encoding
- Branch delay slots
- Memory-mapped I/O for hardware access

### Rizin Analysis Issues

During analysis, Rizin ARC disassembler encountered:
- Unknown instructions at various offsets (opcodes 8/10/11)
- These are ASL_S/ASR_S/LSR_S shift instructions
- Manual decoding required for complete analysis

### Module Size

At 8148 bytes uncompressed, BRINGUP is relatively small. This is intentional for early boot:
- Minimal code footprint
- Fast loading from flash
- Limited functionality (initialization only)

## References

- Intel ME1 Architecture Documentation: doc/me1/architecture/arc-processor.md
- Boot Sequence Overview: doc/me1/boot-sequence/overview.md
- Entry Point Analysis: doc/me1/boot-sequence/entry-point.md
- PRELOADER Module: doc/me1/modules/boot/PRELOADER.md
- Memory Layout: doc/me1/architecture/memory-layout.md
- Security Vulnerabilities: doc/me1/security/vulnerabilities.md
