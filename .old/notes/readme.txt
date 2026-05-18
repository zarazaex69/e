ME1 Reverse Engineering Documentation
======================================

Documentation Structure:

Overview Documents
------------------
README.txt (this file)
  Documentation navigation

modules_overview.txt
  Boot module overview (PRELOADER, BRINGUP, overlays, kernels)
  Module dependencies
  Error codes

boot_sequence.txt
  Complete boot sequence from ROM to firmware
  Stage transitions
  MMU transition

Reference Documents
-------------------
architecture.txt
  CPU architecture (ARC/ARCompact)
  Register set
  Instruction set
  Toolchain setup

memory_map.txt
  Memory map (Flash, SRAM, I/O)
  Hardware register addresses
  Global data structures

data_structures.txt
  Memory descriptors ($MEM, $BUP, $HEC)
  Security signatures (CKEY)
  Overlay module names
  Firmware image structures ($FPT, $MAN, $MOD)

Detailed Module Analysis
-------------------------
preloader.txt
  Detailed PRELOADER.bin analysis
  Key function disassembly
  SRAM initialization

bringup.txt
  Detailed BRINGUP.bin analysis
  CKEY search
  Overlay initialization

Recommended Reading Order
-------------------------
1. README.txt (this file)
2. architecture.txt - understand CPU and toolchain
3. memory_map.txt - understand address space
4. modules_overview.txt - understand modules and their roles
5. boot_sequence.txt - understand complete boot sequence
6. data_structures.txt - understand data formats
7. preloader.txt / bringup.txt - detailed module analysis

Quick Information Lookup
-------------------------
Memory addresses -> memory_map.txt
Data structures -> data_structures.txt
Boot sequence -> boot_sequence.txt or modules_overview.txt
CPU architecture -> architecture.txt
Specific module -> preloader.txt or bringup.txt
Error codes -> modules_overview.txt
Firmware image format -> data_structures.txt

Next Analysis Steps
-------------------
1. BRINGUP to KernelPriv handoff mechanism
2. Code at 0xff816682 (post-MMU virtual address)
3. Page table setup code (before virtual mode transition)
4. fcn.0001690c loop implementation
5. Overlay module analysis
6. KernelPriv entry point and initialization
