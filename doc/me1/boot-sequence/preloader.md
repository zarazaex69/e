# PRELOADER Boot Sequence

## Overview

This document describes the PRELOADER module's role in the Intel ME1 boot sequence, focusing on the kernel loading process and the transition from early boot (BRINGUP) to the operating system kernel (KernelPriv + KernelNonPriv).

## Position in Boot Chain

```
ROM → Entry Point (0x720A0) → BRINGUP → PRELOADER → KernelPriv + KernelNonPriv → Services
```

PRELOADER is the third stage in the boot chain:
- **Previous stage**: BRINGUP (hardware initialization)
- **Current stage**: PRELOADER (kernel loading)
- **Next stage**: KernelPriv (operating system kernel)

## Execution Timeline

| Time | Event | Module | Action |
|------|-------|--------|--------|
| 0ms | Reset | ROM | Hardware reset, ROM execution |
| 2ms | Entry | Entry Point | Parse CODE partition, initialize |
| 5ms | Init | BRINGUP | Hardware sequencer, memory setup |
| 10ms | Load | PRELOADER | Start kernel loading |
| 15ms | Decompress | PRELOADER | LZMA decompress KernelPriv |
| 35ms | Decompress | PRELOADER | LZMA decompress KernelNonPriv |
| 55ms | Setup | PRELOADER | Privilege separation setup |
| 60ms | Transfer | PRELOADER | Jump to KernelPriv entry point |
| 60ms+ | Run | KernelPriv | Kernel initialization begins |

Total PRELOADER execution time: approximately 50ms (dominated by LZMA decompression)

## Control Transfer from BRINGUP

### BRINGUP Exit

BRINGUP completes its initialization and prepares to call PRELOADER:

1. Hardware sequencer configured
2. Memory regions allocated
3. Flash file system initialized
4. Module table prepared

BRINGUP sets up registers for PRELOADER:
- r12: PRELOADER entry point address
- r0: Module metadata pointer
- r1: Memory allocation table
- r2: Hardware status flags

### PRELOADER Entry

PRELOADER entry point at 0x00000000:

```
0x00000000      j_s   [r12]
```

This indirect jump transfers control based on r12 value set by BRINGUP. The actual entry logic begins at the address stored in r12.

## Kernel Loading Sequence

### Step 1: Module Discovery

PRELOADER parses the $MAN manifest to locate kernel modules.

**Function**: parse_manifest (0x000003D8)

Actions:
1. Read CODE partition header
2. Parse $MAN manifest structure
3. Search for module type 0x2B (KernelPriv)
4. Search for module type 0x21 (KernelNonPriv)
5. Extract module offsets and sizes
6. Validate module headers

**Manifest Structure**:
```
Offset  Field           Description
+0x00   Magic           "$MAN" signature
+0x04   Version         Manifest version
+0x08   ModuleCount     Number of modules
+0x0C   ModuleTable     Array of module entries
```

**Module Entry**:
```
Offset  Field           Description
+0x00   Type            Module type (0x2B, 0x21, etc.)
+0x04   Offset          Offset in CODE partition
+0x08   Size            Compressed size
+0x0C   UncompSize      Uncompressed size
+0x10   Flags           Compression flags (LZMA bit)
```

### Step 2: Memory Allocation

PRELOADER allocates memory for decompression.

**Function**: memory_alloc (0x000001AC)

Actions:
1. Calculate required memory for KernelPriv (~500KB uncompressed)
2. Calculate required memory for KernelNonPriv (~520KB uncompressed)
3. Allocate privileged memory region
4. Allocate non-privileged memory region
5. Setup memory protection boundaries

**Memory Layout**:
```
0x00000000 - 0x0007FFFF   Privileged Region (KernelPriv)
0x00080000 - 0x000FFFFF   Non-Privileged Region (KernelNonPriv)
0x00100000 - 0x001FFFFF   Shared Memory
0x00200000+               Heap and Stack
```

### Step 3: LZMA Decompression

PRELOADER decompresses kernel modules using LZMA algorithm.

**Function**: lzma_decompress (0x0000030C)

**KernelPriv Decompression**:
1. Read compressed data from flash (240KB)
2. Initialize LZMA decoder state
3. Setup dictionary (1MB max)
4. Decompress to privileged memory region
5. Verify decompressed size matches manifest
6. Calculate checksum (weak 64-bit hash)

**KernelNonPriv Decompression**:
1. Read compressed data from flash (250KB)
2. Reuse LZMA decoder state
3. Decompress to non-privileged memory region
4. Verify decompressed size
5. Calculate checksum

**LZMA Parameters**:
- Algorithm: LZMA1 (not LZMA2)
- Dictionary size: 1MB
- Compression level: Maximum (used during firmware build)
- Decompression speed: ~10MB/s on ARC processor

### Step 4: Module Validation

PRELOADER validates decompressed kernel modules.

**Function**: verify_signature (0x000003D0)

Actions:
1. Calculate 64-bit hash of KernelPriv
2. Compare with hash in manifest
3. Calculate 64-bit hash of KernelNonPriv
4. Compare with hash in manifest
5. Check for corruption

**Security Note**: This is NOT cryptographic verification. The 64-bit hash can be easily forged. No RSA or ECDSA signature verification is performed.

### Step 5: Privilege Separation Setup

PRELOADER configures privilege separation between kernel components.

**Function**: privilege_setup (0x00000570)

Actions:
1. Configure ARC privilege levels (if supported)
2. Setup memory protection unit (MPU) boundaries
3. Mark privileged region as supervisor-only
4. Mark non-privileged region as user-accessible
5. Configure privilege transition mechanism
6. Setup system call interface

**Privilege Levels**:
- Level 0 (Supervisor): KernelPriv, hardware access
- Level 1 (User): KernelNonPriv, services, applications

### Step 6: Kernel Bootstrap

PRELOADER prepares kernel for execution.

**Function**: kernel_bootstrap (0x000004F4)

Actions:
1. Initialize kernel data structures
2. Setup interrupt vector table
3. Configure timer for scheduler
4. Prepare task control blocks
5. Initialize kernel heap
6. Setup initial stack pointers
7. Prepare kernel parameters

**Kernel Parameters**:
- r0: Hardware configuration pointer
- r1: Memory map pointer
- r2: Module table pointer
- r3: Boot flags

### Step 7: Control Transfer

PRELOADER transfers control to KernelPriv.

**Function**: transfer_control (0x000001F0)

Actions:
1. Validate KernelPriv entry point
2. Setup initial kernel stack
3. Disable PRELOADER memory regions
4. Clear sensitive data from PRELOADER
5. Jump to KernelPriv entry point

**Final Jump**:
```
0x000001F0      ld_s  r12, [kernel_entry]
0x000001F2      j_s   [r12]
```

KernelPriv entry point is typically at offset 0x00000000 in the privileged memory region.

## Error Handling

### Module Not Found

If PRELOADER cannot find kernel modules in manifest:
1. Call error_handler (0x00000290)
2. Set error code: MODULE_NOT_FOUND
3. Halt system (no recovery possible)

### Decompression Failure

If LZMA decompression fails:
1. Call error_handler (0x00000290)
2. Set error code: DECOMPRESS_ERROR
3. Retry decompression once
4. If retry fails, halt system

### Validation Failure

If module hash validation fails:
1. Call error_handler (0x00000290)
2. Set error code: VALIDATION_ERROR
3. Halt system (corrupted firmware)

### Memory Allocation Failure

If memory allocation fails:
1. Call error_handler (0x00000290)
2. Set error code: OUT_OF_MEMORY
3. Halt system (insufficient RAM)

## Memory Usage

### PRELOADER Memory Footprint

- Code: 4,288 bytes (0x10C0)
- Stack: ~1KB
- LZMA decoder state: ~32KB
- Decompression buffer: ~1MB (reused for both kernels)
- Total: ~1.04MB during execution

### Kernel Memory Requirements

- KernelPriv: ~500KB uncompressed
- KernelNonPriv: ~520KB uncompressed
- Shared structures: ~128KB
- Total: ~1.15MB for kernel

### Total Boot Memory

Peak memory usage during PRELOADER execution: ~2.2MB

After control transfer to kernel, PRELOADER memory is freed.

## Performance Characteristics

### Decompression Speed

LZMA decompression on ARC processor:
- Speed: ~10MB/s
- KernelPriv (500KB): ~50ms
- KernelNonPriv (520KB): ~52ms
- Total decompression time: ~102ms

### Actual Timing

Measured timing (from boot_sequence.txt):
- KernelPriv decompression: ~20ms (faster than theoretical)
- KernelNonPriv decompression: ~20ms
- Total PRELOADER execution: ~50ms

The faster actual timing suggests:
- Optimized LZMA implementation
- Cache effects
- Parallel I/O and decompression

## Security Implications

### No Secure Boot

PRELOADER does not implement secure boot:
- No cryptographic signature verification
- 64-bit hash is easily forged
- Kernel can be replaced in flash without detection
- No attestation of loaded code

### Attack Vectors

1. **Flash Modification**: Replace KernelPriv/KernelNonPriv in flash
2. **Manifest Tampering**: Modify module offsets to load arbitrary code
3. **LZMA Bomb**: Craft malicious compressed data to exhaust memory
4. **Privilege Escalation**: Exploit weak privilege separation

### Mitigation

Current firmware has NO mitigation for these attacks. Secure boot would require:
- RSA-2048 or ECDSA-256 signature verification
- Root of trust in ROM
- Chain of trust validation
- Rollback protection

## Comparison with Modern Boot Loaders

### UEFI Secure Boot

Modern UEFI implements:
- RSA-2048 signature verification
- Certificate chain validation
- Revocation lists
- Measured boot with TPM

ME1 PRELOADER has NONE of these features.

### ARM Trusted Firmware

ARM TF-A implements:
- Chain of trust from ROM
- BL1 → BL2 → BL31 → BL33 stages
- Cryptographic verification at each stage
- Secure world isolation

ME1 PRELOADER has minimal privilege separation, no chain of trust.

## Debugging and Analysis

### Rizin Analysis

To analyze PRELOADER with Rizin:

```bash
rizin -a arc reverse/me1/modules/PRELOADER.bin
aaa
afl
s 0x00000724
pdf
```

Note: Do NOT use -b flag (causes Rizin crash with ARC)

### Key Functions to Analyze

1. 0x00000724 - main_loader: Main loading orchestrator
2. 0x0000030C - lzma_decompress: LZMA decompression
3. 0x000004F4 - kernel_bootstrap: Kernel preparation
4. 0x00000570 - privilege_setup: Privilege separation
5. 0x000001F0 - transfer_control: Final control transfer

### Unknown Instructions

Rizin reports unknown instructions (opcodes 8/10/11):
- These are ASL_S/ASR_S/LSR_S shift instructions
- Manual decoding required
- See: reverse/me1/doc/note/rizin_arc_unknown_instructions.txt

## References

- PRELOADER Module Documentation: doc/me1/modules/boot/PRELOADER.md
- BRINGUP Module: doc/me1/modules/boot/BRINGUP.md
- KernelPriv Module: doc/me1/modules/kernel/KernelPriv.md
- KernelNonPriv Module: doc/me1/modules/kernel/KernelNonPriv.md
- Boot Sequence Overview: doc/me1/boot-sequence/overview.md
- Entry Point Analysis: doc/me1/boot-sequence/entry-point.md
- Memory Layout: doc/me1/architecture/memory-layout.md
- Security Vulnerabilities: doc/me1/security/vulnerabilities.md
- Original Notes: reverse/me1/doc/note/boot_sequence.txt
