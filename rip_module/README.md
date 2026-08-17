# SliceSoft RIP module

This directory contains the tracked configuration and deployment metadata for
the external RIP integration. The ignored `rip_project` directory is the local
SDK input; `scripts/PackageRipModule.ps1` produces the relocatable runtime
directory.

The generated module is for local engineering use only. External distribution
remains blocked until the RIP binary, lcms2, ICC profiles and private LibTIFF
provenance and license evidence are supplied.

```powershell
./scripts/PackageRipModule.ps1 `
  -SourceRoot ./rip_project `
  -Destination ./output/ripflow/modules/rip

./scripts/TestRipModulePackage.ps1 `
  -ModuleDirectory ./output/ripflow/modules/rip
```
