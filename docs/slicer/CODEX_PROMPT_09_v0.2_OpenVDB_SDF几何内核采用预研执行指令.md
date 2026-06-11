# CODEX_PROMPT_09_v0.2_OpenVDB_SDF几何内核采用预研执行指令

> 文档版本：v0.2  
> 用途：复制给 VS Code Codex  
> 适用阶段：09  
> 建议提交目录：`docs/slicer/`

---

请先从当前稳定分支切出新分支：

```bash
git checkout r1-architecture-refactor
git pull
git checkout -b spike/09-openvdb-sdf-kernel
```

然后阅读：

```text
docs/slicer/REPORT_08A_支撑桥接Fixture单测与真实模型Profile当前状态.md
docs/slicer/DOC_DECISION_09_v0.2_08A后进入OpenVDB_SDF几何内核采用预研阶段.md
docs/slicer/PRD_09_v0.2_OpenVDB_SDF几何内核采用预研.md
docs/slicer/DEV_09_v0.2_GeometryKernel_OpenVDB_SDF采用预研设计.md
docs/slicer/DEMO_09_v0.2_OpenVDB_SDF几何内核采用预研验证方案.md
docs/slicer/TASKS_09_v0.2_OpenVDB_SDF几何内核采用预研任务清单.md
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

当前阶段：

```text
09：OpenVDB / SDF 几何内核采用预研
```

目标：

```text
1. 新增 experimental geometry kernel boundary；
2. 新增 DistanceField2D；
3. 新增 ShellMask；
4. 新增 GeometryKernelReport；
5. 新增 OpenVDB adapter；
6. 新增 geometry_kernel_demo；
7. 验证 USE_OPENVDB=OFF 默认构建；
8. 验证 USE_OPENVDB=ON 真实 OpenVDB 构建或记录失败；
9. 新增 run_geometry_kernel_tests.ps1；
10. 不影响 production slicer pipeline。
```

必须保持：

```text
p0.rgbwsv.2 输出协议不变
R G B W S V 通道顺序不变
8-bit / black_is_print 不变
Model > Support > Empty 不变
当前 support shape pipeline 不替换
OpenVDB 不是默认强制依赖
```

不要做：

```text
不要把 SDF/OpenVDB 接入生产 slicer_cli；
不要实现 production surface_shell_texture；
不要实现 production compensated_varnish；
不要替换当前 support optimizer；
不要引入设备通信；
不要做 RIP 半色调；
不要做 ICC / CMYK；
不要让 USE_OPENVDB=ON 破坏默认 OFF 构建。
```

执行顺序：

```text
09-0：新建 spike/09-openvdb-sdf-kernel 分支
09-1：新增 geometry kernel 目录和 demo target
09-2：实现 DistanceField2D
09-3：实现 ShellMask
09-4：实现 GeometryKernelReport
09-5：实现 OpenVdbAdapter stub / ON adapter
09-6：实现 geometry_kernel_demo cases
09-7：新增 run_geometry_kernel_tests.ps1
09-8：记录 OPENVDB_DEPENDENCY_NOTES.md
09-9：生成 REPORT_09
```

必须执行验证：

```powershell
cmake --build build --config Debug
cmake --build build --config Debug --target geometry_kernel_demo
.\build\Debug\geometry_kernel_demo.exe --case heightfield-sdf --output output\GeometryKernelDemo
.\build\Debug\geometry_kernel_demo.exe --case surface-shell --shell-mm 0.05 --output output\GeometryKernelShell
.\build\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdbStub
.\scripts\run_geometry_kernel_tests.ps1
.\scripts\run_ci_quick.ps1
```

如果本机已具备 OpenVDB 环境，额外执行：

```powershell
cmake -S . -B build-openvdb -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON
cmake --build build-openvdb --config Debug --target geometry_kernel_demo
.\build-openvdb\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdb
```

完成后生成：

```text
docs/slicer/REPORT_09_OpenVDB_SDF几何内核预研当前状态.md
```

报告必须包含：

```text
1. 分支信息；
2. USE_OPENVDB=OFF 构建与运行结果；
3. USE_OPENVDB=ON 构建与运行结果或失败记录；
4. GeometryKernelDemo 输出；
5. geometry_kernel_report schema；
6. preview 输出；
7. 对 production slicer pipeline 的影响结论；
8. 是否进入 09A / 09B / 09C 的判断。
```
