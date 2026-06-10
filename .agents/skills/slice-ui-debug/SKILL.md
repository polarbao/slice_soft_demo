---
name: slice-ui-debug
description: Use for apps/slicer_debug_ui changes, Qt debug UI, ConfigEditor, PreviewOverlayPanel, ChannelChartPanel, UiSmokeTestRunner, and UI smoke test issues.
---

# Slice Qt Debug UI

Read:

- `apps/slicer_debug_ui/*`
- `.agents/docs/build-and-test.md`
- Relevant `REPORT_07*.md` docs

Rules:

- Qt stays in UI app only.
- UI uses QProcess/report/package services; it must not couple to slicer.cpp internals.
- Do not block the UI thread with long-running commands.
- Surface stdout/stderr/exit codes.
- Keep `--self-test` and `--ui-smoke-test` working.

Verification:

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```
