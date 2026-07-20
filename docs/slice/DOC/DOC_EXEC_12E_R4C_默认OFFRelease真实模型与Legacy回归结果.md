# DOC_EXEC_12E-R4C 默认 OFF Release 真实模型与 Legacy 回归结果

> 文档状态：12E-08C COMPLETE / RELEASE BUDGET BLOCKED
>
> 执行日期：2026-07-20
>
> 后续任务：12E-09A diagnostic UI 可启动；12E-08D production admission 继续阻断

## 1. 任务目标

12E-08C 在默认 `USE_OPENVDB=OFF` 的 Release 轨道完成以下证据收集：

1. 对真实 OBJ/MTL/纹理模型和 3MF Texture2D fixture 运行全局纹理/填充分区候选；
2. 分离模型加载、网格适配、拓扑、占用、距离、分区和输出写盘时间；
3. 记录分类网格规模、查询计数和进程工作集；
4. 运行旧 Profile、RIP strict、Repair Disabled TIFF 哈希回归；
5. 不写生产 package，不授予 production admission。

## 2. 新增入口

核心与命令入口：

```text
src/slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.*
apps/texture_fill_partition_release_benchmark/main.cpp
```

自动化入口：

```powershell
.\scripts\run_12e_08c_release_evidence.ps1 -BuildDir build -Config Release
```

机器可读汇总：

```text
output/benchmarks/12e_08c/release_evidence_summary.json
schema = slicesoft.texture_fill_partition.release_matrix.12e_08c.1
```

`output/` 不进入版本库。后续审计应通过脚本重新生成证据，不依赖开发机残留文件。

## 3. Benchmark 合同

本轮统一使用：

```text
buildType = Release
useOpenVdb = false
backend = legacy_cpu_global_distance
voxelMm = 0.10
widthMm = 0.20
paddingVoxels = 1
productionOutputWritten = false
productionAdmitted = false
```

`totalCoreMs` 只统计拓扑、占用构建、最近表面距离和分区，不包含 TIFF、PNG、JSON 写盘。
配置加载、模型加载和 mesh adapter 时间单独记录。被 strict topology 提前阻断的模型只记录阻断前
已经实际发生的时间，未执行阶段保持 `0` 或 `null`，不得据此估算完整核心耗时。

## 4. 真实模型结果

| Case | 拓扑 | 网格体素 | 核心结果 | 关键时间 | 结论 |
|---|---:|---:|---|---|---|
| `nai_you_new` | boundary=113，degenerate=1 | 1,942,785 | strict_closed 阻断 | load=437.4681 ms，adapt=104.7135 ms，topology=41.9462 ms | 未进入占用/距离/分区 |
| `aishen_fudiao` | boundary=3，nonManifold=59，degenerate=1 | 2,049,356 | strict_closed 阻断 | load=347.1296 ms，adapt=72.4907 ms，topology=31.9507 ms | 未进入占用/距离/分区 |
| `meigui_fudiao` | nonManifold=10,940 | 2,672,298 | strict_closed 阻断 | load=1460.7336 ms，adapt=120.4799 ms，topology=38.8370 ms | 未进入占用/距离/分区 |
| `three_mf_texture2d_checker` | closed | 7,168 | partition PASS | topology=0.0303 ms，occupancy=0.8005 ms，distance=1.2799 ms，partition=0.0157 ms，totalCore=2.2234 ms | diagnostic PASS |

3MF fixture 的分区统计：

```text
modelVoxels = 4500
textureSurfaceVoxels = 3824
modelFillVoxels = 676
overlapTextureFillVoxels = 0
unassignedModelVoxels = 0
peakWorkingSetBytes = 5869568
```

三个真实 OBJ 都在 strict topology gate 被拒绝，因此没有有效的 occupancy/distance/partition
和峰值工作集结果。本轮不能冻结代表真实生产模型的时间或内存阈值。

## 5. Legacy 回归

执行结果：

```text
Release full build: PASS
Release CTest: 21/21 PASS
Repair Disabled RIP strict: PASS
Repair Disabled TIFF SHA-256 invariant: PASS
Release quick regression: PASS
protocol: p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print
```

回归过程中发现并修复一项历史报告误判：`top_n_layers` 是每个 XY 列顶部 N 个模型层，
不能用“全局活动 layer 数量必须小于等于 N”校验曲面模型。现在真实爱神 Top2 Profile
可通过材料报告校验；Top1/Top2 数量对比使用相同小型 fixture，避免不同几何之间的无效比较。

## 6. Admission 结论

12E-08C 的“证据收集任务”已经完成，但 Release budget gate 为 `BLOCKED`：

```text
required cases = 4
partition passed = 1
topology blocked = 3
thresholdsFrozen = false
productionAdmission = blocked
```

这两个结论必须同时保留：

1. 12E-08C COMPLETE 表示脚本、报告、真实输入和 legacy regression 已形成可复现证据；
2. budget BLOCKED 表示候选尚不能覆盖真实 OBJ，不允许进入 12E-08D 生产写包。

## 7. 12E-09 准备判断

12E-09 的文档、配置所有权、异步边界、状态文本、preview 合同和 smoke matrix 已准备完整。

当前允许：

```text
12E-09A diagnostic UI
```

UI 必须显示真实状态：`diagnostic`、`blocked`、topology issue、backend 和 effective config，
不得把 3MF fixture PASS 外推为真实 OBJ 生产可用。

当前禁止：

```text
12E-09B production Profile
12E-08D production package/admission
```

解锁 12E-08D 至少需要先解决真实 OBJ topology admission 策略，重新取得代表性 Release
核心耗时和内存，再由用户明确确认生产路径变更。

## 8. 残余风险

1. strict_closed 与真实 OBJ 资产拓扑不兼容，当前没有 repair/relaxed policy 的生产决策；
2. blocked case 的 `totalCoreMs=0` 只代表核心分区未开始，不代表算法耗时为零；
3. 3MF fixture 较小，只能证明报告链路和分区不变量，不能代表真实模型性能；
4. 本任务没有执行 texture transfer、raster mapping 或 production writer，相关字段保持 `null/0`；
5. OpenVDB 仍为 optional/OFF，本结论不构成 OpenVDB 替代 legacy 的依据。
