# CODEX_PROMPT_09A_OpenVDB依赖锁定与真实Smoke执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 适用阶段：09A  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_09_OpenVDB_SDF几何内核预研当前状态.md
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
docs/slicer/DOC_DECISION_09A_09后进入OpenVDB依赖锁定与真实Smoke阶段.md
docs/slicer/PRD_09A_OpenVDB依赖锁定与真实Smoke.md
docs/slicer/DEV_09A_OpenVDB依赖接入与Smoke设计.md
docs/slicer/DEMO_09A_OpenVDB依赖锁定与真实Smoke验证方案.md
docs/slicer/TASKS_09A_OpenVDB依赖锁定与真实Smoke任务清单.md
```

当前阶段：

```text
09A：OpenVDB 依赖锁定与真实 Smoke
```

目标：

```text
1. 确定 OpenVDB 依赖接入方案；
2. 优先实现 vcpkg manifest mode；
3. 保持 USE_OPENVDB=OFF 默认构建通过；
4. 让 USE_OPENVDB=ON 至少在一个目标环境可复现构建；
5. 让 openvdb-smoke 真实执行；
6. 更新 OPENVDB_DEPENDENCY_NOTES.md；
7. 不接入 production slicer_cli 输出链路。
```

必须保持：

```text
p0.rgbwsv.2 输出协议不变
R G B W S V 通道顺序不变
8-bit / black_is_print 不变
Model > Support > Empty 不变
当前 support shape pipeline 不替换
USE_OPENVDB=OFF 默认仍可构建
```

不要做：

```text
不要实现 production surface_shell_texture；
不要实现 production compensated_varnish；
不要把 VDB/SDF 接入生产 RGBWSV 输出；
不要替换 SupportShapePipeline；
不要做设备通信；
不要做 RIP 半色调；
不要让 OpenVDB 成为所有开发环境强制依赖。
```

执行顺序：

```text
09A-0：确认 REPORT_09 中 ON 失败原因；
09A-1：新增/确认 vcpkg.json；
09A-2：增强 CMake OpenVDB 错误提示；
09A-3：新增 configure_openvdb_vcpkg.ps1；
09A-4：新增 run_openvdb_smoke.ps1；
09A-5：增强 OpenVdbAdapter smoke report；
09A-6：更新 OPENVDB_DEPENDENCY_NOTES.md；
09A-7：执行 OFF/ON 验证；
09A-8：生成 REPORT_09A。
```

必须执行验证：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot C:\vcpkg -BuildDir build-openvdb -Triplet x64-windows
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

完成后生成：

```text
docs/slicer/REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态.md
```

报告必须包含：

```text
1. OFF 构建结果；
2. OFF CI quick 结果；
3. ON configure 结果；
4. ON build 结果；
5. openvdb-smoke 结果；
6. activeVoxels；
7. OpenVDB version；
8. dependency notes；
9. 是否进入 09B surface shell texture prototype。
```
