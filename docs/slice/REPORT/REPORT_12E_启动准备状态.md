# REPORT_12E 启动准备状态

> 文档状态：12E-08A COMPLETE / 12E-08 IN PROGRESS / PRODUCTION NOT ADMITTED
> 日期：2026-07-17

## 1. 当前结论

12E 已完成 12E-01 至 12E-07 以及 12E-08A。除 CPU/OpenVDB 同 grid diagnostic candidate 外，成功报告、真实 Z 层 voxel 统计、代表性 Width Sweep、单调性 validator、all-texture endpoint、OBJ/3MF 纹理传递、内存 Diagnostic Composer、12D 模型域精确闭环联动和 classification-to-raster 确定性映射也已建立。

当前两个候选仍为 diagnostic-only。12E-08A 已在 world-space raster center 上保持 texture/fill 精确互补和真实 layerIndex/zMm，并保持 production output 关闭。完整支撑/光油 closure、默认 OFF Release 真实模型预算和 legacy regression 证据尚未关闭，12E-08D 也尚未获得用户生产路径确认。

## 2. Current State

```text
12A：材料填充、支撑、光油语义当前范围完成；
12B：性能评估与 OpenVDB SDF utility 定位完成；
12C：Qt 工作台 R0/R1/R2 完成；
12D：R0/R1/R2/R3 COMPLETE，12D-10 三个真实 OBJ 验收通过；
12E：R0 complete，12E-01..07 与 12E-08A complete，12E-08 in progress。
```

legacy texture apply mode 和 modelFill scope 保持兼容；CPU backend 只产生 diagnostic result。当前不存在 12E production package，也未改变原有切片生产路径。

## 3. 已完成准备

```text
12E 产品语义和阶段边界；
global_surface_shell 配置结构；
complement_of_global_texture_shell 成对约束；
静态校验与运行时 preflight 分层；
backend unavailable 稳定阻断策略；
slicesoft.texture_fill_partition.12e.1 schema；
generated/real model/backend/UI/protocol 验收矩阵；
12E-01 Config/DTO 契约；
12E-02 service、3D mask 和不变量验证；
12E-03 CPU candidate 的 occupancy、distance、topology、closest reference 和性能证据；
12E-04 OpenVDB conformance adapter 与差异 DTO；
12E-05 Width Sweep、成功 Report 与 monotonic validator；
12E-06 Texture Transfer 与 Diagnostic Composer；
12E-07 12D Closure 联动；
12E-08 Production Admission 准备；
12E-08A classification-to-raster DTO、算法、generated fixture 与 report；
12E-09 Qt UI 与 Effective Config 准备。
```

## 4. 尚未实现

```text
production admission；
完整 support/varnish semantic sidecar 与 full closure；
默认 OFF Release 真实模型性能、内存和 legacy regression；
Qt UI 与 preview；
真实模型回归和 REPORT_12E 完成报告。
```

## 5. 准入状态

| 任务 | 状态 | 说明 |
|---|---|---|
| 12E-00 | COMPLETE | 正式阶段文档与入口 |
| 12E-R0 preparation | COMPLETE | Config/DTO、schema、matrix、状态报告 |
| 12E-01 | COMPLETE | Config/DTO、稳定错误码、安全门禁和 report skeleton |
| 12E-02 | COMPLETE | Service、3D mask DTO、统计与不变量骨架 |
| 12E-03 | COMPLETE | Legacy CPU occupancy/distance diagnostic candidate |
| 12E-04 | COMPLETE | OpenVDB OFF/ON conformance adapter、同 grid 和差异 DTO |
| 12E-05 | COMPLETE | Width Sweep、成功报告、golden 和 monotonic validator |
| 12E-06 | COMPLETE | Texture Transfer 与 Diagnostic Composer |
| 12E-07 | COMPLETE | 12D Closure 联动，限 texture/model-fill 诊断范围 |
| 12E-08A | COMPLETE / DIAGNOSTIC ONLY | classification-to-raster、量化、coverage、报告与 generated fixture |
| 12E-08B/08C | TODO | 完整 closure、Release 预算和回归证据 |
| 12E-08D | BLOCKED / REQUIRES CONFIRMATION | production package、RIP strict 与 admission |
| 12E-09 | PREPARED / BLOCKED | Qt diagnostic UI 与 Effective Config 已准备 |
| 12E-10 | PLANNED | 真实模型和收口 |

## 6. 与 12D 的关系

```text
12D-07/08/09：COMPLETE；
12D-10：COMPLETE；
12E-01：COMPLETE；
12E-02：COMPLETE；
12E-03：COMPLETE；
12E-04：COMPLETE；
12E-05：COMPLETE；
12E-06：COMPLETE；
12E-07：COMPLETE；
12E-08A：COMPLETE；
12E-08B/08C：TODO；
12E-08D：BLOCKED BY PRODUCTION EVIDENCE AND CONFIRMATION；
当前没有 active code task；
12E R0/R1 原型不要求先完成 repair；
12E production admission 必须复核 12D exact closure；
不得把 12E 分区逻辑塞入 12D repair 任务。
```

## 7. 开放项

以下问题不阻塞已完成的契约任务，但必须在后续 Gate 前用实际证据关闭：

```text
CPU 3D distance candidate 的真实模型性能和内存是否满足后续预算；
OpenVDB 是否只保留 conformance，或经新决策获得候选生产角色；
medial-axis tie 的稳定颜色选择规则；
真实模型内腔表面的纹理参与范围；
production Profile 的最终最小宽度是否高于 0.10 mm。
```

## 8. 12E-01 实际实现与验证

实现：

```text
TextureSurfaceShellConfig parser/validator；
GlobalTextureFillPartitionOptions 与 TextureFillPartitionReportData；
TextureFillPartitionErrorCode / TextureFillPartitionError；
global_surface_shell 与 complement_of_global_texture_shell 成对约束；
传统和 OpenVDB 候选入口的 backend unavailable 前置阻断；
slicesoft.texture_fill_partition.12e.1 unavailable report skeleton；
最小 config fixture 与 config/negative/report/no-package 单测。
```

实际验证：

```text
cmake --build build --config Debug --target experimental_config_unit_tests：完成；
build/Debug/experimental_config_unit_tests.exe：全部 PASS；
ctest --test-dir build -C Debug -R "experimental_config|texture_fill_partition" --output-on-failure：1/1 PASS；
ctest --test-dir build -C Debug --output-on-failure：9/9 PASS；
cmake --build build --config Debug --target slicer_cli：PASS；
slicer_cli --config samples/configs/texture_fill_partition/global_surface_shell_unavailable.json：
  exit=1，E_12E_PARTITION_BACKEND_UNAVAILABLE，未写 package。
```

首次定向构建因外层命令 120 秒超时未返回，但后台 MSBuild 随后正常完成，生成的测试程序已实际运行通过；该超时不记录为测试 PASS。

## 9. 12E-02 实际实现与验证

实现：

```text
TextureFillPartitionGridSpec 与 TextureFillPartitionMask3D；
GlobalTextureFillPartitionRequest/Candidate/Result；
IGlobalTextureFillPartitionBackend；
GlobalTextureFillPartitionService；
由 service 重算的七类 partition stats；
grid、mask size、二值性、outside、overlap、unassigned 稳定错误；
backend 异常与 request/backend grid 不一致的稳定阻断；
diagnostic pass 与 productionAcceptance=not_evaluated 的强制边界；
texture_fill_partition_service_unit_tests。
```

实际验证：

```text
cmake --build build --config Debug --target texture_fill_partition_service_unit_tests：PASS；
build/Debug/texture_fill_partition_service_unit_tests.exe：9/9 cases PASS；
ctest --test-dir build -C Debug -R "texture_fill_partition_service|experimental_config" --output-on-failure：2/2 PASS；
ctest --test-dir build -C Debug --output-on-failure：10/10 PASS；
git diff --check：PASS（仅 Git 行尾转换提示）。
```

## 10. 12E-03 实际实现与验证

实现：

```text
PointInClosedMeshQuery：AABB BVH、确定性主/备用射线、边界与歧义统计；
ClassifyPointInClosedMeshBruteForce：generated fixture 测试 oracle；
LegacyCpuGlobalDistanceBackend：严格拓扑门禁、全 grid occupancy、最近三角形距离与互补分区；
TextureFillPartitionWidthMetrics：resolution、epsilon、effective minimum、effective width、max interior distance、all-texture threshold；
TextureFillClosestSurfaceReference：triangleIndex、barycentric、distance；
TextureFillPartitionQueryStats/Performance：查询量、core timing、估算内存和进程 working set；
新增 7 个 CPU/后端稳定错误码；
legacy_cpu_global_distance_unit_tests。
```

实际验证：

```text
cmake --build build --config Debug --target legacy_cpu_global_distance_unit_tests：PASS；
build/Debug/legacy_cpu_global_distance_unit_tests.exe：11/11 cases PASS；
ctest --test-dir build -C Debug -R "legacy_cpu_global_distance|texture_fill_partition_service" --output-on-failure：2/2 PASS；
ctest --test-dir build -C Debug --output-on-failure：11/11 PASS；
cmake --build build --config Debug：PASS；
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test：PASS（startup、experimental-report-summary）；
cmake --build build-openvdb-09p --config Debug --target legacy_cpu_global_distance_unit_tests：PASS；
build-openvdb-09p/Debug/legacy_cpu_global_distance_unit_tests.exe：11/11 cases PASS。
```

OpenVDB ON 兼容轨道确认使用 OpenVDB 12.0.1 与
`D:\vcpkg-openvdb\scripts\buildsystems\vcpkg.cmake`；构建只出现 CMake CMP0167/FindBoost
developer warning，没有影响本任务目标或测试结果。该段记录的是 12E-03 当时仅验证 CPU candidate
在 OpenVDB ON 构建中的兼容性；12E-04 的实际实现证据见后续章节。

一次 Debug generated closed-box 观测值：

```text
gridVoxels=1728；modelVoxels=1000；
topologyMs=0.5792；occupancyMs=3.4379；distanceMs=3.1322；
partitionMs=0.0159；totalCoreMs=7.5128；
processPeakWorkingSetBytes=5509120。
```

该值只证明计时和内存字段已实际产生，不是 Release 性能门槛，也不能外推到真实甲片模型。

`scripts/run_ci_quick.ps1` 本次没有通过：回归脚本对 `obj_mtl_texture_rgb_white_varnish.json` 仍期待 `output/ObjMtlTextureRgbWhiteVarnish`，但该配置当前写入 `output/ProfileTexturedNailRgbWhiteLowerSupport`，随后 RIP Reader 返回 `E_PACKAGE_NOT_FOUND`。12E-03 不进入旧 production pipeline，本任务未修改该既有 config/script 映射；该失败不记录为 12E-03 PASS，也未在本任务中扩展范围修复。

## 11. 12E-04 实际实现与验证

实现：

```text
OpenVdbSignedDistanceSample / SampleOpenVdbSignedDistanceWorld；
OpenVdbTextureFillConformanceBackend；
parity interior test 支持嵌套闭合表面和 cavity；
OpenVDB occupancy + NearestTriangleQuery 完整 exact distance；
TextureFillPartitionConformanceResult 与比较器；
levelSet/gridSample/OpenVDB grid bytes 性能字段；
6 个 OpenVDB/conformance 稳定错误码；
openvdb_texture_fill_conformance_unit_tests，8 个 cases。
```

实际验证：

```text
默认 OFF build target：PASS；8/8 cases PASS；
OpenVDB ON build target：PASS；8/8 cases PASS；
OFF targeted CTest：2/2 PASS；
ON targeted CTest：3/3 PASS；
默认 OFF 全量 CTest：12/12 PASS；
OpenVDB ON 全量 build：PASS；全量 CTest：12/12 PASS；
Qt UI self-test：PASS（startup、experimental-report-summary）；
closed box、sloped body、thin wall、closed cavity、strict topology、width threshold、repeat：PASS。
```

`scripts/run_ci_quick.ps1` 已再次执行，但仍在既有 OBJ/MTL material-process 回归处失败：
配置实际写入 `output/ProfileTexturedNailRgbWhiteLowerSupport`，脚本随后读取
`output/ObjMtlTextureRgbWhiteVarnish`，RIP Reader 返回 `E_PACKAGE_NOT_FOUND`。失败点与
12E-04 新增的 OpenVDB conformance backend 无关；本任务未越界修改旧回归脚本与配置映射，
因此该命令不记录为 PASS。

一次 generated closed-box Debug ON 观测：

```text
cpuModelVoxels=1000；openVdbModelVoxels=1000；
modelOnlyCpu=0；modelOnlyOpenVdb=0；
maxDistanceDeltaMm=0；allTextureThresholdDeltaMm=0；
cpuTotalCoreMs=5.6286；openVdbTotalCoreMs=247.445；
openVdbGridBytes=2492360。
```

该数据只证明同 grid conformance 和性能字段可复现。它是单次 Debug generated fixture，不能作为
Release 性能结论；当前 OpenVDB candidate 明显更慢，也未获得 production role。

## 12. 12E-05 实际实现与验证

实现：

```text
TextureFillPartitionWidthSweepOptions/Sample/Result；
GlobalTextureFillPartitionService::EvaluateWidthSweep；
effective minimum、中间代表点和 allTexture endpoint 动态采样；
显式 full-step scan、maxSamples 守门和 6 个稳定 width-sweep 错误码；
model 不变、texture 非递减、fill 非递增、exact partition 与 endpoint validator；
CPU/OpenVDB allTexture threshold 按 0.01 mm 对动态下界整体向上量化；
BuildTextureFillPartitionReport 成功报告和真实 Z 层 voxel 统计；
BuildTextureFillPartitionWidthSweepSummary；
12e_width_sweep_summary.json 与 12e_texture_fill_partition_report_schema.json；
width sweep 13 个 cases，report 4 个 cases。
```

实际验证：

```text
默认 OFF 两个 target build：PASS；
width sweep：13/13 PASS；report：4/4 PASS；
默认 OFF targeted CTest：3/3 PASS；
OpenVDB ON 两个 target build：PASS；targeted CTest：3/3 PASS；
默认 OFF 全量 build：PASS；全量 CTest：14/14 PASS。
```

12E-05 只序列化 validated diagnostic result；未把报告写入 production manifest，未写
RGBWSV TIFF，未接入 Qt 或 production composer。不可测量的 raster pixel 字段保持 `null`，
不会用假 `0` 冒充证据。

## 13. 12E-06 实际实现与验证

实现：

```text
TextureFillPartitionTextureTransfer 统一处理 OBJ/3MF AdaptedTriangleMesh 属性；
只复用 closestSurfaceReferences，不重复做 nearest triangle 查询；
支持 texture、material diffuse、fallback 三类颜色来源；
missing UV/resource/sample 支持 warn_and_fallback 与 fail_fast；
NearestTriangleQuery 保留确定性 tie candidate 证据；
TextureFillPartitionDiagnosticComposer 输出真实 Z 顺序 exact masks 和内存 RGBWSV；
texture 写 RGB，white/varnish/rgb fill 分别写 W/V/RGB；
S 保持 255，通道顺序固定 R G B W S V；
报告新增 textureTransfer、diagnosticComposer、textureTransferMs 和稳定 issues；
新增 transfer 12 个、composer 6 个、report 5 个单元用例和一个 golden。
```

实际验证：

```text
默认 OFF 三个 target build：PASS；
默认 OFF 定向 CTest：4/4 PASS；
OpenVDB ON 三个 target build：PASS；
OpenVDB ON 定向 CTest：4/4 PASS；
默认 OFF 全量 build：PASS；
默认 OFF 全量 CTest：16/16 PASS；
git diff --check：PASS（仅 Git 行尾转换提示）。
```

12E-06 未写生产 TIFF、manifest、package 或 preview，未接入 Qt，未改变 legacy
`slicer_cli` production path，也未授予 OpenVDB production role。

## 14. 12E-07 实际实现与验证

实现：

```text
TextureFillPartitionClosureAdapter；
六个 closure adapter 稳定错误码；
partition/composer 同 grid、二值 mask、真实 layerIndex/zMm 和 channel order 守门；
12E exact TextureSurface/ModelFill 到 12D MaterialClosureSemanticLayerInput 的只读映射；
model-domain empty 与 ColorFillGap 分层统计；
allTexture not_applicable(reason=all_texture_partition)；
support/varnish 明确 not_evaluated；
closureLinkage report、per-layer evidence 和 golden；
repairAttempted=false、productionOutputWritten=false。
```

实际验证：

```text
adapter 单测：10/10 PASS；
report 单测：6/6 PASS；
默认 OFF 定向 CTest：3/3 PASS；
OpenVDB ON 定向 CTest：3/3 PASS；
12D Repair Disabled 脚本：RIP strict PASS；30 层 TIFF SHA-256 invariant PASS；
默认 OFF 全量 build：PASS；
默认 OFF 全量 CTest：17/17 PASS；
git diff --check：PASS（仅 Git 行尾转换提示）。
```

该结果只证明模型 texture/fill 域的 exact diagnostic closure。支撑、内部空洞支撑和光油
仍为 `not_evaluated`，不能外推为完整 production closure PASS。

## 15. 下一任务

12E-08A 已完成。12E-08 production admission 仍必须依次关闭：

```text
12E-08B 完整 support/varnish closure；
12E-08C 默认 OFF Release 真实模型预算与 legacy regression；
12E-08D 前再次取得用户 production path 明确确认。
```

准备入口：`docs/slice/DOC/DOC_PREP_12E_R4_ProductionAdmission准备.md`。

## 16. 安全边界

```text
p0.rgbwsv.2 不变；
R G B W S V 不变；
uint8 / black_is_print 不变；
OpenVDB optional/OFF；
legacy slicer_cli production path 不替代；
12D repair 默认关闭；
没有 production admission 时不写 12E production TIFF。
```

## 17. 后续准备复核

2026-07-17 已完成 12E-08 准备复核：

```text
12E-07 已完成 texture_model_fill_only exact closure；
12E diagnostic grid 不能直接冒充最终打印 raster grid；
支撑、内部空洞和光油必须在最终 raster semantic sidecar 中形成完整证据；
默认 OFF CPU candidate 仍需真实模型 Release 性能和内存预算；
新 Profile production package、RIP strict 和旧 Profile 回归仍需实证；
12E-08A 已完成 classification-to-raster diagnostic mapping；
12E-08B/08C 继续关闭 production evidence；
12E-08D 前必须再次取得用户明确确认。
```

## 18. 12E-08A 实际实现与验证

实现：

```text
TextureFillPartitionRasterGridSpec / Layer / Stats / Result；
MapTextureFillPartitionToRaster；
world_space_cell_containment 与半开 source cell ownership；
真实 raster center layerIndex/zMm；
model/texture/fill 精确 mask 和 texture-only RGB；
coverage delta、source reuse、quantization error、mappingMs；
rasterMapping report、performance.rasterMappingMs 与 golden；
五个 E_12E_RASTER_MAPPING_* 稳定错误码。
```

验证：

```text
mapper generated fixture：11/11 PASS；
默认 OpenVDB OFF mapper/report 定向 CTest：2/2 PASS；
OpenVDB ON mapper/report 定向 CTest：2/2 PASS；
默认 OFF 全量 Debug build：PASS；
默认 OFF 全量 CTest：18/18 PASS；
12D Repair Disabled：RIP strict PASS，30 层 TIFF SHA-256 invariant PASS；
productionOutputWritten=false；
productionAcceptance=not_evaluated。
```

该结果关闭了 classification-to-raster 的确定性映射缺口，但没有接入生产 composer 或 writer。

12E-09 准备结论：

```text
diagnostic UI、异步 worker、0.01 mm 控件、动态阈值、effective config 和真实 layer preview 合同已准备；
12E-09A 仍建议等待 12E-08B/08C；
12E-09B production Profile 被 12E-08D 阻断。
```
