# Slice Soft Demo AI Collaboration Rules

> Project: `polarbao/slice_soft_demo` / UV 3D print slicing demo and refactor track.  
> Scope: `.agents` project-level rules for ChatGPT / Codex / Copilot / Cursor / Antigravity-style agents.  
> Language: Chinese by default.  

## 1. Project identity

This project is an industrial UV / inkjet 3D printing slicing Host Software prototype, not a generic Qt demo.

Current P0/R-track baseline includes:

```text
OBJ / MTL / Texture input
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
R0 architecture review / R1 module-boundary refactor planning
```

Fixed technical anchors:

```text
C++20
Qt 5.15 Widgets for UI only
CMake target-based
Windows x64 / MSVC
vcpkg manifest mode when dependencies are needed
RGBWSV TIFF, uint8, black_is_print
```

## 2. Mandatory evidence levels

Use the following levels when answering implementation-state questions:

```text
A = current code/config/tests; safe implementation basis
B = formal PRD/DEV/ARCH/ADR target design; direction only
C = historical chat/demo/archive; background only
D = deprecated/conflicting; do not use as implementation basis
```

If docs conflict with code, prefer current code and report the conflict.

## 3. Mandatory read order for project-level work

Before project-wide design, refactor, strategy, or implementation planning, read in this order when present:

```text
1. .agents/AGENTS.md
2. .agents/docs/SLICE_AI_SKILL_MASTER.md
3. .agents/docs/project-profile.md
4. .agents/docs/architecture-boundary.md
5. .agents/docs/build-and-test.md
6. ai_workspace/CONTEXT_INDEX.md
7. latest ai_workspace/context_handoff/*.md
8. ai_workspace/AI_WORKSPACE_TOPIC_INDEX.md
9. related ai_workspace/integrated_reports/*.md
10. docs/slicer/REPORT_*.md for current stage
11. docs/slicer/ARCH_*.md / DOC_DECISION_*.md / PRE_R0_*.md
12. related docs/slicer/PRD_*.md / DEV_*.md / DEMO_*.md / TASKS_*.md
13. current source code
```

If files are missing, state what is missing and continue with the highest available evidence.

## 4. Architecture red lines

```text
Qt must stay in apps/slicer_debug_ui and UI/view-layer code.
slicer_core and future core/importer/material/support/output modules must not depend on QString/QList/QObject/QWidget.
Importers must not write TIFF directly.
Material policy must not read files directly.
Support generation must not write report files directly.
UI must not access slicer.cpp temporary internal structures directly.
Reports must not own business decisions.
```

## 5. RGBWSV protocol red lines

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

## 6. Current architecture track

After 07B-R1, the project enters:

```text
P0 Demo Feature Freeze
R0: architecture review and formal refactor design
R1: core module-boundary refactor
R2: config/report/test/CI engineering consolidation
```

R1 must follow:

```text
wrap first
move later
rewrite last
```

Do not implement `surface_shell_texture`, `compensated_varnish`, OpenVDB, device communication, RIP halftoning, or production task scheduling during R1 unless explicitly re-scoped by the user.

## 7. Required answer format before code changes

For project code/design changes, answer in Chinese and include:

```markdown
## Implementation Plan

### Problem Type
### Layer(s) Involved
### Official Documents
### Historical Documents
### AI Workspace Evidence
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

## 8. Verification gates

At minimum for code refactor:

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

If UI/preview/overlay changed:

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

Never claim validation passed unless it was actually run in the current session or explicitly reported by an A-level file.

## 9. Chat saves and handoff

Use:

```text
ai_workspace/<model>/chat_logs/YYYY-MM-DD.md
ai_workspace/<model>/analysis_reports/
ai_workspace/context_handoff/
ai_workspace/integrated_reports/
```

Do not save secrets, tokens, credentials, cookies, or private host-specific paths.
