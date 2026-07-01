# Slice R0/R1/R2 Roadmap Summary

## Historical transition

```text
Historical P0 demo feature-freeze baseline
R0: architecture review and formal refactor design
R1: core module-boundary refactor
R2: config/report/test/CI consolidation
```

This R-track is historical C-level planning unless a current `docs/slice` document or current code promotes a decision.

## Current 09P transition

```text
09P-R1: OpenVDB experimental production-pipeline access, completed historical evidence.
09P-R2: formalization pre-work, docs governance, experimental OpenVDB hardening, schema/golden/report checks.
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
- 09P OpenVDB experimental surface-shell pipeline hardening.
- 10 slicing output contract / texture fidelity handoff.
- 11 UI layer preview / interactive config / multi-model capability decision.
