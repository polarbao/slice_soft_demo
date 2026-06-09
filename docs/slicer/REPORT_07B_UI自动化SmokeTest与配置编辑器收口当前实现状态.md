# REPORT_07B_UI自动化SmokeTest与配置编辑器收口当前实现状态

> 实现日期：2026-06-09  
> 阶段：07B UI 自动化 Smoke Test 与配置编辑器收口  
> 状态：已完成第一版实现，并已补充 preview 伪彩图与真实 3MF 运行适配；构建、UI smoke test 与 quick regression 通过  

## 1. 阶段边界

07B 是 `slicer_debug_ui` 工程质量收口阶段，不是新切片算法阶段。

本阶段保持：

- 未修改 `slicer_core` 输出协议。
- 未修改 `schema=p0.rgbwsv.2`、`channelOrder=R G B W S V`、`bitDepth=8`、`polarity=black_is_print`。
- 未修改 `MaterialPolicy` / `MaterialRoleMapping` / `MaterialProcessProfile` 的执行语义。
- 未引入设备通信、RIP 半色调、ICC/CMYK、OpenVDB、生产任务系统或完整 3D viewport。

注意：本阶段后续补充修复修改了 preview 伪彩图显示逻辑，但没有修改生产 TIFF 的通道值、极性或协议语义。

## 2. 新增服务

新增：

- `apps/slicer_debug_ui/services/UiSmokeTestRunner.h`
- `apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp`
- `apps/slicer_debug_ui/services/PreviewReportIndex.h`
- `apps/slicer_debug_ui/services/PreviewReportIndex.cpp`
- `apps/slicer_debug_ui/services/ConfigDiffModel.h`
- `apps/slicer_debug_ui/services/ConfigDiffModel.cpp`

## 3. UI Smoke Test Mode

`main.cpp` 新增参数：

- `--ui-smoke-test`
- `--case`
- `--config`
- `--package`
- `--package-a`
- `--package-b`
- `--output`
- `--yes`

当前支持 case：

- `startup`
- `load-package`
- `save-as-config`
- `chart-load`
- `overlay-load`
- `compare-profiles`

`save-as-config` 会在 smoke test 中使用 `--yes` 自动确认覆盖。

## 4. Save 覆盖确认

`ConfigDocument` 新增：

- `SaveOptions`
- `save(QWidget*, SaveOptions)`
- `saveAs(QString, QWidget*, SaveOptions)`

当前行为：

- 交互模式下，目标文件已存在时弹出覆盖确认。
- smoke test 模式下可通过 `SaveOptions{allowOverwriteWithoutPrompt=true}` 自动覆盖。
- 保存前仍执行 `ConfigValidator`，非法配置禁止保存。

## 5. ConfigDiff

新增 `ConfigDiffPanel`，并接入 `ConfigEditorPanel` 的 `配置差异` 页签。

显示列：

- `路径`
- `原值`
- `新值`

第一版覆盖的根节点：

- `materialProcessProfile`
- `materialPolicy`
- `materialRoleMapping`
- `support`
- `preview`

编辑器变更后会刷新；Save / Revert 后会基于新的原始文档刷新。

## 6. 枚举字段收口

本阶段已将关键枚举字段改为下拉：

- `output.storageMode`：`stripped` / `tiled`
- `materialPolicy.white.mode`：`underbase` / `disabled` / `all_model`
- `materialPolicy.varnish.mode`：`top_n_layers` / `all_model` / `disabled`
- `materialRoleMapping.rules[].role`：沿用 07A 表格下拉
- `support.mode`：沿用 07A 下拉，并补齐 `bottom_projection_plus_unsupported` / `full_vertical_projection` 等核心支持枚举

## 7. PreviewReportIndex

`PreviewReportIndex` 支持标准 schema：

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
  ]
}
```

同时兼容旧字段：

- `files`
- `generated`
- `previewFiles`

`PreviewOverlayPanel` 已改为：

1. 优先使用 `PreviewReportIndex`；
2. 若报告无有效图片条目，则 fallback 到 `PackageLoader` 扫描出的 preview 文件列表；
3. 若 package 存在 `preview_report.json` 但没有 preview 图片，smoke test 允许通过 graceful-empty-preview，避免对 preview disabled 的样例误报崩溃。

## 8. Preview 伪彩图修复

运行时发现支撑、白墨、光油伪彩图使用黑色作为未打印区域时，会在 Overlay 中污染 RGB 真彩色显示。已修复为：

- 支撑打印区域默认 `rgb(0,255,0)`，未打印区域默认 `rgb(255,255,255)`。
- 光油打印区域默认 `rgb(127,127,127)`，未打印区域默认 `rgb(255,255,255)`。
- 白墨打印区域默认 `rgb(0,170,255)`，未打印区域默认 `rgb(255,255,255)`。
- `PreviewOverlayPanel` 叠加时只叠加非空伪彩图区域，白色/黑色空白区域不参与 RGB 混合。

伪彩图颜色支持配置：

```json
"preview": {
  "pseudoColors": {
    "empty": [255, 255, 255],
    "support": [0, 255, 0],
    "white": [0, 170, 255],
    "varnish": [127, 127, 127]
  }
}
```

`preview_report.json` 已补充：

- `schema = p0.preview_report.1`
- `pseudoColors`

上述修复只影响 preview 显示图，不影响生产 TIFF。

## 9. 样例配置修正

执行 quick regression 时发现多个 3MF 正向样例配置引用了不存在的旧模型路径，例如：

- `../../models/3mf/0.3.3mf`
- `../../models/3mf/0.2.3mf`
- `../../models/3mf/0.1.3mf`

已统一修正为仓库内已有文件：

- `samples/models/3mf/multi_object_transform.3mf`
- `samples/models/3mf/multi_material_rgb_white_varnish.3mf`
- `samples/models/3mf/multi_material_rgb_white_varnish_deflate.3mf`
- `samples/models/3mf/color_group_cube.3mf`
- `samples/models/3mf/mixed_basematerial_colorgroup_texture.3mf`
- 以及 single / stored / deflate / texture2d 对应样例。

这是 regression 基线修正，不涉及切片协议或算法语义。

后续又针对真实模型新增：

- `samples/configs/3mf/three_mf_real_01.json` -> `samples/models/3mf/01.3mf`
- `samples/configs/3mf/three_mf_real_02.json` -> `samples/models/3mf/02.3mf`
- `samples/configs/3mf/three_mf_real_03.json` -> `samples/models/3mf/03.3mf`

并在 VSCode 运行/调试中新增：

- `SliceSoft: Debug 3MF real 01`
- `SliceSoft: Debug 3MF real 02`
- `SliceSoft: Debug 3MF real 03`
- `SliceSoft: Run 3MF real 01`
- `SliceSoft: Run 3MF real 02`
- `SliceSoft: Run 3MF real 03`

## 10. 验证结果

已执行：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
```

结果：通过。

已执行：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

结果：通过，返回 0。

已执行：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case startup
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case load-package --package output\NailRgbWhiteVarnishTop3
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case save-as-config --config samples\configs\material_process\nail_rgb_white_varnish_top2.json --output output\ui_smoke_generated_top3.json --yes
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case chart-load --package output\NailRgbWhiteVarnishTop3
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load --package output\NailRgbWhiteVarnishTop3
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case compare-profiles --package-a output\NailRgbWhiteVarnishTop1 --package-b output\NailRgbWhiteVarnishTop3 --output output\MaterialProfileCompare_ui_smoke.json
```

结果：

- `startup`：PASS。
- `load-package`：PASS，读取到报告。
- `save-as-config`：PASS，生成 `output/ui_smoke_generated_top3.json`。
- `chart-load`：PASS，读取 `material_process_report` 的 25 层统计。
- `overlay-load`：PASS，当前 Top3 样例 preview disabled，因此通过 graceful-empty-preview。
- `compare-profiles`：PASS，生成 `output/MaterialProfileCompare_ui_smoke.json`。

已执行：

```powershell
.\scripts\run_regression.ps1 -Mode quick
```

结果：通过，输出 `Regression complete. mode=quick`。

后续补充修复后又执行：

```powershell
cmake --build build --config Debug --target slicer_cli slicer_debug_ui
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_real_01.json
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_real_02.json
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_real_03.json
.\build\Debug\rip_reader_test.exe --package output\ThreeMfReal01 --summary
.\build\Debug\rip_reader_test.exe --package output\ThreeMfReal02 --summary
.\build\Debug\rip_reader_test.exe --package output\ThreeMfReal03 --summary
.\build\Debug\slicer_cli.exe --config samples\configs\slice_config_model_0_3.json --preview-only
.\build\Debug\slicer_cli.exe --config samples\configs\material_policy\textured_rgb_white_underbase.json --preview-only
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_regression.ps1 -Mode quick
```

结果：

- `ThreeMfReal01` / `ThreeMfReal02` / `ThreeMfReal03` 均成功生成输出包。
- 三个真实 3MF 输出包均通过 `rip_reader_test --summary`。
- 抽样确认支撑 preview 仅包含 `0,255,0` 与 `255,255,255`。
- 抽样确认光油 preview 仅包含 `127,127,127` 与 `255,255,255`。
- 抽样确认白墨 preview 使用 `0,170,255`。
- quick regression 再次通过。

## 11. 当前限制

- `overlay-load` 在 `NailRgbWhiteVarnishTop3` 上仍是 graceful-empty-preview，因为该样例 `preview.enabled=false`；实际 preview PNG 颜色已通过 `0.3` 与白墨样例抽样验证。
- `ConfigDiffModel` 是第一版递归差异，不是完整 JSON Patch。
- 枚举下拉只覆盖 07B 文档要求的关键字段，其他自由文本策略字段仍保留。
- smoke test 使用服务和 widget 构造检查，不模拟鼠标点击。

## 12. 下一阶段建议

- 增加一个 `preview.enabled=true` 的 UI smoke fixture，用于自动验证实际 RGB+W/V/S overlay 图像加载。
- 为 ConfigDiff 增加过滤、复制路径、导出差异 JSON。
- 后续若进入更完整 UI 测试，可引入 Qt Test，但当前阶段不需要增加外部依赖。
