# REPORT_07B_R1_UI真实OverlaySmoke与配置编辑器小收口当前实现状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-09  
> 适用阶段：07B-R1

---

## 1. 阶段目标与边界

07B-R1 是 07B 之后、进入 08 之前的小收口阶段，目标是补齐 UI 预览真实包的 overlay smoke 验证能力，并修正 preview fixture 覆盖不足的问题。

本阶段不修改生产切片协议，不修改 MaterialPolicy 语义，不修改 RoleMapping/Profile 语义，也不引入新的切片算法、设备联调、RIP 半色调、ICC、OpenVDB 或 3D 视图。

生产输出协议仍保持：

```text
schema = p0.rgbwsv.2
channels = R G B W S V
bitDepth = 8
polarity = black_is_print
0 = 打印
255 = 不打印
```

---

## 2. 新增 preview-enabled fixture

已新增 UI smoke 专用配置：

```text
samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json
```

该配置固定输出：

```text
output/UiSmokeOverlayRgbwv
```

配置特征：

- `preview.enabled = true`
- `preview.format = png`
- `preview.interval = 1`
- `preview.channels = ["rgb", "white", "varnish", "support"]`
- `materialProcessProfile.enabled = true`
- 启用 RGB、白墨、光油与底部投影支撑
- `pseudoColors.empty = [255, 255, 255]`
- `pseudoColors.support = [0, 255, 0]`
- `pseudoColors.white = [0, 170, 255]`
- `pseudoColors.varnish = [127, 127, 127]`

该 fixture 的用途是为 UI overlay 预览提供真实的 preview PNG 与 `preview_report.json`，避免 smoke test 只覆盖空窗口或无 preview 的 package。

---

## 3. overlay-load-real 验证

`UiSmokeTestRunner` 已新增：

```text
--ui-smoke-test --case overlay-load-real
```

该用例当前执行以下检查：

- package 目录存在；
- manifest 存在；
- `reports/preview_report.json` 可读取；
- `preview_report.schema = p0.preview_report.1`；
- `files[]` 中存在 `path` / `channel` / `layerIndex` / `kind`；
- `PreviewOverlayPanel` 可加载真实 package；
- `imageCount() > 0`；
- 可读到归一化后的 preview channel；
- 能够合成 `RGB + W 白墨`、`RGB + V 光油`、`RGB + S 支撑` 中至少一种 overlay。

本次验证结果：

```text
PASS overlay-load-real images=94 channels=rgb,support,varnish,white modes=RGB + W 白墨,RGB + V 光油,RGB + S 支撑
```

---

## 4. PreviewReport schema 验证

本阶段补充了 preview report 中单通道文件的 `kind` 字段，使 `files[]` 至少包含：

```json
{
  "path": "preview/support_s_000000.png",
  "channel": "support",
  "layerIndex": 0,
  "kind": "single"
}
```

当前真实输出验证结果：

```text
schema = p0.preview_report.1
files = 47
files[].path / channel / layerIndex / kind 存在
pseudoColors 存在
```

---

## 5. PreviewOverlayPanel 增强

`PreviewOverlayPanel` 已增加测试所需的只读 query 能力：

```cpp
int imageCount() const;
QStringList availableChannels() const;
bool canComposeMode(const QString& mode) const;
```

同时增加 preview channel 归一化逻辑，将历史或不同来源的名称映射到 UI overlay 使用的稳定名称：

```text
texture_rgb / model_rgb / true_rgb -> rgb
w -> white
v -> varnish
s -> support
```

该改动只用于 UI 预览加载和 smoke test 查询，不改变生产 TIFF 输出或切片材料写入规则。

---

## 6. ConfigDiff 小增强状态

07B-R1 文档中 `ConfigDiffPanel` 的以下能力属于非阻塞增强：

```text
Copy Path
Export Diff JSON
根节点过滤
```

本阶段未实现上述 ConfigDiff 可选项，原因是 07B-R1 的阻塞点是 preview fixture 与真实 overlay smoke 缺失；ConfigDiff 小增强不影响进入 08，可在后续 UI 易用性收口中继续处理。

---

## 7. 验证结果

已执行并通过：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
cmake --build build --config Debug --target slicer_cli
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
.\scripts\run_regression.ps1 -Mode quick
```

切片 fixture 输出摘要：

```text
packageDir = output/UiSmokeOverlayRgbwv
grid = 48 x 24 x 25
modelPixels = 22560
supportPixels = 5640
```

`run_regression.ps1 -Mode quick` 已完成，结果为：

```text
Regression complete. mode=quick
```

---

## 8. 当前限制

- `overlay-load-real` 是 widget/service 层真实包加载与合成验证，不是鼠标点击级 UI 自动化。
- ConfigDiff 的 Copy Path、Export Diff JSON、根节点过滤尚未实现。
- 本阶段不验证显示器色彩管理、ICC、RIP 半色调或设备输出效果。

---

## 9. 是否建议进入 08

建议进入 08。

理由：

- 07B 阶段的 UI self-test 仍然通过；
- 真实 preview package 可由 CLI 生成；
- UI smoke 已能加载真实 preview package；
- `RGB + W`、`RGB + V`、`RGB + S` overlay 合成已通过；
- `preview_report.schema = p0.preview_report.1` 与必要字段已验证；
- quick regression 已通过。

后续 08 可在当前基础上进入支撑形态、材料工艺或 UI 交互体验的下一阶段强化。
