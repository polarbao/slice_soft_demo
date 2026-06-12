# TASKS_09B_OpenVDB_SDF表面壳层纹理任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：09B  
> 建议提交目录：`docs/slicer/`

---

## Milestone 09B-0：阶段与分支

- [x] 阅读 `REPORT_09A_R2_OpenVDB真实Smoke收口当前状态.md`
- [x] 确认 ON smoke / activeVoxels / version 均通过
- [x] 从 09A 分支切出 `spike/09B-openvdb-surface-shell-texture`
- [x] 确认 production pipeline 不在本阶段修改范围

---

## Milestone 09B-1：Mesh Fixture

- [x] 新增 `TriangleMeshData`
- [x] 新增 generated box fixture
- [ ] 可选新增 generated sphere fixture
- [x] 空 mesh fail fast
- [x] 非法 triangle index fail fast

---

## Milestone 09B-2：OpenVDB Level Set

- [x] 新增 `OpenVdbLevelSetBuilder.*`
- [x] 支持 voxelSizeMm
- [x] 支持 interior/exterior band
- [x] 设置 level set grid class
- [x] 输出 active voxels / bbox / transform
- [x] OFF 路径 graceful unavailable

---

## Milestone 09B-3：Surface Shell Classifier

- [x] 新增 `OpenVdbSurfaceShell.*`
- [x] 生成 inside mask
- [x] 生成 shell mask
- [x] 生成 interior mask
- [x] 保证 shell/interior 不重叠
- [x] 保证 shell + interior = inside
- [x] 外部不写 RGB
- [x] 处理 inactive deep interior 语义

---

## Milestone 09B-4：Shell Texture Prototype

- [x] 新增 `SurfaceShellTexturePrototype.*`
- [x] 支持 constant RGB
- [x] 支持 checker RGB
- [x] interior fill role = base
- [x] 不接入 production MaterialPolicy
- [x] 不写 production TIFF

---

## Milestone 09B-5：Report 与 Preview

- [x] 新增 `SurfaceShellTextureReport.*`
- [x] schema = `p0.surface_shell_texture_report.1`
- [x] 输出 shell/interior/inside/outside stats
- [x] 输出 thickness/voxel size/openvdb metadata
- [x] 输出 shell/interior/composite preview

---

## Milestone 09B-6：Demo 与 Unit Tests

- [x] 新增 `surface_shell_texture_demo`
- [x] 新增 `surface_shell_texture_unit_tests`
- [x] generated-box case 通过
- [x] thickness monotonic test 通过
- [x] outsideColoredVoxels = 0
- [x] invalid config tests 通过

---

## Milestone 09B-7：脚本与回归

- [x] 新增 `run_surface_shell_texture_tests.ps1`
- [x] 使用 build-openvdb-09b
- [x] 校验 report schema
- [x] 校验 preview
- [x] 执行 OFF build
- [x] 执行 OFF run_ci_quick

---

## Milestone 09B-8：状态报告

- [x] 生成 `REPORT_09B_OpenVDB_SDF表面壳层纹理原型当前状态.md`
- [x] 判断是否进入 09B-R1 / 09C / 09P
