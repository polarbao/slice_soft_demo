# Verification

Prefer `.agents/docs/build-and-test.md` as the primary build/test entry. This file records the policy for selecting checks.

## Baseline Commands

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Common Gates

```powershell
.\scripts\run_ci_quick.ps1
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

## 09P Experimental Gates

```powershell
.\scripts\run_openvdb_smoke.ps1
.\scripts\run_09p_cli_experimental_tests.ps1
.\scripts\run_09p_experimental_pipeline_tests.ps1
```

## Policy

- Match verification to the changed surface.
- For docs/config-only changes, use targeted text/schema checks and `git diff --check`.
- For C++/Qt/CMake changes, include build and relevant tests.
- For UI changes, include `--self-test` and the relevant UI smoke case.
- For OpenVDB changes, use only explicitly scoped 09P experimental scripts and do not infer production safety.
- Separate static checks, build, unit tests, integration tests, packaging checks, and manual checks.
- State partial verification clearly.
- Never report unrun commands as passed.
