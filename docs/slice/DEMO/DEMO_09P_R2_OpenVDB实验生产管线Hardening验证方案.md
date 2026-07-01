# DEMO_09P_R2_OpenVDB实验生产管线Hardening验证方案

> 文档版本：v0.1
> 文档状态：Formal DEMO / Stage 09P-R2
> 生成日期：2026-07-01

---

## 1. 验证目标

验证 09P-R2 是否把 09P-R1 的 experimental OpenVDB path 收束为可回归、可解释、可准入判断的 hardening 层。

---

## 2. 验证范围

```text
report schema；
productionAdmission；
topology blocker matrix；
mesh repair 前置判断；
service data contract；
downstream output contract / texture fidelity；
Qt UI report integration；
OpenVDB OFF / ON CI matrix。
```

---

## 3. 验证样例

建议准备：

```text
valid_closed_textured_obj；
obj_with_boundary_edges；
obj_with_self_intersection；
obj_with_duplicate_faces；
obj_with_opposite_duplicate_faces；
obj_with_local_winding_issue；
openvdb_unavailable_fixture；
texture_fallback_fixture。
```

---

## 4. 验证命令

文档/配置类任务：

```powershell
git status --short
git diff --check
```

Schema / CLI：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_cli_experimental_tests.ps1
```

OpenVDB experimental：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_openvdb_smoke.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1
```

UI：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

---

## 5. 完成判定

REPORT_09P_R2 必须列出：

```text
已运行命令；
未运行命令及原因；
新增/修改文件；
production 禁止事项是否保持；
当前仍不可 production-safe 的输入；
是否进入 09P-R3；
是否需要 mesh repair / admission gate 专项。
```
