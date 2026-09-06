# DOC_PREP_16C-06-MEMFLOW 实施准备与数据同步

> 状态：**PREPARATION COMPLETE / IMPLEMENTATION AUTHORIZED**
> 日期：2026-08-18
> 对应专项：`16C-06-MEMFLOW`

## 1. 准备结论

用户已明确授权：完成决策、方案、任务清单和上下文同步后，继续进入开发。当前适合从纯
`RasterMemoryBudget` 合同开始，不直接切换生产路由。

## 2. 当前代码落点

| 数据阶段 | 当前拥有者 | 当前问题 | 首个可包裹点 |
|---|---|---|---|
| Occupancy | `LayerOccupancyProvider` / `slicer.cpp` | 完整 3D Mask | 列范围与按层 view |
| Layer compose | `run_slicer` | 每层生成但由 const callback 复制 | owned callback |
| Instance raster | `LegacySceneLayerAdapter` | 保留全部层及 4 类 ownership | layer producer |
| Scene compose | `SceneLayerComposer` | 多实例完整层栈 | global-layer barrier |
| Package | `RgbwsvPackageWriter` | 完整 vector 后写 TIFF | staged layer sink |

## 3. 文件所有权

| 卡 | 主要文件 |
|---|---|
| MF-01 | `pipeline/RasterMemoryBudget.*`、Stage 16 单测、CMake |
| MF-02 | `slicer.h/.cpp`、`LegacySceneLayerAdapter.*`、adapter tests |
| MF-03 | `geometry/LayerOccupancyProvider.*`、support modules、Stage 16 tests |
| MF-04 | `output/rgbwsv/RgbwsvPackageWriter.*`、scene package writer、Writer tests |
| MF-05 | `SceneLayerComposer.*`、Orchestrator、Production Service、scene tests |
| MF-06 | 新建 sparse storage 内部模块；不得改 RGBWSV protocol |
| MF-07 | Production Service、Telemetry、Worker/Host 只读展示 |
| MF-08 | benchmark scripts、REPORT、task status |

## 4. 基线固化要求

下一次 `123.stl` 同请求运行必须先保存非敏感可复现快照：

```text
asset SHA-256
effective Profile/config hash
instance transform hash
Grid/DPI/layer thickness
support/material/varnish/preview/compression
build identity/compiler/runtime manifest
逐层 TIFF SHA-256 和 manifest totals
Worker/Host wall time、各阶段时间、Peak Working Set
```

输出目录继续使用 `output`，不得缩写为 `out`；临时文件名和作业子目录可使用受控短名。

## 5. 数据同步矩阵

| 数据 | 权威来源 | 消费方 | 一致性规则 |
|---|---|---|---|
| Grid/协议/Profile | Effective Config / Scene Admission | Producer/Composer/Writer | 作业开始后不可变 |
| layerIndex/zMm | Producer | Barrier/Writer/Report | 严格单调且无缺层 |
| RGBWSV | Material Composer | TIFF Writer | uint8、RGBWSV、black_is_print |
| Ownership | Producer | Scene Composer/统计 | 不进入 TIFF 新通道 |
| 逐层统计 | 同层校验扫描 | Report | 严格 Reader 独立复核 |
| 取消/修订 | Worker token/scene revision | 全流水线 | 任一步失效则整作业失败 |
| 内存路由 | Core budget planner | Telemetry/Host | Host 不重算 |

## 6. 风险与回退

```text
MF-01 只增加纯合同，不改变生产行为；回退为不调用新预算器。
MF-02..05 始终保留 Retained Dense；任何 hash 漂移即禁用新路径。
MF-06 Sparse 只能显式 candidate；不通过微小组件和支撑 Gate 不进入 MF-07。
已写 staging 不等于成功；严格校验和原子发布仍是唯一成功边界。
```

## 7. 验证命令

首卡：

```powershell
cmake --build build-slicesoft/main --config Release --target stage16c06_memory_budget_unit_tests
ctest --test-dir build-slicesoft/main -C Release -R "^stage16c06_memory_budget_unit_tests$" --output-on-failure
git diff --check
```

生产接入卡另需 Composer、Production Service、Package Writer、Golden、Stage 10 output contract、
RIP strict 和 Runtime self-test。未运行的 Gate 必须保持未完成状态。

## 8. 开工门

```text
[x] 用户显式授权根治专项和准备后开发
[x] Stage 16C-05 低风险复用已落地
[x] RGBWSV/SPI/Worker 冻结边界已登记
[x] 当前工作区其他 RIPFLOW 改动已识别，禁止覆盖
[x] MF-01 不切换生产默认，不需要设备 SLA
```

结论：MF-01 可以开工；MF-02 及后续按任务卡逐张推进。
