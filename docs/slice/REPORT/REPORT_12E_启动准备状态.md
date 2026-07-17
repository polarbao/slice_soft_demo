# REPORT_12E 启动准备状态

> 文档状态：12E-03 COMPLETE / 12E-04 PREPARED
> 日期：2026-07-17

## 1. 当前结论

12E 已完成 12E-01、12E-02 与 12E-03。除配置和服务契约外，现已建立默认 `USE_OPENVDB=OFF` 可运行的 Legacy CPU 全模型三维 occupancy、最近表面欧氏距离和纹理/填充互补分区候选。

当前候选仍为 diagnostic-only，不接入 composer 或生产写包。`12E-04 OpenVDB Conformance Adapter` 的同 grid 采样、OFF/ON 行为、距离完整性、差异 DTO、依赖和测试边界已准备完成，等待用户明确启动。

## 2. Current State

```text
12A：材料填充、支撑、光油语义当前范围完成；
12B：性能评估与 OpenVDB SDF utility 定位完成；
12C：Qt 工作台 R0/R1/R2 完成；
12D：R0/R1/R2/R3 COMPLETE，12D-10 三个真实 OBJ 验收通过；
12E：R0 complete，12E-01/02/03 complete，12E-04 prepared。
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
12E-04 OpenVDB conformance adapter 准备。
```

## 4. 尚未实现

```text
OpenVDB conformance adapter；
width sweep validator；
closest-surface texture transfer；
12D closure 接入；
production admission；
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
| 12E-04 | PREPARED / READY FOR USER ADMISSION | OpenVDB OFF/ON conformance adapter |
| 12E-05..07 | BLOCKED BY PREVIOUS TASK | 按原子任务顺序推进 |
| 12E-08 | REQUIRES EXPLICIT PRODUCTION CONFIRMATION | 涉及 production path |
| 12E-09..10 | PLANNED | UI、真实模型和收口 |

## 6. 与 12D 的关系

```text
12D-07/08/09：COMPLETE；
12D-10：COMPLETE；
12E-01：COMPLETE；
12E-02：COMPLETE；
12E-03：COMPLETE；
12E-04：PREPARED / READY FOR USER ADMISSION；
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
developer warning，没有影响本任务目标或测试结果。该轨道只验证 CPU candidate 在 OpenVDB ON
构建中的兼容性，不代表 12E-04 OpenVDB conformance backend 已实现。

一次 Debug generated closed-box 观测值：

```text
gridVoxels=1728；modelVoxels=1000；
topologyMs=0.5792；occupancyMs=3.4379；distanceMs=3.1322；
partitionMs=0.0159；totalCoreMs=7.5128；
processPeakWorkingSetBytes=5509120。
```

该值只证明计时和内存字段已实际产生，不是 Release 性能门槛，也不能外推到真实甲片模型。

`scripts/run_ci_quick.ps1` 本次没有通过：回归脚本对 `obj_mtl_texture_rgb_white_varnish.json` 仍期待 `output/ObjMtlTextureRgbWhiteVarnish`，但该配置当前写入 `output/ProfileTexturedNailRgbWhiteLowerSupport`，随后 RIP Reader 返回 `E_PACKAGE_NOT_FOUND`。12E-03 不进入旧 production pipeline，本任务未修改该既有 config/script 映射；该失败不记录为 12E-03 PASS，也未在本任务中扩展范围修复。

## 11. 下一任务

12E-03 已完成。下一步应明确指定：

```text
开始 12E-04 OpenVDB Conformance Adapter。
```

该任务只建立 OpenVDB OFF/ON conformance backend、同 grid candidate 和 CPU/OpenVDB 差异证据，不接入 Qt、composer 或生产写包。

## 12. 安全边界

```text
p0.rgbwsv.2 不变；
R G B W S V 不变；
uint8 / black_is_print 不变；
OpenVDB optional/OFF；
legacy slicer_cli production path 不替代；
12D repair 默认关闭；
没有 production admission 时不写 12E production TIFF。
```

## 13. 后续准备复核

2026-07-17 已完成 12E-04 准入准备复核：

```text
12E-03 已提供 CPU 基准 candidate 和统一结果 DTO；
OpenVDB adapter 必须对 request grid 的 cell center 做 world-space SDF sample，不能直接复用 scan-bounds mask；
窄带距离不足必须显式处理，建议 OpenVDB 负责 occupancy、NearestTriangleQuery 负责同口径距离；
默认 OFF 返回 unavailable，ON 继续使用 D:\vcpkg-openvdb 独立 lane；
box/sphere/sloped/thin-wall/cavity/topology 和 OFF/ON conformance 验证已定义；
12E-04 已准备，但不能由本次准备复核自动进入代码实现。
```
