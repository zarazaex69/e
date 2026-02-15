# Entry Point Analysis

## Overview

The ME1 firmware entry point is located at physical offset 0x720A0 in the me1x200.bin file. This address is discovered through the CODE partition module header at offset 0x72000.

## Physical Location

- File: me1x200.bin
- Physical Offset: 0x720A0
- Virtual Address: 0x00401000 (after memory mapping)
- Partition: CODE (starts at 0x72000)

## Module Header Structure

The CODE partition begins with a module header at 0x72000:

```
Offset  Value       Description
------  ----------  -----------
+0x00   0x00000004  Header size indicator
+0x04   0x000000A1  Entry point offset (0xA0 aligned)
+0x1C   $MAN        Module signature
+0x10   0x20081217  Build timestamp (2012.08.17)
```

Entry point calculation: 0x72000 + 0xA0 = 0x720A0

## Initial Instructions

The entry point function begins with data manipulation and control flow setup:

```
0x720A0:  asl_s  r2, r12, 1      
0x720A2:  extw_s r3, r13          
0x720A4:  ld     r18, [r20, r52]  
0x720A8:  ld_s   r14, pcl, 0x13c  
0x720AA:  asl_s  r13, r2, 2       
```

The function performs register initialization and loads constants from PC-relative addresses.

## Control Flow

The entry point function contains multiple conditional branches:

- 0x720B8: Branch to 0x7227E (error handler or alternate path)
- 0x720BA: Branch to 0x72098 if r15 equals zero
- 0x720C0: Loop back to 0x720AC if r13 is non-zero
- 0x720C2: Branch to 0x72132 if r0 equals zero

## Function Calls

The entry point makes calls to internal functions:

- 0x72108: Call to fcn.00072928 (initialization routine)
- 0x72124: Call to 0x729A0 (setup function)

## Transition to BRINGUP

The entry point function acts as a dispatcher that initializes the system and transfers control to the BRINGUP module. The BRINGUP module name is present in the firmware string table, indicating it is loaded dynamically during boot.

Module loading sequence:
1. Entry point at 0x720A0 initializes core system
2. Module loader identifies BRINGUP from partition table
3. BRINGUP module is loaded and executed
4. BRINGUP continues boot sequence to PRELOADER

## Memory Layout

```
0x00000000  Flash Partition Table ($FPT)
0x00010000  FOVD partition (key storage - empty)
0x00072000  CODE partition header
0x000720A0  Entry point function
...         Additional code and data
```

## Analysis Notes

The entry point function is complex with indirect branching patterns. Rizin automatic analysis (aaa) does not fully resolve all function calls due to:
- Dynamic address calculation
- Indirect jumps through registers
- Complex control flow with multiple nested branches

Manual analysis is required for complete understanding of the boot flow.

## References

- Module header analysis: reverse/me1/doc/note/code_entry_point.txt
- Architecture overview: reverse/me1/doc/note/architecture.txt
- Related modules: [BRINGUP](../modules/boot/BRINGUP.md), [PRELOADER](../modules/boot/PRELOADER.md)
