# REPORT_12E-09D-03 单材料浮雕材质解析当前状态

> 状态：COMPLETE
> 日期：2026-08-03
> 下一任务：12E-09D-04 READY

## 1. 已实现

新增 `SingleMaterialReliefResolver`，将单材料浮雕的白墨/光油选择解析为一个不可拆分的生产字段组：

```text
白墨：materialChannel=W，whiteValue=0，varnishValue=255；
光油：materialChannel=V，whiteValue=255，varnishValue=0；
RGB：固定 255/255/255，不参与单材料打印；
modelFill：写入 white/varnish 兼容摘要；
materialProcessProfile：W/V 互斥，validation 与有效通道一致；
preview：仅声明对应 white/varnish 与 support；
support：不改写既有支撑几何及参数。
```

`EffectiveConfigGenerator` 仅在显式启用单材料 override 时调用 resolver，并将 requested material、effective channel 与 stale 状态写入 `uiAudit.production.singleMaterialRelief`。

## 2. 失败闭环

以下情况保持源 JSON 不变并拒绝保存/切片：

```text
E_SINGLE_MATERIAL_RELIEF_UNSUPPORTED_PROFILE；
E_SINGLE_MATERIAL_RELIEF_INVALID_MATERIAL；
E_SINGLE_MATERIAL_RELIEF_CONFIG_CONFLICT；
E_SINGLE_MATERIAL_RELIEF_PROFILE_LOCKED。
```

`ConfigValidator` 会独立复核 `single_material_relief` 的完整 W/V 字段组，不能只信任 UI 选择值。

## 3. 验证

实际执行：

```powershell
cmake --build build-slicesoft/main --config Debug --target single_material_relief_resolver_unit_tests production_effective_config_unit_tests
ctest --test-dir build-slicesoft/main -C Debug -R "^(single_material_relief_resolver_unit_tests|production_effective_config_unit_tests)$" --output-on-failure
cmake --build build-slicesoft/main --config Debug --target slicer_debug_ui
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test
```

结果：2/2 定向测试 PASS，Qt self-test PASS。

## 4. 边界

```text
未新增 Qt 控件；
未改变 RGBWSV/TIFF 固定协议；
未改变支撑形态或 S 通道策略；
未执行真实 W/V Release package 矩阵；
纯白/透明 RIP 分色仍属于冻结的 12G-TCWS，不在本任务实现。
```
