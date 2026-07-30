# DEMO 13E 自动定向与诊断工作流验证方案

> 文档版本：v1.1
> 文档状态：READY
> 日期：2026-07-30

## 1. 验证目标

证明自动定向不再因浮点噪声翻面，产品高度默认值为 9 mm，诊断结论从底部迁入右侧，
同时不改变生产 TIFF/RIP。

## 2. 用例

| 用例 | 输入 | 预期 |
|---|---|---|
| ORIENT-01 | 生成式等价 X 旋转 OBJ | `rotate_x_90` |
| ORIENT-02 | 同一输入重复加载 | 姿态和 bbox 完全一致 |
| ORIENT-03 | autoOrient=false | `identity` |
| ORIENT-04 | 原始高度已低于上限 | `identity` |
| ORIENT-05 | 03/04/目标玫瑰模型 | 三者正面朝 +Z、长轴同向 |
| ORIENT-06 | 生成式横向平放甲片 | 长轴转向 Y，窄端朝 +Y |
| ORIENT-07 | Reality 五模型只读检查 | 均为 `identity_rotate_z_minus_90`，不执行批量切片 |
| ORIENT-08 | 生成式纵向甲片正负端 | 窄端位于 -Y 时转 180 度，位于 +Y 时保持 |
| ORIENT-09 | 生成式平放/竖放 Z 正反面甲片 | 外表面统一朝 +Z，模型重新落台 |
| ORIENT-10 | 真实玫瑰 02/03/04/MF 批量导入 | 四个模型长轴沿 Y 且窄端均朝 +Y |
| CONFIG-01 | 空缺 maxHeightMm | 解析值 9.0 |
| CONFIG-02 | Qt 一键生成配置 | 写入 9.0 |
| UI-01 | 默认启动 | 右侧有“预检与诊断” |
| UI-02 | Package 有 warnings | 右侧“问题”页显示 |
| UI-03 | 右侧任务详情 | 默认折叠、无“诊断”页，且与上下文检查器互斥 |
| UI-04 | 多尺寸窗口 | 无遮挡、无重复侧栏 |
| PROD-01 | 真实场景切片 | TIFF/RIP strict PASS |

## 3. 命令

```powershell
cmake --build build --config Debug --target auto_orient_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "auto_orient|model_preflight|multimodel_scene|ui" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case diagnostics-collapse --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case workbench-context-inspector
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case workspace-layout-sizes --package output\UiSmokeOverlayRgbwv
```

真实模型命令应使用独立临时输出目录，不覆盖用户已有 `runtime/slicesoft/Release/output/ui_sessions`。

## 4. 验收证据

```text
单元测试控制台输出；
三模型 selectedOrientation/bbox 表；
Qt Smoke 输出；
真实场景 manifest、model report、scene report 和 RIP summary；
git diff --check。
```

## 5. 非结论

本验证不能证明任意第三方模型的语义正面都可自动识别，也不构成真机打印验收。
