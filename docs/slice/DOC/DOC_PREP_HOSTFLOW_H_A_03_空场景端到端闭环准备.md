# DOC_PREP HOSTFLOW H-A-03 空场景端到端闭环准备

> 状态：**IMPLEMENTED / VERIFIED**  
> 日期：2026-08-07  
> 任务：H-A-03 空场景 `import → addInstance → layout/transform → slice → verify`

## Goal

证明纯 C 与 Qt 参考宿主都能从空场景开始，仅通过冻结的 11 个 `pm_*` 导出完成一个
`p0.rgbwsv.2` 生产包，并且宿主不构造内部 scene JSON、不包含 `slicer_core/**`。

## Preconditions

| 前置 | 状态 | 本卡使用方式 |
|---|---|---|
| H-A-02 场景生命周期 | PASS | `model.import` 后以 `addInstance` 隐式创建 scene session |
| H-A-04 规则排版 | PASS | 通过 `scene.apply_operation/applyGridLayout` 排版 |
| SPI/能力外壳 | FROZEN | SPI v1、11 导出、15 能力均不改变 |
| 生产协议 | FROZEN | `p0.rgbwsv.2`、RGBWSV、8 bit、`black_is_print` |

## Implemented Flow

```text
model.import
  → scene.apply_operation(addInstance + sceneContext)
  → scene.apply_operation(applyGridLayout)
  → scene.apply_operation(translate)
  → scene.get_snapshot
  → slice.rgbwsv
  → package.verify
  → model.release
```

`scene.get_snapshot.scene` 是模块返回的权威完整 canonical scene。宿主只把该对象原样透传给
`slice.rgbwsv`，不解释、不补字段、不重新构造 scene schema。

## Runtime Corrections

端到端验证识别并关闭了两个跨边界身份问题：

1. `addInstance` 的 `sourceTransformIdentity` 改为已注册模型的源路径，保持与生产切片器的资源身份
   校验一致；不再错误使用 source digest 充当路径。
2. canonical scene 序列化统一把 `-0.0` 写为 `0.0`，使纯 C JSON 与 Qt JSON
   解析/再序列化后的 `sceneHash` 保持稳定。

Worker 在 scene hash 不匹配时同时报告 requested/actual hash，便于定位边界漂移；不改变错误码或
生产成功语义。

## File Ownership

主要实现：

```text
apps/slicer_host_sim/HostFlowEndToEnd.*
apps/slicer_host_sim/Main.c
apps/slicer_ui_host_sim/CapabilityCoverageWorkflow.cpp
apps/slicer_ui_host_sim/CapabilityCoverageRequests.*
src/slicer_core/api/scene/SceneFacadeOperation.cpp
src/slicer_core/scene/MultiModelScene.cpp
src/slicer_module/SceneCapabilitySerializationAdapter.cpp
apps/slicer_worker/slice/WorkerSliceRequestMaterializer.cpp
tests/contracts/scene_facade_14b03/Main.cpp
CMakeLists.txt
apps/slicer_ui_host_sim/CMakeLists.txt
```

边界保持：

```text
apps/slicer_debug_ui/**                 未修改
contracts/print_module_spi.h            未修改
生产 TIFF/RGBWSV 写入协议               未修改
宿主对 slicer_core/** 的直接依赖         0
宿主手工 scene builder                  0
```

## Verification

实际验证结果：

| Gate | Debug | Release |
|---|---:|---:|
| `hostflow_ha03_c_end_to_end` | PASS | PASS |
| `hostflow_ha03_qt_end_to_end` | PASS | PASS |
| H-A-02/H-A-04/SceneFacade 联合回归 | 3/3 PASS | 3/3 PASS |

附加门禁：

- DTO、三车道、Facade header/DTO 四个合同校验：PASS。
- pure-C host boundary、source-size guard：2/2 PASS。
- C 宿主 `HostFlowEndToEnd.c`：331 行，低于 500 行门禁。
- 宿主目录 `slicer_core/` include 与手工 scene builder 搜索：均为零命中。

## Gate Conclusion

H-A-03 已完成，H-A 组全部收口。H-B-01 的硬依赖已解除；打印侧外部 ACK 仍保持
`PENDING / DEFERRED`，本卡不能代替打印侧集成验收。
