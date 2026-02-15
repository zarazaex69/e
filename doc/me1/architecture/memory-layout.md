# Memory Layout

## Overview

Intel Management Engine version 1.x operates in a 32-bit address space with distinct regions for firmware code, data, stack, and hardware peripherals. The ME1 subsystem runs independently of the main x86 processor with its own memory map and direct access to platform hardware through the ICH9M chipset.

## Address Space Organization

ME1 uses a flat 32-bit address space divided into three primary regions:

```
0x00000000 - 0x0FFFFFFF   Reserved/Unmapped (256 MB)
0x10000000 - 0x1FFFFFFF   Firmware Region (256 MB)
0x20000000 - 0xFFFFFFFF   Hardware Peripherals (3.5 GB)
```

### Reserved Region (0x00000000 - 0x0FFFFFFF)

This region is not mapped to physical memory or peripherals. Access to addresses in this range triggers hardware exceptions. The region serves as a guard zone to catch null pointer dereferences and invalid memory accesses.

### Firmware Region (0x10000000 - 0x1FFFFFFF)

The firmware region contains all executable code, read-only data, read-write data, heap, and stack for ME1 modules. This 256 MB region is subdivided into module-specific areas with fixed load addresses.

Typical layout within firmware region:

```
0x10000000 - 0x100FFFFF   Module code and data (1 MB typical)
0x10100000 - 0x101FFFFF   Additional module space
0x10200000 - 0x102FFFFF   Stack region (grows downward)
0x10300000 - 0x1FFFFFFF   Extended module space and heap
```

Module load addresses are determined at firmware build time and remain fixed across boots. Each module receives a dedicated address range that does not overlap with other modules.

### Hardware Peripheral Region (0x20000000 - 0xFFFFFFFF)

This region maps hardware registers and peripheral devices. ME1 firmware accesses platform hardware through memory-mapped I/O in this space. Specific peripheral addresses depend on ICH9M chipset configuration.

## Module Memory Layout

Each ME1 module follows a standard internal memory layout:

```
+------------------+  <- Module base address (e.g., 0x10000000)
| Module Header    |
| - Magic ($MAN)   |
| - Entry offset   |
| - Metadata       |
+------------------+
| Code Section     |
| - Entry point    |
| - Functions      |
| - Jump tables    |
+------------------+
| Read-Only Data   |
| - String literals|
| - Constants      |
| - Function ptrs  |
+------------------+
| Read-Write Data  |
| - Global vars    |
| - Static data    |
+------------------+
| BSS Section      |
| - Uninitialized  |
+------------------+
```

### Module Header

Every module begins with a header containing metadata and the entry point offset. The header structure:

```
Offset  Size  Description
------  ----  -----------
0x00    4     Magic signature ($MAN)
0x04    4     Entry point offset (relative to header start)
0x08    4     Module size
0x0C    4     Build timestamp
0x10    ...   Additional metadata
```

The entry point offset at header+0x04 specifies where execution begins relative to the module base. For example, if the offset is 0xA0, execution starts at module_base + 0xA0.

### Code Section

The code section immediately follows the header and contains all executable instructions. Functions are laid out sequentially with no padding between them unless required for alignment.

Function prologues use the standard ME1 pattern:

```
sub   sp, sp, 4
mov   r12, frame_size
b.d   function_body
sub   sp, sp, r12
```

This pattern (hex: 047e8e53 at the first instruction) serves as a reliable function boundary marker for analysis tools.

### Data Sections

Read-only data follows the code section and contains string literals, constant tables, and function pointer arrays. This section is not writable at runtime.

Read-write data contains global variables and static data that can be modified during execution. The BSS section holds uninitialized global variables that are zeroed at module initialization.

## Stack Organization

ME1 uses a descending stack model where the stack grows from high addresses toward low addresses. Each module receives its own stack region with a fixed base address.

Stack layout for a typical function call:

```
High Address
+------------------+  <- Stack base (sp at module entry)
| Caller's frame   |
+------------------+
| Return address   |  <- Saved blink register
+------------------+
| Saved fp         |  <- Saved frame pointer
+------------------+
| Local variables  |
+------------------+
| Spilled registers|
+------------------+
| Function args    |  <- Arguments beyond r0-r7
+------------------+  <- Current sp
Low Address
```

### Stack Pointer Initialization

Modules initialize the stack pointer at entry:

```
mov   sp, 0x1012210      Set stack base address
```

Stack base addresses are module-specific. Analysis of ME1 modules shows stack bases in the range 0x10100000 - 0x10300000.

### Stack Frame Allocation

Functions allocate stack frames using the two-stage pattern:

```
sub   sp, sp, 4          Reserve 4 bytes
mov   r12, frame_size    Load frame size
b.d   function_body      Branch with delay slot
sub   sp, sp, r12        Allocate frame (executes before branch)
```

This pattern allows the second subtraction to execute in the branch delay slot, improving pipeline efficiency.

### Stack Frame Deallocation

Functions deallocate frames on return:

```
ld    blink, [sp, offset]   Restore return address
ld    fp, [sp, offset]      Restore frame pointer
add   sp, sp, frame_size    Deallocate frame
j     [blink]               Return to caller
```

## Global Pointer (gp)

The global pointer register (r26/gp) holds the base address of the module's data segment. This allows efficient access to global variables using small offsets:

```
mov   gp, 0x10010fc      Initialize global pointer
ld    r0, [gp, offset]   Load global variable
```

The global pointer is initialized once at module entry and remains constant throughout execution. All global variable accesses use gp-relative addressing.

## Frame Pointer (fp)

The frame pointer register (r27/fp) points to the current function's stack frame. It provides a stable reference for accessing local variables and function parameters when the stack pointer changes during execution.

Frame pointer setup:

```
st    fp, [sp, offset]   Save previous frame pointer
mov   fp, sp             Set frame pointer to current stack
```

Local variables are accessed via fp-relative offsets:

```
ld    r0, [fp, -8]       Load local variable at fp-8
st    r1, [fp, -12]      Store to local variable at fp-12
```

## Heap Management

ME1 modules use dynamic memory allocation for runtime data structures. The heap region is located above the static data sections and grows upward toward the stack.

Heap allocation is managed by module-specific allocator functions. Analysis of kernel modules shows standard malloc/free patterns with metadata headers preceding allocated blocks.

## Memory Protection

ME1 firmware operates without virtual memory or memory protection units. All addresses are physical, and all code runs in a single privilege level with unrestricted memory access.

The lack of memory protection means:
- No page tables or address translation
- No access control between modules
- No protection against buffer overflows
- Direct hardware access from any code

This design reflects the embedded nature of ME1 and the assumption that all firmware code is trusted.

## Inter-Module Memory Sharing

Modules communicate through shared memory regions and function pointer tables. The PRELOADER module establishes these shared regions during boot and passes pointers to loaded modules.

Shared data structures use fixed addresses known to all modules. For example, a dispatch table at a fixed address contains function pointers for inter-module calls.

## Memory Access Patterns

### Aligned Access

ARCompact requires natural alignment for multi-byte accesses:
- Word (32-bit) loads/stores must be 4-byte aligned
- Halfword (16-bit) loads/stores must be 2-byte aligned
- Byte loads/stores have no alignment requirement

Unaligned access triggers hardware exceptions. Compiler-generated code ensures proper alignment through padding and address calculation.

### Auto-Increment Addressing

ME1 code extensively uses auto-increment addressing for memory operations:

```
ld.a  r0, [r1, 4]        Load word, then r1 += 4
st.a  r0, [r1, 4]        Store word, then r1 += 4
```

This mode eliminates separate pointer increment instructions in loops, improving code density and performance.

### Memory Copy Optimization

Optimized memory copy routines use word-sized transfers with auto-increment:

```
ld.a  r4, [r1, 4]        Load 4 words with auto-increment
ld.a  r5, [r1, 4]
ld.a  r6, [r1, 4]
ld.a  r7, [r1, 4]
st.a  r4, [r3, 4]        Store 4 words with auto-increment
st.a  r5, [r3, 4]
st.a  r6, [r3, 4]
st.a  r7, [r3, 4]
```

This pattern transfers 16 bytes per loop iteration, achieving maximum memory bandwidth.

## Endianness

ME1 uses little-endian byte ordering. Multi-byte values store the least significant byte at the lowest address.

Example: The 32-bit value 0x12345678 is stored in memory as:

```
Address   Value
--------  -----
0x1000    0x78
0x1001    0x56
0x1002    0x34
0x1003    0x12
```

## Cache and Memory Ordering

ARCompact processors in ME1 may include instruction and data caches, but specific cache configuration is not visible in firmware code. Memory operations appear to execute in program order without explicit synchronization primitives.

The absence of cache management instructions in analyzed modules suggests either:
- No cache present in this ME1 configuration
- Cache is transparent and requires no software management
- Cache coherency is maintained by hardware

## Module Load Addresses

Analysis of extracted ME1 modules reveals the following load address patterns:

```
Module          Typical Load Address
------          --------------------
BRINGUP         0x10000000 range
PRELOADER       0x10000000 range
KernelPriv      0x10100000 range
KernelNonPriv   0x10200000 range
Overlays        Dynamically loaded
```

Exact addresses are determined by the PRELOADER during boot based on module size and dependency order.

## Stack Size Allocation

Stack sizes vary by module based on maximum call depth and local variable requirements:

```
Module Type     Typical Stack Size
-----------     ------------------
Boot modules    4-8 KB
Kernel          16-32 KB
Service modules 8-16 KB
Overlays        4-8 KB
```

Stack overflow detection is not present in ME1 firmware. Stack size must be sufficient for worst-case call depth.

## Memory Map Example: PRELOADER Module

Concrete example from PRELOADER module analysis:

```
0x10000000      Module header ($MAN signature)
0x100000A0      Entry point (offset 0xA0 from header)
0x10000100      Code section start
0x10000BB0      Initialization function
0x10000C78      Dispatch function
0x10001000      Read-only data section
0x100010FC      Global data pointer (gp initialization value)
0x10012210      Stack base (sp initialization value)
```

This layout shows the typical organization of a small boot module with code, data, and stack in close proximity.

## References

- ARCompact Architecture Programmer's Reference Manual
- Intel ME1 firmware modules: PRELOADER, BRINGUP, KernelPriv
- Disassembly analysis of module entry points and initialization sequences
- Module header structure from m1u extraction tool

Content rephrased for compliance with licensing restrictions. Memory addresses verified against actual ME1 module disassembly.
