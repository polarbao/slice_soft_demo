# 12C-R1-03 Generated Effective Config 交接

> 日期：2026-07-13
> 状态：COMPLETE
> 下一原子任务：12C-R1-04 设置项中文帮助元数据

## 1. 已完成

```text
新增 EffectiveConfigGenerator；
输入为只读 Profile template、内存 ConfigDocument override 和 SliceSettingsState；
输出为 session/slice_config.generated.json；
SliceSettingsModel 与 ConfigValidator 均通过后才使用 QSaveFile 原子写入；
运行切片和 legacy 一键切片改用 generated config；
dirty UI 配置不再被忽略，原 template/fixture 不写回；
配置页新增“生效配置”摘要和全字段差异；
常用设置新增模型内部填充、支撑位置、内部镂空开关和最小面积；
相对模型路径按原模板目录解析为绝对路径后写入 session config；
固定协议偏差 bitDepth/channelOrder/background 在 UI 校验阶段阻断。
```

## 2. 字段范围

```text
input.modelPath；
output.packageDir / layerThicknessMm；
modelFill；
support placement / upper / internalVoid；
surfaceVarnish；
outerVarnish；
preview；
experimental.openvdbPipeline engine role。
```

## 3. 安全边界

```text
未修改 slicer_core 和切片算法；
未修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
未默认启用 OpenVDB；
普通切片保持 legacy；
OpenVDB generated config 仅用于 utility/candidate 诊断且 writeProductionRgbwsv=false；
未实现 12D 材料闭环算法。
```

## 4. 验证入口

```powershell
cmake --build build-12c-ui --config Debug --target slicer_debug_ui
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scenario-registry
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case slice-settings-model
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case generated-effective-config
```

## 5. 下一任务边界

R1-04 只集中设置项中文帮助元数据，并复用到 tooltip/说明面板；不得提前进入 R2 PreviewWorkspace，也不得修改生成链路或协议。
