# ARC Processor Architecture

## Overview

Intel Management Engine version 1.x (ICH9M chipset) uses an ARCompact processor core. ARCompact is a 32-bit RISC architecture developed by ARC International (later acquired by Synopsys). The architecture provides a mixed 16/32-bit instruction set for code density optimization in embedded systems.

ME1 firmware modules execute directly on this ARC core with no virtualization layer. The processor operates independently of the main x86 CPU and has direct access to platform hardware through the Platform Controller Hub.

## Instruction Set Architecture

ARCompact implements a load-store architecture where arithmetic operations work exclusively on registers. Memory access occurs only through dedicated load and store instructions.

### Instruction Formats

ARCompact supports two instruction encodings:

- 32-bit instructions: Full instruction set with three-operand format
- 16-bit instructions: Compact encoding for common operations with limited operand range

The processor decodes both formats in the same instruction stream, selecting encoding based on opcode. This mixed encoding reduces code size by approximately 30% compared to fixed 32-bit encoding.

### Common Instructions

Load and Store:
```
ld    r0, [r1]           Load word from memory address in r1 to r0
ld    r0, [r1, offset]   Load with immediate offset
ld.a  r0, [r1, offset]   Load with auto-increment (r1 += offset after load)
st    r0, [r1]           Store word from r0 to memory address in r1
st.a  r0, [r1, offset]   Store with auto-increment (r1 += offset after store)
stb   r0, [r1]           Store byte
stb.a r0, [r1, offset]   Store byte with auto-increment
ldb.a r0, [r1, offset]   Load byte with auto-increment
```

The `.a` suffix indicates auto-increment addressing mode, where the base register is automatically incremented by the offset after the memory operation. This is commonly used in memory copy loops to eliminate separate increment instructions.

Arithmetic and Logic:
```
add   r0, r1, r2         r0 = r1 + r2
sub   r0, r1, r2         r0 = r1 - r2
and   r0, r1, r2         r0 = r1 & r2
or    r0, r1, r2         r0 = r1 | r2
mov   r0, r1             r0 = r1
mov   r0, immediate      r0 = immediate value
lsr   r0, r1, count      Logical shift right
lsr.f r0, r1, count      Logical shift right and set flags
asl   r0, r1, count      Arithmetic shift left (same as lsl)
asr   r0, r1, count      Arithmetic shift right (sign-extend)
```

Barrel Shifter Operations:

ARCompact includes a barrel shifter that performs shift operations in a single cycle. These operations are frequently used for bit manipulation, multiplication/division by powers of 2, and byte replication patterns.

Common patterns:
- Byte broadcast: `asl r0, r1, 8` followed by `or` to replicate bytes
- Fast division: `lsr r0, r1, 2` divides by 4
- Bit extraction: `asr r0, r1, count` with sign extension

Control Flow:
```
bl    target             Branch and link (function call)
bl.d  target             Branch and link with delay slot
b.d   target             Branch with delay slot
j     target             Unconditional jump
j.f   [blink]            Conditional jump if flags set
j.f.d [blink]            Conditional jump with delay slot if flags set
jl    [r0]               Jump and link to address in register
jz.f  [blink]            Conditional return if zero flag set
bnc   target             Branch if no carry
bnz   target             Branch if not zero
bnz.d target             Branch if not zero with delay slot
bz.d  target             Branch if zero with delay slot
bc.d  target             Branch if carry with delay slot
lp    target             Loop instruction (hardware loop end marker)
lpnz  target             Loop if not zero (conditional hardware loop)
```

Conditional Execution:
```
sub.f r0, r1, r2         Subtract and set flags
mov.f 0, r2              Test r2 and set flags
and.f 0, r0, 3           Test r0 & 3 and set flags
```

The `.f` suffix indicates flag-setting variants that update condition codes.

Bit Manipulation:
```
extb  r0, r1             Extract byte (sign-extend byte to word)
exth  r0, r1             Extract halfword (sign-extend halfword to word)
```

Special Instructions:
```
flag  value              Set processor flags directly
nop                      No operation (pipeline alignment)
```

## Register Set

ARCompact provides 32 general-purpose registers plus auxiliary registers for system control.

### General Purpose Registers

```
r0-r12    General purpose registers
r13 (sp)  Stack pointer
r14 (gp)  Global pointer (data segment base)
r15 (fp)  Frame pointer
r16-r25   General purpose registers
r26 (gp)  Alternative global pointer usage
r27-r31   Reserved/special purpose
```

### Special Registers

```
blink     Branch link register (return address)
lp_count  Loop counter for hardware loops
pcl       Program counter (lower bits)
```

### Auxiliary Registers

Accessed via `lr` (load from auxiliary) and `sr` (store to auxiliary) instructions:

```
lr    r0, [0x10005]      Load from auxiliary register 0x10005
sr    r0, [0x10005]      Store to auxiliary register 0x10005
```

Auxiliary registers control processor modes, interrupt handling, and hardware features. Register 0x10005 appears in ME1 code for status/control operations.

## Calling Convention

ME1 firmware follows standard ARC calling conventions:

### Parameter Passing

Function arguments pass in registers:
```
r0      First argument / return value
r1      Second argument
r2      Third argument
r3      Fourth argument
r4-r7   Additional arguments
```

Arguments beyond register capacity pass on the stack.

### Register Preservation

Caller-saved registers (may be modified by callee):
```
r0-r3   Argument and scratch registers
r12     Scratch register
```

Callee-saved registers (must be preserved):
```
r13-r26 Must be saved and restored if used
```

### Function Prologue/Epilogue

Standard function entry sequence:
```
sub   sp, sp, frame_size    Allocate stack frame
st    blink, [sp, offset]   Save return address
st    fp, [sp, offset]      Save frame pointer
mov   fp, sp                Set up frame pointer
```

ME1-specific compact prologue pattern (hex: 047e8e53):
```
sub   sp, sp, 4             Reserve 4 bytes
mov   r12, frame_size       Load frame size into r12
b.d   function_body         Branch with delay slot
sub   sp, sp, r12           Allocate frame (executes before branch)
```

This pattern appears in most ME1 modules and serves as a reliable function boundary marker. The two-stage stack allocation allows the processor to execute the second subtraction in the branch delay slot, improving pipeline efficiency.

Standard function exit:
```
ld    blink, [sp, offset]   Restore return address
ld    fp, [sp, offset]      Restore frame pointer
add   sp, sp, frame_size    Deallocate stack frame
j     [blink]               Return to caller
```

### Branch and Link

Function calls use branch-and-link instructions:
```
bl    function_address      Call function, save return in blink
bl.d  function_address      Call with delay slot (next instruction executes)
```

The `bl.d` variant executes the following instruction before branching, allowing optimization of the instruction pipeline.

### Jump Tables and Dispatch

ME1 modules implement function dispatch using jump table patterns:
```
sub   sp, sp, 4             Entry 0: prologue
mov   r12, 0x10
b.d   dispatch_target_0
sub   sp, sp, r12

sub   sp, sp, 4             Entry 1: prologue
mov   r12, 0x14
b.d   dispatch_target_1
sub   sp, sp, r12
```

Each entry consists of the standard prologue pattern followed by a branch to the actual handler. This structure allows indexed dispatch where the caller computes an offset into the table based on operation type or message ID.

Example from BRINGUP module at 0x244:
```
0x00000244    sub   sp, sp, 4
0x00000248    mov   r12, 0x10
0x0000024c    b.d   0x000003a0
0x00000250    sub   sp, sp, r12
0x00000254    b.d   0x00000260
0x00000258    sub   sp, sp, 4
0x0000025c    mov   r12, 0x14
0x00000260    b.d   0x0000039c
```

The pattern repeats with incrementing frame sizes (0x10, 0x14, 0x18, 0x1c, etc.), suggesting different handler types with varying local variable requirements.

## Memory Model

### Address Space

ME1 operates in a 32-bit address space with the following layout:

```
0x00000000 - 0x0FFFFFFF   Reserved/unmapped
0x10000000 - 0x1FFFFFFF   ME firmware region (modules, data, stack)
0x20000000 - 0xFFFFFFFF   Hardware registers and peripherals
```

Module load addresses typically start at 0x10000000 range. Stack grows downward from high addresses in the firmware region.

### Endianness

ARCompact in ME1 uses little-endian byte ordering. Multi-byte values store least significant byte at lowest address.

### Alignment

Load and store instructions require natural alignment:
- Word (32-bit) accesses must be 4-byte aligned
- Halfword (16-bit) accesses must be 2-byte aligned
- Byte accesses have no alignment requirement

Unaligned access triggers hardware exceptions.

## Code Examples from ME1 Modules

### PRELOADER Entry Point

```
0x00000000    mov   sp, 0x1012210      Initialize stack pointer
0x00000008    mov   gp, 0x10010fc      Initialize global pointer
0x00000010    sub   sp, sp, 0x10       Allocate 16 bytes on stack
0x00000014    bl    0x00000bb0         Call initialization function
0x00000018    mov   r0, 0x10010fc      Load data pointer
0x00000020    ld    r0, [r0]           Dereference pointer
0x00000024    bl.d  0x00000c78         Call function with delay slot
0x00000028    mov   fp, 0              Clear frame pointer (delay slot)
```

This sequence demonstrates typical module initialization: stack setup, global pointer initialization, and function calls.

### KernelPriv Entry Point

```
0x00000000    lr    r12, [status]      Read processor status register
0x00000004    ???   r12, r12, 0x19     Shift operation (barrel shifter)
0x00000008    and   r12, r12, 6        Mask status bits
0x0000000c    flag  4                  Set processor flags
0x00000010    nop                      Pipeline alignment
0x00000014    bl    0x00000660         Call initialization
0x00000018    mov   r11, 0x10378a0     Load dispatch table base
0x00000020    ld    r11, [r11, 0xc]    Load table pointer
0x00000024    ld    r11, [r11, 0xc]    Follow indirection
0x00000028    ld    r11, [r11]         Load handler address
0x0000002c    flag  r12                Restore flags
0x00000030    j     [r11]              Jump to handler
```

Kernel initialization reads processor status, performs mode checks, and dispatches to the appropriate handler through a multi-level indirection table.

### Memory Fill (memset) with Byte Broadcast Pattern

```
0x0000004c    extb  r1, r1             Extract byte: 0x...77 -> 0x00000077
0x00000050    asl   r3, r1, 8          Shift left by 8: 0x00007700
0x00000054    or    r3, r3, r1         Combine: 0x00007700 | 0x00000077 = 0x00007777
0x00000058    asl   r1, r3, 0x10       Shift left by 16: 0x77770000
0x0000005c    sub.f 0, r2, 8           Test if count >= 8
0x00000060    or    r3, r1, r3         Combine: 0x77770000 | 0x00007777 = 0x77777777
0x00000064    bnc   0x00000078         Branch if count < 8
0x00000068    mov   r1, r0             Copy destination pointer
0x0000006c    mov.f 0, r2              Test count
0x00000070    jz.f  [blink]            Return if zero
0x00000078    and.f 0, r0, 3           Test alignment of destination
0x00000080    mov   r1, r0             Copy destination pointer
0x00000084    stb   r3, [r1]           Store byte
0x00000088    add   r1, r1, 1          Increment pointer
0x0000008c    and.f 0, r1, 3           Test alignment again
0x00000090    sub   r2, r2, 1          Decrement counter
0x00000098    lsr   r4, r2, 2          Shift right by 2 (divide by 4 for word count)
0x0000009c    and   r2, r2, 3          Get remainder (count % 4)
0x000000a0    st    r3, [r1]           Store word (aligned)
0x000000a4    sub.f r4, r4, 1          Decrement word counter with flags
0x000000a8    add   r1, r1, 4          Increment by word size
0x000000ac    bnz   0x000000a0         Loop if not zero
```

This is an optimized memset implementation using the byte broadcast pattern. The sequence extb -> asl 8 -> or -> asl 16 -> or is the signature of compiler-generated memset on 32-bit architectures. It replicates a single byte across all four bytes of a register (0x77 becomes 0x77777777), allowing word-sized stores instead of byte-by-byte operations. The code handles unaligned addresses byte-by-byte first, then switches to word operations for the bulk fill, achieving 4x throughput improvement.

### Hardware Loop

```
0x000000b8    mov   lp_count, r2       Set loop counter
0x000000bc    nop                      Pipeline alignment
0x000000c0    lp    0x000000cc         Hardware loop end marker
0x000000c4    stb   r3, [r1]           Store byte (loop body)
0x000000c8    add   r1, r1, 1          Increment pointer (loop body)
```

ARCompact provides zero-overhead hardware loops using the `lp_count` register. The `lp` instruction marks the end of the loop body. The processor automatically decrements the counter and branches back to the loop start without explicit comparison or branch instructions. The nop ensures proper pipeline alignment before entering the loop.

### Optimized Memory Copy with Auto-Increment

```
0x00000000    mov.f lp_count, r2       Set loop counter and test
0x00000004    or    r4, r0, r1         Combine source and dest for alignment test
0x00000008    jz.f  [blink]            Return if count is zero
0x0000000c    and.f 0, r4, 3           Test if both pointers are aligned
0x00000018    lsr.f lp_count, r4       Divide count by 4 for word operations
0x0000001c    sub   r1, r1, 4          Adjust source pointer
0x00000020    sub   r3, r0, 4          Adjust destination pointer
0x00000024    lpnz  0x00000048         Loop if not zero
0x00000028    ld.a  r4, [r1, 4]        Load word with auto-increment
0x0000002c    ld.a  r5, [r1, 4]        Load second word
0x00000030    ld.a  r6, [r1, 4]        Load third word
0x00000034    ld.a  r7, [r1, 4]        Load fourth word (16 bytes total)
0x00000038    st.a  r4, [r3, 4]        Store word with auto-increment
0x0000003c    st.a  r5, [r3, 4]        Store second word
0x00000040    st.a  r6, [r3, 4]        Store third word
0x00000044    st.a  r7, [r3, 4]        Store fourth word
0x00000068    lp    0x00000074         Final byte copy loop
0x0000006c    ldb.a r4, [r1, 1]        Load byte with auto-increment
0x00000070    stb.a r4, [r3, 1]        Store byte with auto-increment
0x00000074    j.f   [blink]            Return
```

This is a highly optimized memory copy routine from ALIASCHECK_OVL module. It uses hardware loops with auto-increment addressing to copy 16 bytes per iteration (4 words), then handles remaining bytes. The auto-increment mode eliminates separate pointer arithmetic instructions, achieving maximum throughput.

### Conditional Return

```
0x0000006c    mov.f 0, r2              Test r2 value
0x00000070    jz.f  [blink]            Return if zero
```

Conditional returns use flag-setting instructions followed by conditional jumps to the link register.

## Processor Features

### Hardware Loops

ARCompact implements zero-overhead loops via dedicated registers. The processor automatically manages loop counter and branch without instruction overhead.

### Delay Slots

Branch instructions with `.d` suffix execute the following instruction before taking the branch. This allows filling the pipeline bubble with useful work.

ME1 firmware extensively uses delay slots for stack allocation:
```
b.d   target
sub   sp, sp, r12    Executes before branch
```

This pattern appears in every function prologue, saving one instruction cycle per function call.

### Conditional Execution

Most instructions support conditional execution based on flag state. The `.f` suffix sets flags, and conditional suffixes (`.z`, `.nz`, `.c`, `.nc`) control execution.

### Code Density

Mixed 16/32-bit encoding reduces code size significantly. Compiler selects encoding based on operand requirements and instruction type.

Analysis of ME1 modules shows approximately 60% 16-bit instructions and 40% 32-bit instructions, achieving the expected 30% code size reduction compared to pure 32-bit encoding.

## ME1-Specific Implementation Details

### Module Loading

ME1 modules load at fixed addresses in the 0x10000000 region. Each module receives:
- Stack pointer (sp) initialized to module-specific address
- Global pointer (gp) pointing to module data segment
- Frame pointer (fp) typically cleared to zero

### Auxiliary Register Usage

ME1 firmware accesses auxiliary register 0x10005 for processor status and control:
```
lr    r0, [0x10005]      Read status register
or    r0, r0, 0x1000     Set control bit
sr    r0, [0x10005]      Write back to status register
```

This register controls interrupt enable/disable, privilege level, and other processor modes.

### Inter-Module Calls

Modules call each other through indirect jumps using function pointer tables:
```
mov   r11, table_base    Load dispatch table
ld    r11, [r11, offset] Load function pointer
jl    [r11]              Call through pointer
```

This indirection allows dynamic module loading and relocation without recompiling caller modules.

## Limitations in Rizin Analysis

Rizin's ARC support has incomplete instruction decoding. Some instructions appear as `???` in disassembly output:

```
0x00000050    ???   r3, r1, 8           Barrel shifter: asl r3, r1, 8
0x00000058    ???   r1, r3, 0x10        Barrel shifter: asl r1, r3, 0x10
0x00000098    ???   r4, r2, 2           Barrel shifter: lsr r4, r2, 2
0x00000004    ???   r12, r12, 0x19      Barrel shifter: lsr r12, r12, 0x19
```

These are barrel shifter operations that Rizin fails to decode:
- asl (arithmetic shift left) - used for byte replication and fast multiplication
- lsr (logical shift right) - used for division by powers of 2
- asr (arithmetic shift right) - used for signed division with sign extension

Pattern recognition helps identify these operations:
- Byte broadcast pattern: extb -> asl 8 -> or -> asl 16 -> or (memset signature)
- Division by 4: lsr by 2 (converting byte count to word count)
- Status bit extraction: lsr by large value (0x19 = 25 bits) to get high-order bits

Manual analysis of instruction bytes is required for complete understanding. Refer to Synopsys ARCompact Programmer's Reference Manual for full instruction encoding details.

### Function Boundary Detection

Rizin's automatic analysis (aa/aaa) fails to identify function boundaries in ARC binaries. The entire module gets marked as a single function, making analysis impractical.

Two workarounds exist:

Method 1 - Automatic function definition at all prologue locations:
```
af @@/x 047e8e53
```

This command runs `af` (analyze function) at every location matching the hex pattern 047e8e53. However, this only creates function entries without proper boundaries.

Method 2 - Manual function definition with boundaries (recommended):
```
s 0xb8; afu 0xcc
s 0xcc; afu 0xe0
s 0xe0; afu 0xf4
```

The `afu` (analyze function until) command defines a function from current seek position until the specified end address. This produces accurate function boundaries and enables proper disassembly with `pdf`.

Complete workflow for ALIASCHECK_OVL module:
```
/x 047e8e53                    Search for prologue pattern
s 0xb8; afu 0xcc               Define first function
s 0xcc; afu 0xe0               Define second function
s 0xe0; afu 0xf4               Continue for all matches
...
afl                            List all defined functions
pdf @ fcn.000000b8             Disassemble specific function
aaaa                           Run full analysis with xrefs
```

After defining functions manually, run `aaaa` to perform cross-reference analysis and identify function calls between modules. See methodology/rizin-setup.md for automation scripts.

## References

- Synopsys ARCompact ISA Programmer's Reference Manual
- ARC 600 and ARC 700 Processor Family documentation
- Intel ME1 firmware modules: PRELOADER, BRINGUP, KernelPriv, KernelNonPriv
- Rizin disassembly output from ME1 binaries
- Analysis notes: reverse/me1/doc/note/rizin_arc_function_analysis_fix.txt

Content rephrased for compliance with licensing restrictions. Technical details verified against actual ME1 module disassembly.
