# CODEX_PROMPT_09A_R1_OpenVDB真实环境复测执行指令

> 文档版本：v0.1
> 用途：复制给 VS Code Codex
> 适用阶段：09A-R1
> 建议提交目录：`docs/slicer/`

请继续使用当前分支：

```text
spike/09-openvdb-sdf-kernel
```

先阅读：

```text
docs/slicer/REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态.md
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
docs/slicer/DOC_DECISION_09A_R1_OpenVDB真实环境复测与依赖锁定修正.md
docs/slicer/TASKS_09A_R1_OpenVDB真实环境复测任务清单.md
docs/slicer/DEMO_09A_R1_OpenVDB真实Smoke复测验证方案.md
```

当前阶段不是 09B，而是：

```text
09A-R1：OpenVDB 真实环境复测与依赖锁定修正
```

目标：

```text
1. 使用正确的 VCPKG_ROOT 重新执行 USE_OPENVDB=ON configure；
2. 跑通 geometry_kernel_demo ON build；
3. 跑通 openvdb-smoke；
4. 确认 activeVoxels > 0；
5. 更新 OPENVDB_DEPENDENCY_NOTES.md；
6. 保持 USE_OPENVDB=OFF 和 run_ci_quick.ps1 通过；
7. 生成 REPORT_09A_R1。
```

执行命令：

```powershell
$env:VCPKG_ROOT = "D:\Program Files Tools\vcpkg"

.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows

.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb

cmake --build build --config Debug

.\scripts\run_ci_quick.ps1
```

如果 configure/build/smoke 失败，不要继续做 09B。请：

```text
1. 记录失败原因；
2. 更新 OPENVDB_DEPENDENCY_NOTES.md；
3. 生成 REPORT_09A_R1；
4. 判断是否需要 09A-R2。
```

必须保持：

```text
不修改 production slicer_cli
不修改 RGBWSV 输出协议
不替换 SupportShapePipeline
不实现 production surface_shell_texture
不实现 production compensated_varnish
```

完成后生成：

```text
docs/slicer/REPORT_09A_R1_OpenVDB真实环境复测当前状态.md
```

报告必须包含：

```text
1. 实际 VCPKG_ROOT；
2. configure 结果；
3. build 结果；
4. openvdb-smoke 结果；
5. activeVoxels；
6. OpenVDB version；
7. OFF run_ci_quick 结果；
8. 是否进入 09B 的判断。
```
