# DEMO_11_UI切片层预览交互配置验证方案

> 文档版本：v0.1
> 文档状态：Formal DEMO / Stage 11
> 生成日期：2026-06-30

---

## 1. 验证目标

验证 11 阶段是否能把切片输出以 UI 可理解方式展示出来：

```text
按层滑动；
伪彩显示；
通道切换；
常用配置交互修改；
报告/诊断联动；
多模型能力边界可解释。
```

---

## 2. 验证样例

建议准备：

```text
single_model_rgb_texture
single_model_white_varnish_support
experimental_surface_shell_report
bad_texture_fallback
multi_model_scene_fixture_for_decision_only
```

多模型 fixture 在 11 阶段只用于能力评估和报告，不作为 production 输出承诺。

---

## 3. UI 验证场景

### 3.1 Layer Slider

```text
打开 package；
读取 layerCount；
滑动到首层、中间层、末层；
显示 layerIndex / z / stats；
图像不为空；
切换层时布局不跳动。
```

### 3.2 Pseudo Color

```text
切换 RGB composite；
切换 W heat；
切换 S mask；
切换 V mask；
切换 diagnostic overlay；
检查图例和统计摘要一致。
```

### 3.3 Interactive Config

```text
打开配置面板；
修改 layer height；
修改 material profile；
切换 support enable；
显式切换 experimental OpenVDB；
运行 validate；
显示 normalized config 或错误信息。
```

### 3.4 Multi-Model Decision

```text
加载多模型 fixture；
展示 model list；
展示 modelId / instanceId / transform；
生成 capability report；
不生成 production 多模型输出。
```

---

## 4. 验证命令

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
git diff --check
```

如 layer preview fixture 尚未实现，应在 REPORT_11 中明确标记未运行原因。
