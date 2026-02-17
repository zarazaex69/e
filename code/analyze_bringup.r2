e asm.arch=arc
e asm.bits=16
e cfg.bigendian=false

f base_addr @ 0x10000000
f entry_point @ 0x10000204
f export_table @ 0x1000036c
f main_init @ 0x1000058c
f jump_table @ 0x100010a0
f bup_header @ 0x100013d0
f str_version @ 0x10001f2c

s entry_point
af
s main_init
af
s jump_table
af

CC Header Signature @ bup_header
CC Version String @ str_version
CC Jump Table @ jump_table

s main_init
