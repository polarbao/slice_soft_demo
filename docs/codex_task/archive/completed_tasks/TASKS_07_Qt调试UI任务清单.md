# TASKS_07_Qt调试UI任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：PRD_07 / DEV_07  
> 建议提交目录：`docs/slicer/`

---

## Milestone 07-0：阅读确认

- [x] 阅读 `REPORT_05A_真实材料工艺参数验证当前实现状态.md`
- [x] 阅读 `DOC_DECISION_07_REPORT05A后进入Qt调试UI阶段.md`
- [x] 阅读 `PRD_07 / DEV_07 / DEMO_07`
- [x] 确认不改 slicer_core 输出协议
- [x] 确认第一版是 debug UI，不是生产 UI

---

## Milestone 07-1：工程结构

- [x] 新增 `apps/slicer_debug_ui`
- [x] 新增 Qt CMake target
- [x] Qt 不存在时 CLI 构建不被破坏
- [x] MainWindow 空壳可启动

---

## Milestone 07-2：ProcessRunner

- [x] QProcess 异步执行
- [x] stdout/stderr 捕获
- [x] exit code 捕获
- [x] duration 统计
- [x] E_* 错误码高亮

---

## Milestone 07-3：Config / Package Panel

- [x] 选择 config.json
- [x] 选择 output package
- [x] 显示 repo root
- [x] 显示 tool path
- [x] 打开输出目录

---

## Milestone 07-4：Run Panel

- [x] Build Debug
- [x] Run Slicer
- [x] Run RIP Summary
- [x] Run Quick Regression
- [x] Compare Profiles

---

## Milestone 07-5：Report Viewer

- [x] manifest raw JSON
- [x] slice_report raw JSON
- [x] material_process_report summary
- [x] material_policy_report summary
- [x] texture_report summary
- [x] three_mf_report summary
- [x] warnings / failures list

---

## Milestone 07-6：Preview Viewer

- [x] 扫描 preview 目录
- [x] 支持 PNG
- [x] 支持 PPM
- [x] layer slider
- [x] zoom / fit
- [x] channel selector

---

## Milestone 07-7：MaterialProcessPanel

- [x] profileName
- [x] target
- [x] RGB/W/V/S printPixels
- [x] coverageRatio
- [x] V activeLayerIndices
- [x] missingUnderbasePixels
- [x] validation.pass
- [x] validation.failures

---

## Milestone 07-8：Profile Compare

- [x] 选择 Package A
- [x] 选择 Package B
- [x] 调用 compare_material_profiles.ps1
- [x] 显示 delta
- [x] 显示 changedLayers

---

## Milestone 07-9：验证与报告

- [x] UI demo 样例通过
- [x] run_regression.ps1 -Mode quick 仍通过
- [x] 生成 `REPORT_07_Qt调试UI当前实现状态.md`
