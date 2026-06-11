# TASKS_09_v0.2_OpenVDB_SDF几何内核采用预研任务清单

> 文档版本：v0.2  
> 文档状态：Codex Task List  
> 适用阶段：09  
> 建议提交目录：`docs/slicer/`

---

## Milestone 09-0：分支与阅读确认

- [x] 从 `r1-architecture-refactor` 切出 `spike/09-openvdb-sdf-kernel`
- [x] 阅读 `REPORT_08A_支撑桥接Fixture单测与真实模型Profile当前状态.md`
- [x] 确认不修改 production slicer path
- [x] 确认不修改 p0.rgbwsv.2
- [x] 确认 OpenVDB 不是默认强制依赖

---

## Milestone 09-1：Geometry Kernel 目录

- [x] 新增 `src/slicer_core/geometry/`
- [x] 新增 `apps/geometry_kernel_demo/`
- [x] CMake 增加 `ENABLE_GEOMETRY_KERNEL_DEMO`
- [x] CMake 增加 `USE_OPENVDB=OFF` 默认

---

## Milestone 09-2：DistanceField2D

- [x] 新增 `DistanceField2D.*`
- [x] 支持 binary mask
- [x] 支持 nearest boundary distance
- [x] 输出 distance stats
- [x] 第一版优先 correctness，不追求性能

---

## Milestone 09-3：ShellMask

- [x] 新增 `ShellMask.*`
- [x] 支持 shellThicknessPx / shellThicknessMm
- [x] 输出 shell/interior/boundary stats
- [x] 不接入 production material composition

---

## Milestone 09-4：GeometryKernelReport

- [x] 新增 `GeometryKernelReport.*`
- [x] 输出 `p0.geometry_kernel_report.1`
- [x] 包含 openvdb status
- [x] 包含 distanceStats / shellStats / warnings / timings

---

## Milestone 09-5：OpenVDB Adapter

- [x] 新增 `OpenVdbAdapter.*`
- [x] `USE_OPENVDB=OFF` 时返回 unavailable/stub
- [x] `USE_OPENVDB=ON` 时尝试真实 OpenVDB smoke
- [x] 不影响默认构建
- [x] 不强制引入外部依赖

---

## Milestone 09-6：geometry_kernel_demo

- [x] 支持 `--case heightfield-sdf`
- [x] 支持 `--case surface-shell`
- [x] 支持 `--case openvdb-smoke`
- [x] 支持 `--case compensated-varnish` graceful stub
- [x] 输出 report 与 preview

---

## Milestone 09-7：Dependency Notes

- [x] 新增 `docs/slicer/OPENVDB_DEPENDENCY_NOTES.md`
- [x] 记录 OpenVDB 安装方式
- [x] 记录 CMake 查找方式
- [x] 记录依赖库
- [x] 记录 USE_OPENVDB=ON 验证结果
- [x] 记录风险与推荐方案

---

## Milestone 09-8：测试脚本

- [x] 新增 `scripts/run_geometry_kernel_tests.ps1`
- [x] 执行 heightfield-sdf
- [x] 执行 surface-shell
- [x] 执行 openvdb-smoke
- [x] 校验 report schema
- [x] 默认不阻塞 run_ci_quick

---

## Milestone 09-9：状态报告

- [x] 生成 `REPORT_09_OpenVDB_SDF几何内核预研当前状态.md`
- [x] 判断是否进入 09A / 09B / 09C
