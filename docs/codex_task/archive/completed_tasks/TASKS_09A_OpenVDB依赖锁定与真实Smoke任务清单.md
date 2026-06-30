# TASKS_09A_OpenVDB依赖锁定与真实Smoke任务清单

> 文档版本：v0.2  
> 文档状态：Codex Task List  
> 适用阶段：09A  
> 建议提交目录：`docs/slicer/`

---

## Milestone 09A-0：阅读确认

- [x] 阅读 `REPORT_09_OpenVDB_SDF几何内核预研当前状态.md`
- [x] 阅读 `docs/slicer/OPENVDB_DEPENDENCY_NOTES.md`
- [x] 确认 `USE_OPENVDB=ON` 当前失败原因
- [x] 确认不修改 production slicer path

---

## Milestone 09A-1：依赖方案选择

- [x] 优先选择 vcpkg manifest mode
- [x] 新增或更新 `vcpkg.json`
- [x] 记录 triplet
- [x] 记录 Debug/Release 注意事项

---

## Milestone 09A-2：CMake 错误提示增强

- [x] 保持 `USE_OPENVDB=OFF` 默认
- [x] `USE_OPENVDB=ON` 找不到 OpenVDB 时输出可操作提示
- [x] 支持 OpenVDB::openvdb / OpenVDB::OpenVDB target
- [x] 不影响 OFF 构建

---

## Milestone 09A-3：配置脚本

- [x] 新增 `scripts/configure_openvdb_vcpkg.ps1`
- [x] 支持 `-VcpkgRoot`
- [x] 支持 `-BuildDir`
- [x] 支持 `-Triplet`

---

## Milestone 09A-4：Smoke 脚本

- [x] 新增 `scripts/run_openvdb_smoke.ps1`
- [x] 构建 `geometry_kernel_demo`
- [x] 执行 `openvdb-smoke`
- [x] 校验 report openvdb.enabled / available
- [x] 校验 activeVoxels > 0

说明：脚本能力已实现；前次真实 ON smoke 使用了错误的 `C:\vcpkg` 路径。本机应使用 `VCPKG_ROOT=D:\Program Files Tools\vcpkg` 重新执行。

---

## Milestone 09A-5：OpenVdbAdapter 增强

- [x] 输出 version
- [x] 输出 activeVoxels
- [x] 输出 grid metadata
- [x] 输出 warnings
- [x] 写入 geometry_kernel_report

---

## Milestone 09A-6：Dependency Notes

- [x] 更新 `OPENVDB_DEPENDENCY_NOTES.md`
- [x] 记录 OFF 结果
- [x] 记录 ON 结果
- [x] 记录失败/成功日志摘要
- [x] 给出推荐安装方案

---

## Milestone 09A-7：最终验证

- [x] OFF build
- [x] OFF run_ci_quick
- [x] ON configure attempted；本机失败原因已记录
- [x] ON build attempted through smoke script；因 ON configure 未完成而失败，原因已记录
- [x] ON openvdb-smoke attempted through smoke script；因 ON configure 未完成而失败，原因已记录
- [x] 生成 `REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态.md`
