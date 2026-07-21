# REPORT_12E 启动准备状态

> 文档状态：12E-08C-R1/R2/R3 COMPLETE / R3-04 NO-GO / PRODUCTION NOT ADMITTED
> 日期：2026-07-21

## 1. 当前结论

12E 已完成 12E-01 至 12E-07 以及 12E-08A/08B/08C。除 CPU/OpenVDB 同 grid diagnostic candidate 外，成功报告、真实 Z 层 voxel 统计、代表性 Width Sweep、单调性 validator、all-texture endpoint、OBJ/3MF 纹理传递、内存 Diagnostic Composer、12D 模型域与完整材料域精确闭环联动、classification-to-raster 确定性映射和默认 OFF Release 证据轨道也已建立。

当前两个候选仍为 diagnostic-only。12E-08C-R1/R2/R3 已完成非生产 repair、Release global core、legacy TIFF invariant 与 RIP strict 证据；三个真实 OBJ 在 mutation 前因 confirmed self-intersection fail-fast，只有闭合 3MF 完成 global full chain。R3-04 已输出 NO-GO，预算阈值未冻结。12E-08D 继续被真实模型 topology、预算、known legacy baseline 和用户生产路径确认阻断。

## 2. Current State

```text
12A：材料填充、支撑、光油语义当前范围完成；
12B：性能评估与 OpenVDB SDF utility 定位完成；
12C：Qt 工作台 R0/R1/R2 完成；
12D：R0/R1/R2/R3 COMPLETE，12D-10 三个真实 OBJ 验收通过；
12E：R0 complete，12E-01..07 与 12E-08A/08B/08C complete；12E-08C-R1/R2/R3 complete，R3-04 NO-GO，Release budget blocked，12E-08D blocked。
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
12E-08B full-material semantic sidecar、12D closure、报告与 generated fixture；
12E-08C 默认 OFF Release 真实模型证据、legacy regression 与稳定回归 fixture；
12E-08C-R1/R2/R3 修复专项 Decision、PRD、DEV、DEMO、ROADMAP、Schema、Matrix、Prep 与任务入口；
12E-09 Qt UI 与 Effective Config 准备。
```

## 4. 尚未实现

```text
production admission；
真实 OBJ strict topology admission 与可冻结的 Release 性能/内存预算；
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
| 12E-08B | COMPLETE / DIAGNOSTIC ONLY | 完整材料 semantic sidecar、五类 gap、S/V 通道一致性与报告 |
| 12E-08C | COMPLETE / BUDGET BLOCKED | Release evidence 与 legacy regression 完成；3 个真实 OBJ strict topology 阻断 |
| 12E-08C-R1/R2/R3 | COMPLETE / NON-PRODUCTION / R3-04 NO-GO | Repair、完整相交、真实模型矩阵、Release global core、legacy TIFF/RIP 证据完成；三个 OBJ topology skipped |
| 12E-08D | BLOCKED / REQUIRES CONFIRMATION | production package、RIP strict 与 admission |
| 12E-09 | 09A READY / 09B BLOCKED | Qt diagnostic UI 与 Effective Config 已准备；production Profile 等待 08D |
| 12E-10 | PLANNED | 真实模型和收口 |
| 12E-10 Prep | COMPLETE / EXECUTION BLOCKED | Preview、真实模型、Release 和 REPORT 原子任务已拆分 |

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
12E-08B：COMPLETE；
12E-08C：COMPLETE / RELEASE BUDGET BLOCKED；
12E-08C-R1/R2/R3：COMPLETE / NON-PRODUCTION，R3-04 NO-GO；
12E-08D：BLOCKED BY REAL OBJ TOPOLOGY BUDGET AND CONFIRMATION；
当前没有 active code task；12E-08D 等待外部修复和 Gate 重跑；
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

12E-08A/08B/08C 已完成。12E-08 production admission 仍必须依次关闭：

```text
真实 OBJ strict topology admission 与可冻结的 Release 时间/内存预算；
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

2026-07-20 已完成 12E-08 准备复核：

```text
12E-07 已完成 texture_model_fill_only exact closure；
12E diagnostic grid 不能直接冒充最终打印 raster grid；
12E-08B 已使支撑、内部空洞和光油在最终 raster semantic sidecar 中形成完整诊断证据；
默认 OFF CPU candidate 的真实 OBJ 因 strict topology 被阻断，Release 预算未冻结；
旧 Profile、RIP strict 和 Repair Disabled TIFF invariant 已通过；
12E-08A 已完成 classification-to-raster diagnostic mapping；
12E-08B 已完成 full-material diagnostic closure；
12E-08C 已完成取证，但 Release budget 保持 BLOCKED；
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
12E-09A 已满足前置证据，可进入 diagnostic UI 实现；
12E-09B production Profile 被 12E-08D 阻断。
```

## 19. 12E-08B 实际实现与验证

实现：

```text
TextureFillPartitionFullClosureLayerEvidence/Request/Result；
TextureFillPartitionFullClosureAdapter；
最终 raster texture/fill 与 support/internal-void/surface-varnish/outer-varnish sidecar；
Model > OuterVarnishShell > Support > Empty 优先级守门；
ExpectedOccupiedDomain 和六通道 LayerEmpty 精确对照；
model/support/varnish 独立 closure status；
support/varnish mask 与 S/V 通道 mismatch 统计；
五类 12D gap、域外材料和真实 layerIndex/zMm；
fullClosureLinkage、performance.fullClosureMs 与 golden；
八个 E_12E_FULL_CLOSURE_* 稳定错误码。
```

验证：

```text
full-closure adapter generated fixture：16/16 PASS；
report unit cases：8/8 PASS；
默认 OpenVDB OFF 全量 Debug build：PASS；
默认 OpenVDB OFF 全量 CTest：19/19 PASS；
OpenVDB ON adapter/report 定向 build：PASS；
OpenVDB ON adapter/report 定向 CTest：2/2 PASS；
12D Repair Disabled：baseline/diagnostic RIP Reader PASS；
30 层 production TIFF SHA-256 invariant：PASS；
repairAttempted=false；
productionOutputWritten=false；
productionAcceptance=not_evaluated。
```

详细结果：`docs/slice/DOC/DOC_EXEC_12E_R4B_完整材料语义闭环结果.md`。

12E-08C 已完成证据收集，详细结果见
`docs/slice/DOC/DOC_EXEC_12E_R4C_默认OFFRelease真实模型与Legacy回归结果.md`。下一允许原子任务为
12E-08C-R1-01；12E-09A diagnostic UI 可并行，09B 继续被 12E-08D 阻断。

## 20. 12E-08C 实际结果

```text
Release full build：PASS；
Release CTest：21/21 PASS；
Release quick regression：PASS；
Repair Disabled RIP strict / TIFF SHA-256 invariant：PASS；
3MF Texture2D：partition PASS，totalCoreMs=2.2234，peakWorkingSetBytes=5869568；
nai_you_new：boundaryEdges=113，strict_closed BLOCKED；
aishen_fudiao：boundaryEdges=3、nonManifoldEdges=59，strict_closed BLOCKED；
meigui_fudiao：nonManifoldEdges=10940，strict_closed BLOCKED；
productionOutputWritten=false；
productionAdmitted=false；
thresholdsFrozen=false。
```

因此 12E-08C 任务状态为 COMPLETE，但 Release budget 状态为 BLOCKED。不得把证据任务完成
解释为真实 OBJ 性能准入或 production admission 通过。

## 21. 12E-08C-R1/R2/R3 修复专项准备

2026-07-20 已建立真实模型拓扑修复前置专项：

```text
R1：Contract & Eligibility；
R2：Conservative Repair；
R3：Real Model & Release Gate。
```

已生成正式 Decision、PRD、DEV、DEMO、ROADMAP、Report Schema、Acceptance Matrix、Prep、任务清单、
执行指令、启动报告和 AI handoff。R1/R2/R3 已完成非生产证据闭环；`repair_then_strict` 当前已有保守
cleanup、guarded topology、simple boundary、独立 evidence guard、non-manifold classifier、完整自相交、
真实模型矩阵和 Release/legacy 证据，尚未生产接入；R3-04 为 NO-GO。12E-08D 继续 BLOCKED。

## 22. 双切片模式与统一 TIFF 目标状态

2026-07-20 已补充产品目标：用户最终可通过 UI 或配置在 `legacy` 与 `global_surface_shell` 两条流水线间
显式选择，正式字段为 `slicePipeline.mode`，历史配置默认按 legacy 解释。

当前实现状态必须区分：

```text
legacy：现有生产路径，可生成 p0.rgbwsv.2 RGBWSV TIFF；
global_surface_shell：分区、纹理传递、raster 与 closure 为 diagnostic-only；
双模式 Router / global production adapter / 共享 writer 接入：NOT IMPLEMENTED；
Qt 双模式生产选择器：NOT IMPLEMENTED；
12E-08D：仍被 R3-04 NO-GO 与 production confirmation 阻断。
```

目标状态中，两种模式只在生产层组合之前分叉，最终共用现有 TIFF writer、manifest、preview/report 和
RIP Reader。任何模式只有在完整 TIFF 写出并通过协议校验后才能标记 production success；global
unavailable/blocked 禁止静默回退 legacy。该补充已形成 Decision、Config Schema、08D Prep 和 AI handoff；
当前等待 required OBJ 外部修复和准入 Gate 重跑。

## 23. 12E-08C-R1-04 实际结果

R1 已完成。三个真实 OBJ 均输出稳定 `manual_repair_required`，闭合 Texture2D 3MF 输出
`strict_pass_no_repair`；每个 case 连续两次的 config/source/geometry/attribute/stable evidence 一致。

Baseline 同时确认：真实 OBJ 分别含 10/10/2 个组件；`aishen_fudiao` 与 `meigui_fudiao` 的 duplicate 全部
属于 opposite duplicate；三个 OBJ 自相交检查均为 sampled。R2-01 因此只处理显式 degenerate 与同属性 exact
duplicate，并保持组件不隐式 merge；R3-01A 已完成完整自相交证据并确认三个 required OBJ 存在自相交。
12E-08D 继续 BLOCKED。

## 24. 12E-08C-R2-01 实际结果

R2-01 已实现 adapter degenerate provenance、同属性同向 exact duplicate cleanup、source mapping、operation/post
hash 和真实模型重复性脚本。`nai_you_new`、`aishen_fudiao` 各记录 1 个 adapter-filtered degenerate；
`meigui_fudiao` 与闭合 3MF 为 no-op。opposite duplicate 和属性冲突均不自动删除，三个 OBJ 继续 manual，
闭合 3MF 继续 strict PASS。该结果作为 R2-02 的输入基线；后续阶段仍按原子 Gate 推进。

## 25. 12E-08C-R2-02 实际结果

R2-02 已实现受约束 vertex weld、唯一 local winding 传播、组件不隐式 merge、UV corner 同步和
`vertexMappings[]`。generated safe/negative fixtures 通过；四个 required case 双运行 stable projection 通过。
三个 OBJ 无新增 weld/flip 并继续 manual，闭合 3MF no-op strict PASS。该结果作为 R2-03 输入；后续仍按
原子 Gate 推进。

## 26. 12E-08C-R2-03 实际结果

R2-03 已实现简单平面凸 boundary loop fill、全量预算守门、完整 self-intersection evidence 前置、统一无 UV
材质策略和 generated triangle provenance。generated 缺顶 box 补面后 strict PASS；negative fixtures 稳定
blocked。四个 required case 双运行稳定且未生成新面，三个 OBJ 继续 manual。R2-04 已完成独立 post-strict/
attribute/hash 守门。R3-01 已完成 non-manifold 模式分类，R3-01A 已完成完整自相交证据，R3-02 已完成真实模型矩阵，R3-03 已完成 Release/legacy 证据，R3-04 NO-GO，08D 继续阻断。

## 27. 12E-08C-R3-02 实际结果

R3-02 已完成 strict/no-repair 与 conservative-repair 双 lane、每条双运行的真实模型矩阵。三个 required OBJ
分别确认 8409、19270、5592 对 self-intersection，并在 mutation 前 fail-fast；`aishen_fudiao` 另有 20 对
coplanar overlap。闭合 Texture2D 3MF 保持 no-op strict PASS，validator 与属性保持通过。任务证据 4/4 完整，
production Gate 0/4 通过，所有 case 均未写 production output。R3-03 已完成非生产 Release/legacy 回归，
R3-04 已输出 NO-GO，12E-08D 继续 BLOCKED。

## 28. 12E-08C-R3-03/R3-04 实际结果

Release build 与 CTest 37/37 PASS。三个 OBJ global core 为 `skipped_due_topology`；Texture2D 3MF 的
partition、texture transfer、raster mapping、full closure PASS。repair-disabled TIFF SHA-256 invariant
和 RIP strict PASS；Quick CI 如实保留 `material_process_top2 widthPx=48/226` known baseline。R3-04 因
required OBJ 0/3 strict admitted、预算未冻结而输出 NO-GO。
