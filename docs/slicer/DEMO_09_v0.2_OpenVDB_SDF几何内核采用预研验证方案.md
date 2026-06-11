# DEMO_09_v0.2_OpenVDB_SDF几何内核采用预研验证方案

> 文档版本：v0.2  
> 文档状态：Demo / 验证方案  
> 适用阶段：09  
> 建议提交目录：`docs/slicer/`

---

## 1. 验证目标

验证 geometry kernel prototype 可以独立运行，并验证 OpenVDB 采用可行性，不影响当前生产 slicer pipeline。

---

## 2. 分支准备

```bash
git checkout r1-architecture-refactor
git pull
git checkout -b spike/09-openvdb-sdf-kernel
```

---

## 3. 默认构建验证：USE_OPENVDB=OFF

```powershell
cmake --build build --config Debug

cmake --build build --config Debug --target geometry_kernel_demo

.\build\Debug\geometry_kernel_demo.exe --case heightfield-sdf --output output\GeometryKernelDemo

.\build\Debug\geometry_kernel_demo.exe --case surface-shell --shell-mm 0.05 --output output\GeometryKernelShell

.\build\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdbStub

.\scripts\run_geometry_kernel_tests.ps1

.\scripts\run_ci_quick.ps1
```

期望：

```text
主项目构建通过；
pure-cpp cases 通过；
openvdb-smoke graceful skip 或 stub pass；
run_ci_quick.ps1 通过。
```

---

## 4. OpenVDB ON 验证

在已安装 OpenVDB 的环境中执行：

```powershell
cmake -S . -B build-openvdb -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON
cmake --build build-openvdb --config Debug --target geometry_kernel_demo
.\build-openvdb\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdb
```

如构建失败，也必须记录到：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

失败不应影响默认 `USE_OPENVDB=OFF` 主线构建。

---

## 5. 验收 Checklist

- [ ] `spike/09-openvdb-sdf-kernel` 分支已建立。
- [ ] `geometry_kernel_demo` 可构建。
- [ ] `USE_OPENVDB=OFF` 可构建。
- [ ] `heightfield-sdf` case 返回 0。
- [ ] `surface-shell` case 返回 0。
- [ ] `openvdb-smoke` 在 OFF 下 graceful skip 或 stub pass。
- [ ] `USE_OPENVDB=ON` 至少完成一次验证或形成依赖失败记录。
- [ ] 输出 `geometry_kernel_report.json`。
- [ ] `geometry_kernel_report.schema = p0.geometry_kernel_report.1`。
- [ ] 输出 preview PNG。
- [ ] `run_ci_quick.ps1` 仍通过。
- [ ] `slicer_cli` / `rip_reader_test` 不受影响。
- [ ] RGBWSV 输出协议不变。
