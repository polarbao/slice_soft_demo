# REPORT_09_OpenVDB_SDF几何内核预研当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-11  
> 适用阶段：09

---

## 1. 阶段结论

09 已完成 OpenVDB / SDF 几何内核采用预研的第一轮隔离实现。

本阶段新增 experimental geometry kernel 通道，默认 `USE_OPENVDB=OFF` 可构建和运行；`USE_OPENVDB=ON` 已在本机尝试配置并记录依赖缺失失败。当前实现不接入生产 `slicer_cli` 主流程，不修改 RGBWSV 输出，不替换 SupportShapePipeline。

证据等级：

- [A] 当前代码新增 `src/slicer_core/geometry`。
- [A] 当前代码新增 `geometry_kernel_demo` target。
- [A] `heightfield-sdf` / `surface-shell` / `openvdb-smoke` 在默认 OFF 下可运行。
- [A] `geometry_kernel_report.json` 输出 `schema = p0.geometry_kernel_report.1`。
- [A] preview PNG 已生成。
- [A] `USE_OPENVDB=ON` 已执行配置验证并形成失败记录。
- [A] 默认 Debug build 已通过。
- [A] `run_ci_quick.ps1` 已通过，生产链路未被 09 改动破坏。
- [B] OpenVDB 真实依赖接入仍待 vcpkg / Conan / source build 环境验证。

---

## 2. 分支信息

当前分支：

```text
spike/09-openvdb-sdf-kernel
```

说明：

- 从 `r1-architecture-refactor` 当前工作树切出。
- 未执行 `git pull`，因为切分支前工作区存在用户新增 09 文档和 `.specstory` 未跟踪文件。

---

## 3. 新增模块

新增：

```text
src/slicer_core/geometry/GeometryKernelTypes.h
src/slicer_core/geometry/DistanceField2D.h
src/slicer_core/geometry/DistanceField2D.cpp
src/slicer_core/geometry/ShellMask.h
src/slicer_core/geometry/ShellMask.cpp
src/slicer_core/geometry/GeometryKernelReport.h
src/slicer_core/geometry/GeometryKernelReport.cpp
src/slicer_core/geometry/OpenVdbAdapter.h
src/slicer_core/geometry/OpenVdbAdapter.cpp
```

职责：

- `GeometryKernelTypes`：定义 `BinaryMask2D`、`DistanceField2D`、`ShellMaskResult`、`OpenVdbStatus`。
- `DistanceField2D`：从 binary mask 生成 signed distance field，第一版使用 O(N^2) 最近边界距离。
- `ShellMask`：从 signed distance field 生成 shell / interior / boundary mask。
- `GeometryKernelReport`：输出 `p0.geometry_kernel_report.1`。
- `OpenVdbAdapter`：`USE_OPENVDB=OFF` 时 stub；`USE_OPENVDB=ON` 时编译真实 OpenVDB smoke 逻辑。

---

## 4. Demo Target

新增：

```text
apps/geometry_kernel_demo/main.cpp
geometry_kernel_demo
```

CMake 选项：

```cmake
option(ENABLE_GEOMETRY_KERNEL_DEMO "Build experimental geometry kernel demo" ON)
option(USE_OPENVDB "Enable optional OpenVDB adapter" OFF)
```

支持 case：

```text
--case heightfield-sdf
--case surface-shell
--case openvdb-smoke
--case compensated-varnish
```

当前 `compensated-varnish` 为 graceful stub，仅写 report warning，不修改生产光油策略。

默认输出：

```text
output/GeometryKernel*/
  reports/geometry_kernel_report.json
  preview/*.png
```

---

## 5. USE_OPENVDB=OFF 构建与运行结果

已执行并通过：

```powershell
cmake --build build --config Debug
cmake --build build --config Debug --target geometry_kernel_demo
.\build\Debug\geometry_kernel_demo.exe --case heightfield-sdf --output output\GeometryKernelDemo
.\build\Debug\geometry_kernel_demo.exe --case surface-shell --shell-mm 0.05 --output output\GeometryKernelShell
.\build\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdbStub
.\scripts\run_geometry_kernel_tests.ps1
.\scripts\run_ci_quick.ps1
```

输出摘要：

```text
heightfield-sdf:
  shellPixels: 884
  interiorPixels: 508
  boundaryPixels: 440
  openvdbCompiled: false

surface-shell:
  shellPixels: 884
  interiorPixels: 508
  boundaryPixels: 440
  openvdbCompiled: false

openvdb-smoke:
  shellPixels: 884
  interiorPixels: 508
  boundaryPixels: 440
  openvdbCompiled: false
  graceful skip
```

Report 验证：

```text
geometry_kernel_report.schema = p0.geometry_kernel_report.1
preview PNG count >= 1
openvdb.enabled = false
openvdb.available = false
```

`run_ci_quick.ps1` 最终结果：

```text
CI quick complete.
```

---

## 6. USE_OPENVDB=ON 构建与运行结果

已执行：

```powershell
cmake -S . -B build-openvdb -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON
```

结果：

```text
Configure: FAILED
Build: not executed
openvdb-smoke: not executed
```

失败原因：

```text
Could not find a package configuration file provided by "OpenVDB":
OpenVDB.cps
openvdb.cps
OpenVDBConfig.cmake
openvdb-config.cmake
```

结论：

```text
本机没有可被 CMake 找到的 OpenVDB package config。
这是依赖环境缺失，不影响 USE_OPENVDB=OFF 默认构建和当前主线。
```

详细记录见：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

---

## 7. 测试脚本

新增：

```text
scripts/run_geometry_kernel_tests.ps1
```

覆盖：

- `heightfield-sdf`
- `surface-shell`
- `openvdb-smoke`
- report schema 校验
- preview PNG 存在性校验
- 默认 OFF 下 OpenVDB disabled 校验

---

## 8. 对 Production Pipeline 的影响

本阶段未修改：

```text
slicer_cli 默认执行路径
RGBWSV TIFF writer
manifest schema
rip_reader_test
SupportShapePipeline
SupportShapeOptimizer
MaterialPolicy
MaterialRoleMapping
MaterialProcessProfile
Qt Debug UI
```

协议保持：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF channel
```

---

## 9. 已知限制

1. `DistanceField2D` 是 O(N^2) correctness-first 实现，只适合小 fixture。
2. 当前 shell mask 是 2D mask prototype，不是完整 3D SDF shell。
3. `OpenVdbAdapter` 在默认 OFF 下是 stub。
4. 本机 ON 验证未通过，原因是未安装或未配置 OpenVDB package config。
5. 尚未建立 vcpkg manifest / Conan profile / source build 的正式依赖锁定。
6. 尚未接入 3D mesh → OpenVDB level set 的真实生产路径。

---

## 10. 是否进入 09A / 09B / 09C

建议可以进入 09A，但范围必须保持 prototype：

```text
09A：SDF surface shell texture prototype
```

进入 09A 前建议：

1. 继续保持 `USE_OPENVDB=OFF` 默认可构建。
2. 09A 只消费 geometry kernel prototype 的 shell mask，不接入生产 RGBWSV 输出。
3. 若要验证真实 OpenVDB，先用 vcpkg 建立 `build-openvdb` 环境。
4. 不在 09A 同时展开 compensated varnish 和 support clearance。

备选路线：

- `09B`：如果优先验证光油补偿，则进入 SDF compensated varnish prototype。
- `09C`：如果优先验证支撑避让/悬垂诊断，则进入 SDF support clearance / overhang diagnostics。

当前建议：

```text
先进入 09A，原因是 surface shell texture 是当前彩色/纹理链路最直接的几何短板。
```
