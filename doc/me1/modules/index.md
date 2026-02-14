# Modules

Intel ME1 firmware consists of 18 modules organized by functional category.

## Module Classification

### Boot Modules
- [BRINGUP](boot/BRINGUP.md) - Initial boot module
- [PRELOADER](boot/PRELOADER.md) - Module loader
- [BUCLS_OVL](boot/BUCLS_OVL.md) - Boot overlay
- [BUPMSEQ_OVL](boot/BUPMSEQ_OVL.md) - Boot sequence overlay

### Kernel Modules
- [KernelPriv](kernel/KernelPriv.md) - Privileged kernel (LZMA compressed, 240KB)
- [KernelNonPriv](kernel/KernelNonPriv.md) - Non-privileged kernel (LZMA compressed, 250KB)

### Power Management
- [PMHWSEQ](power/PMHWSEQ.md) - Power management hardware sequencer
- [MOFFM0_OVL](power/MOFFM0_OVL.md) - Power state overlay

### File System
- [EFFS_IOVL](filesystem/EFFS_IOVL.md) - Embedded Flash File System I/O
- [EFFS_OPOVL](filesystem/EFFS_OPOVL.md) - EFFS operations

### Security
- [TPM](security/TPM.md) - Trusted Platform Module (265KB, largest compressed)
- [PKTPM](security/PKTPM.md) - TPM packet handler
- [PKTPMINIT_OVL](security/PKTPMINIT_OVL.md) - TPM initialization
- [TDT](security/TDT.md) - Theft Deterrent Technology

### Biometrics
- [UPEK](biometrics/UPEK.md) - Fingerprint reader (373KB, largest module)

### Services
- [CLS](services/CLS.md) - Capability Licensing Service
- [ALIASCHECK_OVL](services/ALIASCHECK_OVL.md) - Alias checking
- [SUPPORT_OVL](services/SUPPORT_OVL.md) - Support overlay

## Module Statistics

Total modules: 18
- Compressed modules: 2 (KernelPriv, KernelNonPriv)
- Overlay modules: 7
- Core modules: 9
