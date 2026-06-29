# TASKS_09B_R3_R2后续原子任务与阶段任务清单

> 文档版本：v0.2
> 文档状态：已按当前实现更新
> 建议分支：`spike/09B-R3-shell-production-readiness`
> 说明：未勾选项表示本轮未执行或仍不满足验收，不用“看起来完成”掩盖边界。

## Stage 09B-R3 总目标

在 09B-R2 已完成真实模型鲁棒性与性能基线的基础上，补齐 production 接入前仍缺失的关键诊断与策略能力。

本阶段不接入 production `slicer_cli`，不写 production RGBWSV TIFF。

---

## Milestone 09B-R3-0：分支与文档同步

- [ ] 确认 `spike/09B-R2-shell-robustness-performance` 已提交并推送。
  - 当前仅确认本地 R2 分支存在；未执行远端 push/pull 验证。
- [x] 从 R2 分支切出 `spike/09B-R3-shell-production-readiness`。
- [x] 确认 R2 report、MASTER PRD、MASTER DEV、R3 task/prompt 文档存在。
- [ ] 工作树 clean。
  - 当前仍存在用户/工具遗留 `.specstory` 与 3MF 样例二进制改动，未纳入本阶段处理。

## Milestone 09B-R3-1：实现 narrow-phase triangle-triangle self-intersection

- [x] 新增 `TriangleIntersectionQuery.h/.cpp`。
- [x] 定义 `TriangleIntersectionResult` 与 `TriangleIntersectionKind`。
- [x] 保留 AABB broad phase candidate。
- [x] 实现 narrow-phase triangle-triangle intersection。
- [x] 排除共享 vertex index 的相邻面。
- [x] 区分 confirmed intersection / coplanar overlap / touching only / AABB false positive。
- [x] `MeshRobustnessReport` 新增 candidate / confirmed / coplanar / touching / false positive / sampled 字段。
- [x] 保留 sampled / maxPairs / maxPairChecks 策略。
- [x] report v2 输出新增字段。
- [x] 单元测试覆盖明显相交、AABB false positive、共面重叠、共享边不算自交、sampled。

## Milestone 09B-R3-2：引入稳定 ValidationErrorCode / WarningCode

- [x] 新增 `src/slicer_core/diagnostics/ValidationIssue.*`。
- [x] 定义 `ValidationSeverity`。
- [x] 定义 `ValidationIssue`：code / severity / message / context。
- [x] 输出稳定 code：
  - `MESH_BOUNDARY_EDGES`
  - `MESH_NON_MANIFOLD_EDGES`
  - `MESH_DUPLICATE_FACES`
  - `MESH_OPPOSITE_DUPLICATE_FACES`
  - `MESH_LOCAL_WINDING_INCONSISTENCY`
  - `MESH_SELF_INTERSECTION_CONFIRMED`
  - `MESH_SELF_INTERSECTION_SAMPLED`
  - `MESH_THIN_FEATURE_EDGE`
  - `MESH_THIN_FEATURE_AREA`
  - `TEXTURE_MISSING`
  - `TEXTURE_UV_MISSING`
  - `TEXTURE_UV_OUT_OF_RANGE`
  - `OPENVDB_UNAVAILABLE`
  - `OPENVDB_LEVEL_SET_FAILED`
- [x] 保留 human-readable message。
- [x] report 新增 `issues` / `warningCodes` / `errorCodes`。
- [x] robustness script 改为通过 code 验证负向 fixture。
- [x] 新增 R3 golden expected JSON。

## Milestone 09B-R3-3：建立 repeat/wrap 纹理 fixture

- [x] 新增 `samples/models/openvdb/surface_shell_repeat_texture.obj`。
- [x] 新增 `samples/models/openvdb/surface_shell_repeat_texture.mtl`。
- [x] 新增 `samples/configs/openvdb/surface_shell_repeat_texture.json`。
- [x] 新增 clamp 对照配置 `surface_shell_repeat_texture_clamp.json`。
- [x] 复用 `samples/models/textured/textures/checker.png`。
- [x] report 输出 `uvOutOfRangeVoxels` / `sampler` / `uvAddressMode` / `repeatedSampledVoxels`。
- [x] robustness script 验证 repeat 与 clamp 差异。

## Milestone 09B-R3-4：接入 Windows process peak working set

- [x] 新增 `ProcessMemoryStats` 模块。
- [x] Windows 下使用 `GetProcessMemoryInfo`。
- [x] CMake Windows 链接 `psapi`。
- [x] 非 Windows 返回 unavailable，不失败。
- [x] texture report 输出 `processPeakWorkingSetBytes` / `processPeakWorkingSetAvailable` / `processWorkingSetBytes`。
- [x] benchmark report 输出 OS 级峰值。
- [x] 文档区分结构估算内存与 OS 级进程峰值。

## Milestone 09B-R3-5：真实模型拓扑生产准入策略评估

- [x] 对真实 OBJ golden 做 topology issue summary。
- [x] 对真实 3MF golden 做 topology issue summary。
- [x] 分析 issue 是否可自动修复 / 拒绝 / diagnostic-only / warn_and_attempt。
- [x] 输出 strict_closed / repair_then_strict / warn_and_attempt / diagnostic_only 策略表。
- [x] 不实现完整自动修复，只形成策略判断。
- [x] 生成 `DOC_DECISION_09B_R3_真实模型拓扑生产准入策略.md`。

## Milestone 09B-R3-6：Benchmark 扩展与阈值建议

- [x] 尝试 100k triangle fixture。
- [x] 100k fixture 在当前环境通过。
- [x] benchmark summary 包含 runtime trend / memory trend / BVH 指标。
- [x] 不使用 strict time equality。
- [x] 建议 soft gate：10k 必须通过；50k 建议通过；100k 可选或 nightly。
- [x] benchmark report 输出 process memory。

## Milestone 09B-R3-7：全量回归

已执行并通过：

- [x] OpenVDB R3 Debug configure。
- [x] `run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3`。
- [x] `run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3 -RunMatrix`。
- [x] `run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3 -RunRealModels`。
- [x] `run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09b-r3`。
- [x] `run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b-r3`。
- [x] `run_openvdb_smoke.ps1 -BuildDir build-openvdb-09b-r3`。
- [x] OpenVDB R3 Release configure。
- [x] `run_surface_shell_benchmarks.ps1 -BuildDir build-openvdb-09b-r3-release -Config Release`。
- [x] `cmake --build build --config Debug`。
- [x] `run_ci_quick.ps1`。

## Milestone 09B-R3-8：状态报告

- [x] 生成 `docs/slicer/REPORT_09B_R3_壳层纹理生产准入前诊断策略收口当前状态.md`。
- [x] 报告判断是否进入 09P。
- [x] 报告判断是否需要 09B-R4。
- [x] 报告判断是否可以并行 09C。
- [x] 报告确认 production RGBWSV 未被修改。
