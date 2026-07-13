# 12C-R1-02 SliceSettingsModel 交接

> 日期：2026-07-13
> 状态：COMPLETE
> 下一原子任务：12C-R1-03 Generated Effective Config

## 1. 已完成

```text
新增 apps/slicer_debug_ui/services/SliceSettingsModel.h/.cpp；
设置状态完全独立于 QWidget/QObject；
覆盖模型、输出、层高、模型填充、支撑、表面/外侧光油、预览和引擎角色；
四个稳定 Profile 已具有可验证的默认设置；
白墨 Profile 默认 ModelFillMaterial::White；
光油 Profile 默认 ModelFillMaterial::Varnish；
外侧光油默认关闭且厚度 0 mm；
内部镂空支撑默认开启；
默认引擎为 LegacyProduction；
OpenVDB 仅为 OpenVdbUtilityCandidate。
```

## 2. 验证

```powershell
cmake --build build-12c-ui --config Debug --target slicer_debug_ui
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case slice-settings-model
```

结果：

```text
PASS slice-settings-model profiles=4 legacy-default=true openvdb=candidate-only
```

## 3. R1-03 必须完成

```text
从 ScenarioRegistry 取得 Profile template；
将 SliceSettingsState 映射到配置字段；
写入 session 目录下 generated effective config；
调用 ConfigValidator；
只在校验通过后运行 slicer_cli；
不覆盖原始 template/fixture；
UI 显示 effective summary 和差异。
```

## 4. 安全边界

```text
未修改 slicer_core；
未修改切片算法和 RGBWSV 协议；
未默认启用 OpenVDB；
未让 experimental path 写生产包；
未实现 12D material closure。
```
