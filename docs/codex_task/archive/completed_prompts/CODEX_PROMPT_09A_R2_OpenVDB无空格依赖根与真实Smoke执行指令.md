# CODEX_PROMPT_09A_R2_OpenVDB无空格依赖根与真实Smoke执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 适用阶段：09A-R2  
> 建议提交目录：`docs/slicer/`

---

继续使用分支：

```text
spike/09-openvdb-sdf-kernel
```

先阅读：

```text
docs/slicer/REPORT_09A_R1_OpenVDB真实环境复测当前状态.md
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
docs/slicer/DOC_DECISION_09A_R2_OpenVDB无空格依赖根与真实Smoke收口.md
docs/slicer/VCPKG_OPENVDB_NOSPACE_BOOTSTRAP_GUIDE.md
docs/slicer/TASKS_09A_R2_OpenVDB无空格依赖根与真实Smoke任务清单.md
docs/slicer/DEMO_09A_R2_OpenVDB真实Smoke收口验证方案.md
```

当前阶段：

```text
09A-R2：OpenVDB 无空格依赖根、真实 ON 构建与 Smoke 收口
```

不要进入 09B，除非真实 OpenVDB ON smoke 成功。

执行顺序：

```text
09A-R2-0：确认 hwloc 失败来自含空格 vcpkg root
09A-R2-1：建立 D:\vcpkg-openvdb
09A-R2-2：bootstrap vcpkg
09A-R2-3：设置 VCPKG_ROOT
09A-R2-4：使用全新 build-openvdb-r2 configure
09A-R2-5：执行 ON build 与 openvdb-smoke
09A-R2-6：执行 OFF build 与 CI quick
09A-R2-7：更新 dependency notes
09A-R2-8：生成 REPORT_09A_R2
```

建议命令：

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\vcpkg-openvdb

& "D:\vcpkg-openvdb\bootstrap-vcpkg.bat"

$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

if (Test-Path .\build-openvdb-r2) {
    Remove-Item -Recurse -Force .\build-openvdb-r2
}

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-r2 `
  -Triplet x64-windows

.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2

cmake --build build --config Debug

.\scripts\run_ci_quick.ps1
```

必须保持：

```text
不修改 production slicer_cli
不修改 p0.rgbwsv.2
不替换 SupportShapePipeline
不实现 production surface_shell_texture
不实现 production compensated_varnish
USE_OPENVDB=OFF 默认构建继续通过
```

完成后生成：

```text
docs/slicer/REPORT_09A_R2_OpenVDB真实Smoke收口当前状态.md
```

报告必须包含：

```text
VCPKG_ROOT
vcpkg commit/version
OpenVDB port/version
ON configure 结果
ON build 结果
openvdb-smoke 结果
activeVoxels
OpenVDB version
OFF run_ci_quick 结果
是否进入 09B
```
