# TASKS_07B_R1_UI真实OverlaySmoke修正任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：07B-R1  
> 建议提交目录：`docs/slicer/`

---

## Milestone 07B-R1-0：阅读确认

- [x] 阅读 `REPORT_07B_UI自动化SmokeTest与配置编辑器收口当前实现状态.md`
- [x] 阅读 `DOC_DECISION_07B_R1_UI真实OverlaySmoke与预览Fixture修正.md`
- [x] 确认不修改 slicer_core 输出协议
- [x] 确认不修改材料策略执行语义
- [x] 确认本阶段只是 UI smoke test 与 fixture 修正

---

## Milestone 07B-R1-1：新增 preview-enabled smoke fixture

新增或复用一个轻量配置：

```text
samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json
```

要求：

```text
preview.enabled = true
preview.channels 至少包含 rgb / white / varnish / support 中的两个以上
materialProcessProfile.enabled = true
输出包路径固定，例如 output/UiSmokeOverlayRgbwv
```

输入模型可优先复用已有小 fixture：

```text
samples/models/3mf/color_group_cube.3mf
samples/models/3mf/texture2d_checker_cube.3mf
samples/models/3mf/mixed_basematerial_colorgroup_texture.3mf
```

如需要 W/V/S 同时存在，可使用已有 material_process 样例并打开 preview。

完成状态：

- [x] 已新增 `samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json`
- [x] `preview.enabled = true`
- [x] `preview.channels` 覆盖 `rgb` / `white` / `varnish` / `support`
- [x] `materialProcessProfile.enabled = true`
- [x] 输出包路径固定为 `output/UiSmokeOverlayRgbwv`

---

## Milestone 07B-R1-2：新增真实 overlay smoke case

在 `UiSmokeTestRunner` 中新增：

```text
overlay-load-real
```

行为：

```text
1. 如果 package 不存在，可先提示用户先运行 slicer_cli，或在 smoke test 中调用 slicer_cli 生成；
2. 加载 package；
3. 确认 PreviewOverlayPanel.imageCount() > 0；
4. 尝试切换 RGB+W / RGB+V / RGB+S；
5. 确认 composeCurrent 或等价内部状态非空；
6. 返回 0。
```

如果当前 `PreviewOverlayPanel` 没有公开必要只读状态，可增加测试专用 query 方法：

```cpp
int imageCount() const;
bool canComposeMode(const QString& mode) const;
QStringList availableChannels() const;
```

完成状态：

- [x] 已新增 `overlay-load-real`
- [x] 已公开 `imageCount()` / `availableChannels()` / `canComposeMode(...)`
- [x] 已验证 `RGB + W 白墨` / `RGB + V 光油` / `RGB + S 支撑` 可合成

---

## Milestone 07B-R1-3：PreviewReport schema 强化

确保 `preview_report.json` 对新 fixture 输出：

```json
{
  "schema": "p0.preview_report.1",
  "files": [
    {
      "path": "preview/model_rgb_000001.png",
      "channel": "rgb",
      "layerIndex": 1,
      "kind": "single"
    }
  ],
  "pseudoColors": {
    "empty": [255, 255, 255],
    "support": [0, 255, 0],
    "white": [0, 170, 255],
    "varnish": [127, 127, 127]
  }
}
```

并确认 `PreviewReportIndex` 能读到：

```text
path
channel
layerIndex
kind
```

完成状态：

- [x] `preview_report.schema = p0.preview_report.1`
- [x] `files[].path` / `files[].channel` / `files[].layerIndex` / `files[].kind` 已输出并可读
- [x] `pseudoColors` 已输出

---

## Milestone 07B-R1-4：ConfigDiff 小增强，非阻塞

可选增强：

```text
1. ConfigDiffPanel 增加 Copy Path；
2. ConfigDiffPanel 增加 Export Diff JSON；
3. ConfigDiffModel 增加按根节点过滤。
```

如果时间不足，这一项可延期，不阻塞进入 08。

完成状态：

- [ ] `Copy Path` 未做，按本阶段非阻塞项延期
- [ ] `Export Diff JSON` 未做，按本阶段非阻塞项延期
- [ ] 根节点过滤未做，按本阶段非阻塞项延期

---

## Milestone 07B-R1-5：验证

必须执行：

```powershell
cmake --build build --config Debug --target slicer_debug_ui

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test

.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv

.\scripts\run_regression.ps1 -Mode quick
```

完成状态：

- [x] `cmake --build build --config Debug --target slicer_debug_ui`
- [x] `.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test`
- [x] `.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json`
- [x] `.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv`
- [x] `.\scripts\run_regression.ps1 -Mode quick`

---

## Milestone 07B-R1-6：状态报告

完成后生成：

```text
docs/slicer/REPORT_07B_R1_UI真实OverlaySmoke与配置编辑器小收口当前实现状态.md
```

报告必须说明：

```text
1. 新增 preview-enabled fixture；
2. overlay-load-real 验证结果；
3. preview_report schema 验证结果；
4. 是否增强 ConfigDiff；
5. quick regression 结果；
6. 是否建议进入 08。
```

完成状态：

- [x] 已生成 `docs/slicer/REPORT_07B_R1_UI真实OverlaySmoke与配置编辑器小收口当前实现状态.md`
