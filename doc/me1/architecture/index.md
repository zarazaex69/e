# Architecture

ARC processor architecture documentation for Intel ME1.

## Overview

Intel Management Engine version 1.x uses an ARCv2 (Argonaut RISC Core) processor with a mixed 16/32-bit instruction set. Analysis of 18 ME1 modules revealed comprehensive instruction usage including extended ARCv2 features not fully supported by standard disassemblers.

## Key Findings

- **Processor**: ARCv2 (16/32-bit RISC architecture)
- **Instruction Set**: Full ARCv2 ISA including extended opcodes
- **Analyzed Modules**: 18 ME1 firmware modules
- **Decoded Instructions**: 21,113 previously unrecognized ARCv2 instructions
- **Calling Convention**: Standard ARCompact ABI with ME1-specific optimizations

## Documents

- [ARC Processor](arc-processor.md) - Instruction set and processor architecture
- [Memory Layout](memory-layout.md) - Address space organization
- [Calling Conventions](calling-conventions.md) - Function call ABI
- [Instruction Set](instruction-set.md) - ARC instruction reference

## Instruction Coverage

ME1 modules use the complete ARCv2 instruction set:

- **16-bit instructions**: Format 1 (register-register), Format 2 (immediate), Format 3 (shifts)
- **32-bit instructions**: Extended arithmetic, conditional operations, LIMM (long immediate)
- **Condition suffixes**: .f, .d, .n, .nz, .z, .c, .nc, .v, .nv and combinations
- **ME-specific**: Custom instructions with prefix 0x0e10

## Analysis Tools

- **Rizin**: Partial ARCv2 support, 21,113 instructions appear as `???`
- **IDA Pro**: Full ARCv2 support with commercial ARC plugin
- **Ghidra**: Community ARC processor module available
- **Manual decoding**: ARCv2 ISA reference for unrecognized opcodes
