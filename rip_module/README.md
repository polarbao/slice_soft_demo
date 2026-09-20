# SliceSoft RIP module

This directory contains the tracked configuration and deployment metadata for
the external RIP integration. The ignored `rip_project` directory is the local
SDK input; `scripts/PackageRipModule.ps1` produces the relocatable runtime
directory. `source.json` independently pins the reviewed binary and resource
inputs. Module 1.3.0 uses binaries from `rip_project/RIPDLL_20260920` and the
latest complete reviewed resources from
`rip_project/RIPDLL_20260909/CmykFiles`. The intermediate 20260917 and
20260918 directories remain review evidence and are not runtime selections.
Adding a future `RIPDLL_YYYYMMDD` directory does not change either source.
Update the pins only after reviewing the SDK interfaces, resources and tests.
Module version 1.3.0 requires `--transparent <0-4>`, `--ripmode <0|1>` and
`--verbose`; production commands do not enable verbose pixel logging. The
generated inventory records each payload file's SHA-256, and
`source_provenance.json` records the binary and resource directories
separately.

The generated module is for local engineering use only. External distribution
remains blocked until the RIP binary, lcms2, ICC profiles and private LibTIFF
provenance and license evidence are supplied.

```powershell
./scripts/PackageRipModule.ps1 `
  -Destination ./output/ripflow/modules/rip

./scripts/TestRipModulePackage.ps1 `
  -ModuleDirectory ./output/ripflow/modules/rip
```

For an explicit engineering override with split inputs, pass both roots:

```powershell
./scripts/PackageRipModule.ps1 `
  -SourceRoot ./rip_project/RIPDLL_YYYYMMDD `
  -ResourceSourceRoot ./rip_project/RIPDLL_20260909/CmykFiles `
  -Destination ./output/ripflow/modules/rip
```

Using only `-SourceRoot` retains the legacy complete-directory convention and
looks for `CmykFiles` below that root. Overrides never update the reviewed
default used by runtime deployment.
