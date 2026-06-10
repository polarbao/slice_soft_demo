# Slice R0/R1/R2 Roadmap Summary

## Current transition

```text
P0 Demo Feature Freeze
R0: architecture review and formal refactor design
R1: core module-boundary refactor
R2: config/report/test/CI consolidation
```

## R1 scope

- Establish module directories and wrapper APIs.
- Split scene/importer/pipeline/material/support/raster/output/report boundaries.
- Keep current behavior and quick regression.
- Do not implement large new strategies.

## R2 scope

- Config schema version and migration.
- Report schema consolidation.
- Unit/golden/schema/regression/UI smoke test layering.
- CI entry hardening.

## Later feature tracks

- 08 support shape/process optimization.
- 09 SDF/OpenVDB geometry kernel exploration.
- 10 RIP/device integration.
