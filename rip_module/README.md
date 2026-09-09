# SliceSoft RIP module

This directory contains the tracked configuration and deployment metadata for
the external RIP integration. The ignored `rip_project` directory is the local
SDK input; `scripts/PackageRipModule.ps1` produces the relocatable runtime
directory. `source.json` pins the reviewed SDK to
`rip_project/RIPDLL_20260909`. Older SDKs remain in `rip_project/RIPDLL`;
adding a future `RIPDLL_YYYYMMDD` directory does not change the selected SDK.
Update the pin only after reviewing the SDK interfaces, resources and tests.
Module version 1.2.0 requires both `--transparent <0-4>` and `--ripmode <0|1>`.
The entire DLL/EXE, ICC, linearization and matrix payload is copied together.
The generated inventory records each payload file's SHA-256, and
`source_provenance.json` records its source directory.

The generated module is for local engineering use only. External distribution
remains blocked until the RIP binary, lcms2, ICC profiles and private LibTIFF
provenance and license evidence are supplied.

```powershell
./scripts/PackageRipModule.ps1 `
  -Destination ./output/ripflow/modules/rip

./scripts/TestRipModulePackage.ps1 `
  -ModuleDirectory ./output/ripflow/modules/rip
```

`-SourceRoot ./rip_project/RIPDLL_YYYYMMDD` is an explicit engineering override;
it does not update the reviewed default used by runtime deployment.
