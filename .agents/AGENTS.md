# Slice Soft Demo AI Collaboration Rules

> Project: `polarbao/slice_soft_demo` / UV industrial inkjet 3D printing slicer.
> Scope: `.agents` project-level rules for ChatGPT / Codex / Copilot / Cursor / Antigravity-style agents.
> Language: Chinese by default.

## 1. Project Identity

This project is an industrial UV / inkjet 3D printing slicing Host Software prototype moving from demo to formal product planning. It is not a generic Qt demo.

Current implementation baseline includes:

```text
OBJ / STL / MTL / Texture input
3MF stored / deflate input
3MF BaseMaterial / ColorGroup / Texture2DGroup
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
Support / SupportType / island diagnostics
RGBWSV TIFF output package
RIP reader strict validation
Reports / manifests / preview images
Qt Debug UI and UI smoke tests
OpenVDB optional experimental surface-shell path
ProductionAdmissionPolicy / strict geometry diagnostics
Schema / golden / regression scripts
Stage 12E global texture/fill partition width sweep and diagnostic report
```

Fixed technical anchors:

```text
C++20
Qt 5.15 Widgets for UI only
CMake target-based
Windows x64 / MSVC
vcpkg manifest mode when dependencies are needed
RGBWSV TIFF, uint8, black_is_print
OpenVDB optional and disabled by default
```

## 2. Mandatory Evidence Levels

Use the following levels when answering implementation-state questions:

```text
A = current code/config/tests/build scripts; safe implementation basis
B = formal docs/slice PRD/DEV/ARCH/ADR target design; direction only
C = historical chat/demo/archive/completed Codex tasks; background only
D = deprecated/conflicting; do not use as implementation basis
```

If docs conflict with code, prefer current code and report the conflict.

## 3. Mandatory Read Order

Before project-wide design, refactor, strategy, or implementation planning, read in this order when present:

```text
1. AGENTS.md
2. .agents/AGENTS.md
3. .agents/docs/SLICE_AI_SKILL_MASTER.md
4. .agents/docs/project-profile.md
5. .agents/docs/architecture-boundary.md
6. .agents/docs/build-and-test.md
7. .agents/docs/doc-state.md
8. docs/slice/README.md
9. docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md
10. related docs/slice/PRD/PRD_*.md / docs/slice/DEV/DEV_*.md / docs/slice/ROADMAP/ROADMAP_*.md / docs/slice/DOC/DOC_*.md
11. docs/codex_task/README.md
12. related docs/codex_task/current/*.md
13. related docs/archive/2026-06-30_slicer_legacy/**/*.md as historical evidence
14. current source code and tests
```

If files are missing, state what is missing and continue with the highest available evidence.

## 4. Architecture Red Lines

```text
Qt must stay in apps/slicer_debug_ui and UI/view-layer code.
slicer_core and future core/importer/material/support/output modules must not depend on QString/QList/QObject/QWidget.
Importers must not write TIFF directly.
Material policy must not read files directly.
Support generation must not write report files directly.
UI must not access slicer.cpp temporary internal structures directly.
Reports must not own business decisions.
Geometry diagnostics must feed admission policy, not silently downgrade strict failures.
Experimental OpenVDB pipeline must not replace legacy production path.
```

## 5. RGBWSV Protocol Red Lines

Do not change without explicit architecture decision:

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType only in metadata/report/debug; never encoded as a TIFF channel value
```

## 6. Current Architecture Track

Historical R-track:

```text
R0: architecture review and formal refactor design
R1: core module-boundary refactor
R2: config/report/test/CI engineering consolidation
```

Historical 09P track:

```text
09P-R1: OpenVDB experimental production-pipeline access, historical/completed evidence.
09P-R2: docs governance, formalization readiness, OpenVDB experimental hardening, schema/golden/report checks.
```

Current Stage 12 track:

```text
12A: material fill, support, and varnish semantics, current P0/P1 scope complete.
12B: benchmark, legacy optimization, and OpenVDB SDF utility positioning complete.
12C: R0/R1/R2 Qt workbench closure complete.
12D: R0/R1/R2/R3 complete; 12D-10 real-model validation passed on three OBJ fixtures.
12E: 12E-01/02/03/04/05/06/07, 12E-08A/08B/08C, 12E-08C-R1-01..04, R2-01..04, and R3-01..04 complete. R3-04 records the historical `NO-GO / FROZEN`. R4-01..07, R4-07-R1, R4-07-R2, Quick-CI-R1, and R4-08-R2 are complete; xiao_ma/yecan two-family candidate evidence, four-case Release/closure/legacy/RIP gates, the versioned reference-machine candidate budget, and current Quick CI pass. R4-08-R2 is `GO` after explicit authorization. aishen/meigui/titian remain a 0/3 complex-relief coverage gap. 12E-09A-01..06 and 12E-08D-01..06 are complete. Restricted RGB+W and lower-S/internal-void-S/surface-outer-V material-parity Profiles pass the 0.01 mm TIFF/RIP gate as explicit candidates. Legacy remains default because Global total time is 4.82x-8.59x and peak memory is 8.19x-8.74x higher in the current matrix. `03D-01..03` are complete; `03D-04` is ready for LibTIFF tiled/error-model work. `12E-09D` is prepared after 03D, and 12E-10A follows both.
12G-TCWS: remains FROZEN. Current RIP facts allow one full-RGB six-channel package to drive transparent or opaque-white output and use white-region `WSV=000` as a private signal, but that byte pattern conflicts with physical `black_is_print` semantics. Do not implement or unfreeze it until the downstream contract and collision policy are decided. Texture underbase is out of scope.
13: 13A/13B/13C and inserted 13B-08/13D/13E are complete. 13E freezes deterministic standard-nail front-up orientation (`rotate_x_90`, scene `+Z`), product `maxHeightMm=9`, right-side preflight/diagnostics, and mutually exclusive right-side task details.
```

R0/R1/R2 principles still apply to refactors:

```text
wrap first
move later
rewrite last
```

OpenVDB and surface-shell work is allowed only inside explicitly scoped tasks. OpenVDB must stay optional and disabled by default. The documented 12E-08D target may connect an admitted global_surface_shell pipeline to the shared production RGBWSV writer, but only after repair/strict/Release gates and explicit user authorization; no silent fallback is allowed.

## 7. Required Answer Format Before Code Changes

For project code/design changes, answer in Chinese and include:

```markdown
## Implementation Plan

### Problem Type
### Layer(s) Involved
### Official Documents
### Historical Documents
### Current Code Reality
### Current State
### Target State
### Historical State
### Pending Confirmation
### Risk Points
### Files To Change
### Verification Plan
```

For large changes, stop for user confirmation before implementation.

## 8. Verification Gates

At minimum for code refactor:

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

If UI/preview/overlay changed:

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

For OpenVDB experimental work, use the 09P-specific scripts listed in `.agents/docs/build-and-test.md`.

Never claim validation passed unless it was actually run in the current session or explicitly reported by an A-level file.

## 9. Chat Saves And Handoff

Use:

```text
ai_workspace/<model>/chat_logs/YYYY-MM-DD.md
ai_workspace/<model>/analysis_reports/
ai_workspace/context_handoff/
ai_workspace/integrated_reports/
```

Do not save secrets, tokens, credentials, cookies, or private host-specific paths.

## 10. Commit Message Contract

New commits must use:

```text
type(scope): 【功能分类】中文摘要
```

The body must use Chinese audit items such as `【模块】`, `【验证】`, and
`【边界】`. Follow `.agents/docs/commit-style.md`. Do not rewrite published
remote history solely to restyle old commit messages.
