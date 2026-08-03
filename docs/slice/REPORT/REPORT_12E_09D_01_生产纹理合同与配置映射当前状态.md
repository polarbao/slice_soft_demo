# REPORT_12E-09D-01 生产纹理合同与配置映射当前状态

> 状态：COMPLETE
> 日期：2026-08-03
> 下一任务：12E-09D-02 READY

## 1. 目标

在实现生产控件和 Effective Config 写回前，先冻结 Legacy、Global 与诊断三类纹理宽度语义，
防止诊断参数被误写入生产配置，并为后续设置模型提供稳定 DTO、字段路径和错误码。

## 2. 已实现

新增 `ProductionTextureSettingsContract`：

```text
LegacyTopBand：texture.topSurfaceLayers，单位 layers；
GlobalSurfaceShell：texture.surfaceShell.widthMm + mode，单位法向距离 mm；
DiagnosticOnly：没有生产 applyMode/value/mode 写入路径；
Unsupported：默认 fail closed。
```

新增 `ProductionTextureControlState`，冻结 requested/effective、backend、editable、lockReason、stale、
valid、issues 和 errorCode 等后续服务/UI 所需状态，但本任务不负责写回生产配置。

稳定错误码包括：

```text
E_PRODUCTION_TEXTURE_UNSUPPORTED_STRATEGY
E_PRODUCTION_TEXTURE_INVALID_TOP_LAYERS
E_PRODUCTION_TEXTURE_INVALID_WIDTH
E_PRODUCTION_TEXTURE_INVALID_PARTITION_MODE
E_PRODUCTION_TEXTURE_PROFILE_LOCKED
E_PRODUCTION_TEXTURE_GLOBAL_NOT_ADMITTED
```

## 3. 验证

实际执行：

```powershell
cmake --build build-slicesoft/main --config Debug --target production_texture_settings_contract_unit_tests
ctest --test-dir build-slicesoft/main -C Debug -R "^(production_texture_settings_contract_unit_tests|production_effective_config_unit_tests)$" --output-on-failure
```

结果：2/2 PASS。

## 4. 当前边界

```text
本任务未增加 Qt 控件；
未修改 Effective Config 输出；
未把诊断宽度转换为生产参数；
未实现单材料 W/V Resolver；
未修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print；
未实现或解冻 12G-TCWS。
```

## 5. 下一步

`12E-09D-02` 在本合同上实现 `ProductionTextureSettingsModel`，读取 Profile requested/effective
状态，计算 Legacy 等效 Z 厚度，并维护 Global 显式 partial_shell/all_texture 模式。
