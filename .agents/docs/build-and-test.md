# Slice Build and Test

## Environment

```text
Windows x64
MSVC
CMake
Qt 5.15 Widgets for slicer_debug_ui
PowerShell scripts
```

## Configure

```powershell
cmake -S . -B build
```

If vcpkg is used:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

## Build

```powershell
cmake --build build --config Debug
```

UI only:

```powershell
cmake --build build --config Debug --target slicer_debug_ui
```

## Regression

```powershell
.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_regression.ps1 -Mode full
.\scripts\run_regression.ps1 -Mode heavy
```

## UI smoke

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

## RIP validation

```powershell
.\build\Debug\rip_reader_test.exe --package <package> --summary
```

## R1 refactor gate

Each meaningful refactor step must pass:

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

If preview/UI changed, also run `overlay-load-real`.
