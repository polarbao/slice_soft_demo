---
name: slice-build
description: Use for slice_soft_demo build configuration, CMake target changes, Qt build issues, dependency choices, packaging, CI, compiler errors, and test command troubleshooting.
---

# Slice Build Skill

Read `.agents/docs/build-and-test.md` first.

When changing dependencies:

1. Compare at least 2 options.
2. Explain CMake/vcpkg changes.
3. Explain deployment/runtime impact on Windows/MSVC.
4. Provide verification commands.

Default gates:

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

If UI/preview changes, also run `overlay-load-real`.
