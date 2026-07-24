# REPORT 12E-09B-01 能力目录与 UI DTO 当前状态

> 状态：COMPLETE
> 日期：2026-07-23
> 下一原子任务：12E-09B-02 Production Effective Config

## 1. 任务目标

本任务建立 Qt 产品入口可复用的双模式能力目录和 fail-closed UI 状态 DTO，不增加可见控件，
不启动切片，也不改变现有 Profile、TIFF 或 OpenVDB 行为。

## 2. 已实现内容

新增：

```text
apps/slicer_debug_ui/services/ProductionModeCatalog.h
apps/slicer_debug_ui/services/ProductionModeCatalog.cpp
tests/unit/production_mode_catalog/Main.cpp
```

能力目录固定：

| 项目 | 当前能力 |
|---|---|
| Legacy | 默认模式；保留现有 Profile；资源等级 normal |
| Global restricted | 显式 opt-in；RGB/W 启用；S/V 禁用 |
| Global material parity | 显式 opt-in；RGB/W/S/V 启用；S 为 lower/internal void；V 为 surface/outer |
| 未知 Profile | 返回空并 fail-closed，不映射为 Legacy |

中文显示名已进入目录，但 09B-01 不创建 UI 控件。能力锁定版本固定为：

```text
slicesoft.ui.production_capability.12e_09b.1
```

## 3. UI DTO

`ProductionModeUiDto` 当前覆盖：

```text
requested/effective mode；
requested/effective Profile；
admission state；
productionOutputWritten；
fallbackApplied；
resource cost；
实际总耗时和峰值工作集；
session/config/package identity；
稳定阻断码和中文消息承载字段。
```

默认值保持 fail-closed：

```text
effective mode/profile = 未评估；
admission = pending；
productionOutputWritten = false；
fallbackApplied = false；
resource/timing/memory = 未评估。
```

DTO 不包含 Qt 控件、OpenVDB 类型、拓扑判定或 TIFF 写入逻辑。

## 4. 架构边界

```text
SlicePipelineConfig：模式枚举和稳定字符串真源；
materialProcessProfile.target：Profile 选择真源；
08D production Profile evaluator：最终材料能力和生产准入真源；
ProductionModeCatalog：UI 展示、预锁定和未知值阻断，不替代核心校验。
```

## 5. 验证结果

实际执行：

```powershell
cmake --build build --config Debug --target production_mode_catalog_unit_tests slicer_debug_ui
.\build\Debug\production_mode_catalog_unit_tests.exe
ctest --test-dir build -C Debug -R "(production_mode_catalog|slice_pipeline_router|global_surface_shell_production_pipeline)" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

结果：

```text
production_mode_catalog_unit_tests：PASS；
slice_pipeline_router_unit_tests：PASS；
global_surface_shell_production_pipeline_unit_tests：PASS；
CTest：3/3 PASS；
Qt self-test：PASS startup / PASS experimental-report-summary。
```

## 6. 未实现内容

以下内容属于后续任务：

```text
09B-02：把模式、Profile 和 capability lock 写入 session Effective Config；
09B-03：中文模式/Profile 选择器和能力禁用提示；
09B-04：一键切片路由与 no-fallback；
09B-05：生产 package、资源数据和当前 session 绑定；
09B-06：真实模型、Release、UI smoke 和文档收口。
```

## 7. 结论

12E-09B-01 已完成。Legacy 仍为默认，Global 仍需显式选择；本任务没有开放 Global 可见入口，
也没有修改生产协议。下一原子任务可进入 `12E-09B-02`。
