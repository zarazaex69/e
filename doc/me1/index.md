# Intel Management Engine 1.x Documentation

Comprehensive technical documentation of Intel ME1 firmware architecture, modules, and security analysis.

## Documentation Structure

### [Architecture](architecture/)
ARC processor architecture, memory layout, instruction set, and calling conventions.

### [Boot Sequence](boot-sequence/)
Firmware initialization process from entry point through kernel loading.

### [Modules](modules/)
Detailed analysis of all 18 ME1 firmware modules organized by category.

### [Inter-Module Communication](inter-module/)
Module dependencies, API surfaces, and shared data structures.

### [Security Analysis](security/)
Vulnerability documentation and security assessment.

### [Methodology](methodology/)
Reverse engineering guides, tools setup, and analysis procedures.

## Module Categories

- **Boot**: BRINGUP, PRELOADER, BUCLS_OVL, BUPMSEQ_OVL
- **Kernel**: KernelPriv, KernelNonPriv
- **Power Management**: PMHWSEQ, MOFFM0_OVL
- **File System**: EFFS_IOVL, EFFS_OPOVL
- **Security**: TPM, PKTPM, PKTPMINIT_OVL, TDT
- **Biometrics**: UPEK
- **Services**: CLS, ALIASCHECK_OVL, SUPPORT_OVL

## Quick Links

- [Entry Point Analysis](boot-sequence/entry-point.md)
- [Module Classification](modules/index.md)
- [Dependency Graph](inter-module/dependency-graph.md)
- [Vulnerabilities](security/vulnerabilities.md)
- [Rizin Setup Guide](methodology/rizin-setup.md)
