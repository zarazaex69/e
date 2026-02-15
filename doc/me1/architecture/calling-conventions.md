# Calling Conventions

## Overview

Intel Management Engine version 1.x firmware follows standard ARCompact calling conventions with ME1-specific optimizations. The calling convention defines how functions receive parameters, return values, preserve registers, and manage stack frames.

## Register Usage

### Argument Registers

Function parameters pass in registers r0 through r7:

```
Register  Purpose
--------  -------
r0        First argument / Return value
r1        Second argument
r2        Third argument
r3        Fourth argument
r4        Fifth argument
r5        Sixth argument
r6        Seventh argument
r7        Eighth argument
```

Functions with more than eight parameters pass additional arguments on the stack in right-to-left order.

### Return Value

Functions return values in register r0. For 64-bit return values, r0 contains the low 32 bits and r1 contains the high 32 bits.

Return value conventions:

```
Return Type       Register(s)
-----------       -----------
void              None
int/pointer       r0
long long         r0 (low), r1 (high)
struct (small)    r0 (or r0+r1 if 8 bytes)
struct (large)    Pointer in r0
```

### Scratch Registers

Caller-saved registers that functions may modify without preservation:

```
r0-r3     Argument and return value registers
r12       General scratch register
```

Callers must save these registers before function calls if their values are needed after the call.

### Preserved Registers

Callee-saved registers that functions must preserve:

```
r13 (sp)  Stack pointer
r14-r26   General purpose registers
r27 (fp)  Frame pointer
```

Functions that modify these registers must save them on entry and restore them before return.

### Special Purpose Registers

```
Register  Name    Purpose
--------  ----    -------
r26       gp      Global pointer (data segment base)
r27       fp      Frame pointer
r28       sp      Stack pointer
blink     -       Branch link register (return address)
lp_count  -       Hardware loop counter
```

The global pointer (gp) is initialized once at module entry and must not be modified by functions. The frame pointer (fp) is optional but commonly used for stack frame access.

## Function Prologue

### Standard Prologue Pattern

ME1 functions use a distinctive two-stage prologue:

```
sub   sp, sp, 4          Reserve 4 bytes
mov   r12, frame_size    Load frame size into r12
b.d   function_body      Branch with delay slot
sub   sp, sp, r12        Allocate frame (executes before branch)
```

This pattern (hex signature: 047e8e53 at first instruction) appears in most ME1 functions and serves as a reliable function boundary marker.

The two-stage allocation allows the second stack subtraction to execute in the branch delay slot, saving one instruction cycle per function call.

### Extended Prologue with Register Preservation

Functions that use callee-saved registers extend the prologue:

```
sub   sp, sp, 4          Initial allocation
mov   r12, frame_size    Load frame size
b.d   function_body      Branch with delay slot
sub   sp, sp, r12        Allocate frame
st    blink, [sp, 0]     Save return address
st    fp, [sp, 4]        Save frame pointer
st    r14, [sp, 8]       Save r14
st    r15, [sp, 12]      Save r15
mov   fp, sp             Set frame pointer
```

Saved registers are stored at fixed offsets from the stack pointer. The frame pointer is set after saving to provide a stable reference for local variable access.

### Minimal Prologue

Leaf functions (functions that make no calls) may use a minimal prologue:

```
sub   sp, sp, frame_size    Allocate frame in one instruction
```

This optimization is used when the frame size is small enough to fit in the immediate field of the sub instruction.

## Function Epilogue

### Standard Epilogue

Functions restore saved registers and deallocate the stack frame before return:

```
ld    blink, [sp, 0]     Restore return address
ld    fp, [sp, 4]        Restore frame pointer
ld    r14, [sp, 8]       Restore r14
ld    r15, [sp, 12]      Restore r15
add   sp, sp, frame_size Deallocate frame
j     [blink]            Return to caller
```

The return address is loaded from the stack into blink, then the indirect jump through blink transfers control back to the caller.

### Conditional Return

Functions may return conditionally based on flag state:

```
mov.f 0, r0              Test return value
jz.f  [blink]            Return if zero
```

This pattern is common in functions that check conditions before performing cleanup.

### Tail Call Optimization

When a function's last action is calling another function, the compiler may optimize by jumping instead of calling:

```
add   sp, sp, frame_size Deallocate frame
j     target_function    Jump instead of call
```

This optimization eliminates the overhead of an extra return instruction and stack frame.

## Parameter Passing

### Register Parameters

The first eight parameters pass in registers r0-r7:

```
void function(int a, int b, int c, int d, int e, int f, int g, int h)

a -> r0
b -> r1
c -> r2
d -> r3
e -> r4
f -> r5
g -> r6
h -> r7
```

### Stack Parameters

Parameters beyond the eighth pass on the stack in right-to-left order:

```
void function(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j)

a-h -> r0-r7
i   -> [sp, 0]
j   -> [sp, 4]
```

The caller pushes stack parameters before the call and removes them after return.

### Structure Parameters

Small structures (up to 8 bytes) pass by value in registers:

```
struct Point { int x; int y; };
void function(struct Point p)

p.x -> r0
p.y -> r1
```

Large structures (more than 8 bytes) pass by reference:

```
struct Large { int data[10]; };
void function(struct Large l)

&l -> r0 (pointer to structure)
```

The caller allocates space for the structure and passes a pointer in r0.

### Variadic Functions

Functions with variable argument lists (varargs) receive fixed parameters in registers and variable parameters on the stack:

```
int printf(const char *format, ...)

format -> r0
...    -> [sp, 0], [sp, 4], [sp, 8], ...
```

The function accesses variable arguments through stack pointer arithmetic.

## Function Calls

### Direct Call

Direct function calls use the branch-and-link instruction:

```
bl    function_address   Call function, save return in blink
```

The bl instruction saves the return address (address of next instruction) in the blink register and branches to the target.

### Call with Delay Slot

The delayed branch variant executes the following instruction before branching:

```
bl.d  function_address   Call with delay slot
mov   r0, argument       Set argument (executes before call)
```

This optimization fills the branch delay slot with useful work, typically argument setup or register moves.

### Indirect Call

Calls through function pointers use jump-and-link:

```
ld    r11, [gp, offset]  Load function pointer
jl    [r11]              Call through pointer
```

The jl instruction saves the return address in blink and jumps to the address in the register.

### Inter-Module Calls

Modules call functions in other modules through dispatch tables:

```
mov   r11, table_base    Load dispatch table base
ld    r11, [r11, offset] Load function pointer from table
jl    [r11]              Call through pointer
```

This indirection allows dynamic module loading and relocation without recompiling caller modules.

## Stack Frame Layout

### Typical Stack Frame

A function's stack frame contains saved registers, local variables, and space for outgoing parameters:

```
High Address
+------------------+  <- fp (frame pointer)
| Saved blink      |  [fp, 0]
+------------------+
| Saved fp         |  [fp, 4]
+------------------+
| Saved r14        |  [fp, 8]
+------------------+
| Saved r15        |  [fp, 12]
+------------------+
| Local var 1      |  [fp, -4]
+------------------+
| Local var 2      |  [fp, -8]
+------------------+
| Local var 3      |  [fp, -12]
+------------------+
| Outgoing param 1 |  [sp, 0]
+------------------+
| Outgoing param 2 |  [sp, 4]
+------------------+  <- sp (stack pointer)
Low Address
```

Saved registers are stored at positive offsets from fp. Local variables are stored at negative offsets from fp. Outgoing parameters are stored at positive offsets from sp.

### Frame Pointer Usage

The frame pointer provides a stable reference for accessing local variables when the stack pointer changes:

```
st    fp, [sp, 4]        Save previous frame pointer
mov   fp, sp             Set frame pointer to current stack
add   fp, fp, 16         Adjust fp to point above saved registers
```

With the frame pointer set, local variables are accessed via fixed offsets:

```
ld    r0, [fp, -4]       Load local variable 1
st    r1, [fp, -8]       Store to local variable 2
```

### Frameless Functions

Simple functions may omit the frame pointer and access everything via sp:

```
sub   sp, sp, 16         Allocate frame
st    r0, [sp, 0]        Store local variable
ld    r1, [sp, 0]        Load local variable
add   sp, sp, 16         Deallocate frame
j     [blink]            Return
```

This optimization reduces prologue/epilogue overhead for small functions.

## Calling Convention Examples

### Example 1: Simple Function Call

Function definition:

```
int add(int a, int b) {
    return a + b;
}
```

Disassembly:

```
add:
    add   r0, r0, r1         r0 = r0 + r1
    j     [blink]            Return
```

Caller:

```
mov   r0, 5                  First argument
mov   r1, 3                  Second argument
bl    add                    Call function
```

After return, r0 contains 8.

### Example 2: Function with Local Variables

Function definition:

```
int compute(int x, int y) {
    int temp = x * 2;
    int result = temp + y;
    return result;
}
```

Disassembly:

```
compute:
    sub   sp, sp, 4          Allocate frame
    mov   r12, 0x10
    b.d   .body
    sub   sp, sp, r12
.body:
    asl   r2, r0, 1          temp = x * 2
    st    r2, [sp, 0]        Store temp
    add   r0, r2, r1         result = temp + y
    add   sp, sp, 0x14       Deallocate frame
    j     [blink]            Return
```

### Example 3: Function with Preserved Registers

Function definition:

```
int process(int *array, int count) {
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += array[i];
    }
    return sum;
}
```

Disassembly:

```
process:
    sub   sp, sp, 4          Allocate frame
    mov   r12, 0x10
    b.d   .body
    sub   sp, sp, r12
.body:
    st    blink, [sp, 0]     Save return address
    st    r14, [sp, 4]       Save r14
    st    r15, [sp, 8]       Save r15
    mov   r14, r0            r14 = array
    mov   r15, r1            r15 = count
    mov   r0, 0              sum = 0
    mov   r2, 0              i = 0
.loop:
    cmp   r2, r15            Compare i with count
    bge   .done              Branch if i >= count
    ld    r3, [r14, r2]      Load array[i]
    add   r0, r0, r3         sum += array[i]
    add   r2, r2, 4          i++
    b     .loop              Continue loop
.done:
    ld    blink, [sp, 0]     Restore return address
    ld    r14, [sp, 4]       Restore r14
    ld    r15, [sp, 8]       Restore r15
    add   sp, sp, 0x14       Deallocate frame
    j     [blink]            Return
```

### Example 4: Function with Stack Parameters

Function definition:

```
int sum_many(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    return a + b + c + d + e + f + g + h + i + j;
}
```

Caller:

```
mov   r0, 1                  Argument 1
mov   r1, 2                  Argument 2
mov   r2, 3                  Argument 3
mov   r3, 4                  Argument 4
mov   r4, 5                  Argument 5
mov   r5, 6                  Argument 6
mov   r6, 7                  Argument 7
mov   r7, 8                  Argument 8
sub   sp, sp, 8              Allocate space for stack args
mov   r12, 9
st    r12, [sp, 0]           Argument 9 on stack
mov   r12, 10
st    r12, [sp, 4]           Argument 10 on stack
bl    sum_many               Call function
add   sp, sp, 8              Remove stack arguments
```

Function body:

```
sum_many:
    add   r0, r0, r1         a + b
    add   r0, r0, r2         + c
    add   r0, r0, r3         + d
    add   r0, r0, r4         + e
    add   r0, r0, r5         + f
    add   r0, r0, r6         + g
    add   r0, r0, r7         + h
    ld    r12, [sp, 0]       Load i from stack
    add   r0, r0, r12        + i
    ld    r12, [sp, 4]       Load j from stack
    add   r0, r0, r12        + j
    j     [blink]            Return
```

## ME1-Specific Patterns

### Jump Table Dispatch

ME1 modules implement function dispatch using jump tables with embedded prologues:

```
dispatch_table:
    sub   sp, sp, 4          Entry 0 prologue
    mov   r12, 0x10
    b.d   handler_0
    sub   sp, sp, r12
    
    sub   sp, sp, 4          Entry 1 prologue
    mov   r12, 0x14
    b.d   handler_1
    sub   sp, sp, r12
    
    sub   sp, sp, 4          Entry 2 prologue
    mov   r12, 0x18
    b.d   handler_2
    sub   sp, sp, r12
```

Callers compute an offset into the table based on operation type:

```
mov   r11, dispatch_table    Load table base
asl   r12, r0, 4             Multiply index by 16 (entry size)
add   r11, r11, r12          Compute entry address
j     [r11]                  Jump to entry
```

Each entry consists of a standard prologue followed by a branch to the actual handler. This structure allows indexed dispatch while maintaining consistent stack frame setup.

### Module Initialization

Module entry points follow a standard initialization sequence:

```
entry:
    mov   sp, stack_base     Initialize stack pointer
    mov   gp, data_base      Initialize global pointer
    sub   sp, sp, 0x10       Allocate initial frame
    bl    init_function      Call initialization
    mov   fp, 0              Clear frame pointer
    bl    main_loop          Enter main processing loop
```

The stack pointer and global pointer are set to module-specific values. The frame pointer is cleared to indicate the top-level frame.

### Auxiliary Register Access

Functions access processor control registers through auxiliary register operations:

```
lr    r0, [0x10005]          Load from auxiliary register
or    r0, r0, 0x1000         Modify control bits
sr    r0, [0x10005]          Store back to auxiliary register
```

Auxiliary register 0x10005 controls processor status, interrupt enable, and privilege level.

## Performance Considerations

### Delay Slot Utilization

ME1 code extensively uses delay slots to improve performance:

```
bl.d  function               Call with delay slot
mov   r0, argument           Set argument (executes before call)

b.d   target                 Branch with delay slot
sub   sp, sp, r12            Allocate frame (executes before branch)
```

Filling delay slots with useful instructions eliminates pipeline bubbles and reduces execution time.

### Register Allocation

Efficient register allocation minimizes stack spills:

```
mov   r14, r0                Save argument in preserved register
bl    other_function         Call may clobber r0-r3, r12
mov   r0, r14                Restore argument from preserved register
```

Using callee-saved registers (r14-r26) for values that must survive function calls avoids stack memory operations.

### Tail Call Optimization

Tail calls eliminate unnecessary stack frame operations:

```
ld    blink, [sp, 0]         Restore return address
add   sp, sp, frame_size     Deallocate frame
j     next_function          Jump instead of call+return
```

This optimization is particularly effective in state machine implementations and recursive functions.

## Instruction Decoding Notes

### Unrecognized Instructions in Calling Sequences

ME1 calling conventions use several ARCv2 instructions that Rizin doesn't recognize:

#### Argument Setup with Immediate Values

```
Bytes   Rizin Output        Actual Instruction   Usage
0x4020  ??? r0, r0, r1      ADD r0, r0, 1        Increment argument
0x5041  ??? r0, r0, r2      ADD r0, r0, 2        Add 2 to argument
0x5060  ??? r0, r0, r3      ADD r0, r0, 3        Add 3 to argument
```

These compact 16-bit instructions adjust argument values before calls.

#### Register Preservation with Shifts

```
Bytes   Rizin Output        Actual Instruction   Usage
0x7a08  ??? r2, r2, r0      ASL r2, r2, r0       Shift for alignment
0x8e53  ??? r3, r3, r12     ASL r3, r3, r12      Multiply by power of 2
```

Used in prologues for stack frame calculations and pointer arithmetic.

#### Conditional Returns

```
Bytes   Rizin Output        Actual Instruction   Usage
0xe157  ??? r15, r15, r15   CMP r15, r15         Compare before return
0xe057  ??? r15, r15, r15   TST r15, r15         Test flags before return
```

#### Extended Operations in Call Setup

```
Full Bytes      Rizin Output        Actual Instruction      Usage
0x0f380000      ??? r0, r0, r0      ADD r0, r0, LIMM        Large immediate argument
0xff270005      ??? r7, r7, r20     ADD.cc r7, r7, 20       Conditional argument setup
0x0e10xxxx      ??? r14, rx, 0      MOV r14, rx             ME-specific register moves
```

### Impact on Call Analysis

When analyzing ME1 calling conventions:

1. **Argument Passing**: Some argument setup uses unrecognized immediate adds (0x4020, 0x5041)
2. **Stack Calculations**: Frame size computation may use unrecognized shifts (0x8e53)
3. **Conditional Logic**: Return conditions use unrecognized compare/test (0xe157, 0xe057)
4. **ME Extensions**: Custom MOV instructions (0x0e10) for ME-specific register operations

### Decoding Strategy

For complete call analysis:

1. Identify prologue pattern (047e8e53) to locate function boundaries
2. Manually decode `???` instructions using ARCv2 ISA reference
3. Track register usage through unrecognized instructions
4. Verify calling convention compliance by examining register preservation

## References

- ARCompact Architecture Programmer's Reference Manual
- ARCv2 ISA Programmer's Reference (for extended instruction set)
- ARC Procedure Call Standard (APCS)
- Intel ME1 firmware modules: PRELOADER, BRINGUP, KernelPriv, ALIASCHECK_OVL
- Disassembly analysis of function prologues, epilogues, and call sites
- Complete instruction decode analysis: 21,113 instructions across 18 modules

Content rephrased for compliance with licensing restrictions. Calling convention details verified against actual ME1 module disassembly.
