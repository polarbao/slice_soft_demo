# TASKS_09A_R2_OpenVDB无空格依赖根与真实Smoke任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：09A-R2  
> 建议提交目录：`docs/slicer/`

---

## Milestone 09A-R2-0：阅读确认

- [x] 阅读 `REPORT_09A_R1_OpenVDB真实环境复测当前状态.md`
- [x] 阅读 `OPENVDB_DEPENDENCY_NOTES.md`
- [x] 确认失败根因是 vcpkg root 包含空格
- [x] 确认本阶段不执行 09B

---

## Milestone 09A-R2-1：准备无空格 vcpkg root

- [x] 选择 `D:\vcpkg-openvdb`
- [x] clone vcpkg
- [x] bootstrap-vcpkg
- [x] 验证 toolchain file 存在
- [x] 验证 vcpkg.exe 可运行

---

## Milestone 09A-R2-2：脚本复查

- [x] `configure_openvdb_vcpkg.ps1` 支持显式 `-VcpkgRoot`
- [x] 对含空格 root 继续输出 warning
- [x] 使用干净 BuildDir
- [x] 不改 production build 目录

---

## Milestone 09A-R2-3：ON Configure

执行：

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-r2 `
  -Triplet x64-windows
```

- [x] configure 返回 0
- [x] CMakeCache.txt 生成
- [x] build files 生成
- [x] OpenVDB package 被找到

---

## Milestone 09A-R2-4：ON Build 与 Smoke

执行：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2
```

- [x] geometry_kernel_demo build 成功
- [x] openvdb-smoke 返回 0
- [x] openvdb.enabled = true
- [x] openvdb.available = true
- [x] activeVoxels > 0
- [x] OpenVDB version 非空

---

## Milestone 09A-R2-5：OFF 回归

- [x] `cmake --build build --config Debug`
- [x] `run_ci_quick.ps1`
- [x] production slicer_cli 不受影响
- [x] RGBWSV 协议不变

---

## Milestone 09A-R2-6：文档与状态报告

- [x] 更新 `OPENVDB_DEPENDENCY_NOTES.md`
- [x] 记录新 VCPKG_ROOT
- [x] 记录 OpenVDB port/version
- [x] 记录 configure/build/smoke 结果
- [x] 生成 `REPORT_09A_R2_OpenVDB真实Smoke收口当前状态.md`
- [x] 判断是否进入 09B
