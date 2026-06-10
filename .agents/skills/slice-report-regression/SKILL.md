---
name: slice-report-regression
description: Use for slice_soft_demo reports, manifest, preview_report, material_process_report, regression scripts, RIP reader validation, bad package tests, and CI/test-layering decisions.
---

# Slice Report and Regression

Read:

- `.agents/docs/build-and-test.md`
- Relevant report schema docs
- `scripts/run_regression.ps1`
- `rip_reader_test` source when needed

Rules:

- Keep existing report filenames compatible unless migration is documented.
- Report schema changes require regression updates.
- `preview_report.schema = p0.preview_report.1` for the current preview index path.
- Do not treat UI smoke tests as real printing validation.

Verification examples:

```powershell
.\scripts\run_regression.ps1 -Mode quick
.\build\Debug\rip_reader_test.exe --package <package> --summary
.\build\Debug\rip_reader_test.exe --package <bad-package> --expect-error --expect-code <E_CODE>
```
