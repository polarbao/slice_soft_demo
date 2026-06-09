# TASKS_07A_v0.2_基于当前代码的修订任务清单

> 文档版本：v0.2  
> 文档状态：Codex Task List / 修订版  
> 适用阶段：07A  
> 建议提交目录：`docs/slicer/`

---

## Milestone 07A-0：基线确认

- [x] 保留现有 `apps/slicer_debug_ui` target。
- [x] 保留现有 `MainWindow`。
- [x] 保留现有 `ProcessRunner`。
- [x] 保留现有 `PackageLoader`。
- [x] 保留现有 `ReportLoader`。
- [x] 保留现有 `PreviewPanel`。
- [x] 保留现有 `ReportPanel`。
- [x] 保留现有 `MaterialProcessPanel`。
- [x] 保留现有 `LogPanel`。

---

## Milestone 07A-1：CMake 增量更新

- [x] 不重新创建 `slicer_debug_ui` target。
- [x] 只向 `apps/slicer_debug_ui/CMakeLists.txt` 增加新源文件。
- [x] 保持 Qt 缺失时 CLI 构建不受影响。

---

## Milestone 07A-2：ConfigDocument

- [x] 新增 `services/ConfigDocument.*`
- [x] 加载 JSON config。
- [x] 保留未知字段。
- [x] 支持 get/set helper。
- [x] 支持 dirty 状态。
- [x] 支持 Save As。
- [ ] Save 覆盖前确认。
- [x] 非法 JSON 禁止保存。

---

## Milestone 07A-3：Editor Panels

- [x] 新增 `ConfigEditorPanel`。
- [x] 新增 `MaterialProcessProfileEditor`。
- [x] 新增 `MaterialPolicyEditor`。
- [x] 新增 `MaterialRoleMappingEditor`。
- [x] 新增 `SupportEditor`。
- [x] 编辑器只修改 config JSON，不直接改 slicer_core。

---

## Milestone 07A-4：MainWindow 集成

- [x] Center tabs 增加 `Config`。
- [x] Center tabs 增加 `Charts`。
- [x] Center tabs 增加 `Overlay`。
- [ ] Right tabs 可增加 `Profile Editor`。
- [x] 不破坏原 Preview / Reports / Material / Warnings / Compare / Log。

---

## Milestone 07A-5：ChannelChartPanel

- [x] 新增 `ChannelChartPanel`。
- [x] 读取 `material_process_report.json`。
- [x] 绘制 per-layer RGB/W/V/S printPixels。
- [x] 支持 channel checkbox。
- [x] 支持 hover layer index。
- [x] 第一版用 QPainter，不引入 Qt Charts。

---

## Milestone 07A-6：PreviewOverlayPanel

- [x] 新增 `PreviewOverlayPanel`。
- [x] 优先读取 `preview_report.json` metadata。
- [x] fallback 到当前 preview 文件名 token 分类。
- [x] 支持 RGB + W overlay。
- [x] 支持 RGB + V overlay。
- [x] 支持 RGB + S overlay。
- [x] 支持 layer slider / zoom / fit。

---

## Milestone 07A-7：Self-test

- [x] `--self-test` 初始化 ConfigDocument。
- [x] `--self-test` 初始化 editor panels。
- [x] `--self-test` 初始化 chart panel。
- [x] `--self-test` 初始化 overlay panel。
- [x] 不进入 event loop。
- [x] 返回 0 表示 UI 初始化通过。

---

## Milestone 07A-8：验证

- [ ] UI 可启动。
- [ ] Save As 生成新 config。
- [ ] 修改 topLayers 后运行 slicer 成功。
- [ ] Chart 显示 V active layers 差异。
- [ ] Overlay 显示 RGB+V / RGB+S。
- [ ] Compare Profiles 保持可用。
- [x] `run_regression.ps1 -Mode quick` 通过。
- [x] 生成 `REPORT_07A_Qt参数编辑与Profile可视化当前实现状态.md`。
