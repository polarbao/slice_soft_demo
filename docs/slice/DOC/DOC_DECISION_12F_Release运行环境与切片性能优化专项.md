# DOC_DECISION_12F Release 运行环境与切片性能优化专项

> 文档状态：Decision / R0 Implemented / R1-R5 Planned
> 日期：2026-07-16
> 上游证据：12B 性能报告、12C Qt fresh build lane、2026-07-16 UI 耗时截图

## 1. 决策结论

建立 `12F Release 运行环境与切片性能优化专项`，将以下问题纳入同一个可度量、可回退的专项：

```text
1. Debug 与 Release 使用场景不清；
2. Qt UI、slicer_cli、rip_reader_test 没有统一运行目录；
3. VS Code 存在两个功能重叠的 Qt Debug 环境；
4. UI 默认调用 Debug slicer_cli，导致性能观测失真；
5. 支撑生成、逐层材料合成、稠密 mask 和重复切片缓存仍有优化空间；
6. preview/I/O 与纯切片耗时需要继续分开治理。
```

本专项不重新打开 OpenVDB production replacement。OpenVDB 继续保持：

```text
optional / disabled-by-default SDF utility candidate
```

## 2. 两个历史 Qt Debug 环境的区别

| 环境 | 构建目录 | 配置方式 | 目的 | 主要问题 |
|---|---|---|---|---|
| `SliceSoft: Debug Qt UI` | `build` | 普通 `cmake -S . -B build` | 日常开发 | 依赖历史 cache，Qt 是否发现取决于已有环境 |
| `SliceSoft: Debug 12C Fresh Qt UI` | `build-12c-ui` | `Configure12CQtUi.ps1` 显式设置 Qt、UI ON、OpenVDB OFF | 12C fresh gate | 是阶段验收 lane，不应长期成为第二套日常入口 |

两者编译的是同一个 `slicer_debug_ui` target，没有产品功能差异。12C 环境增加了：

```text
显式 Qt5_DIR；
独立 fresh build directory；
USE_OPENVDB=OFF；
Qt 5.15.2 / MSVC 19.50+ compatibility shim 验证；
12C self-test/smoke 证据。
```

历史上两个 UI 即使从不同目录启动，`ToolPaths` 仍硬编码调用 `build/Debug/slicer_cli.exe`，因此它们并不是两套完整运行环境。

## 3. 整合决策

两个日常 Debug UI 入口整合为一个：

```text
SliceSoft: Debug Qt UI
```

统一构建根目录：

```text
build-slicesoft/Debug
build-slicesoft/Release
```

统一运行根目录：

```text
runtime/slicesoft/Debug
runtime/slicesoft/Release
```

12C 的 `Configure12CQtUi.ps1` 保留为历史 fresh gate 和回归证据，不再作为日常第二套 VS Code UI 入口。

## 4. 构建生成器决策

本机 Visual Studio multi-config generator 会通过 `HostX86/x64 cl.exe` 启动编译，实际验证中出现 compiler identification 和普通构建进程长时间不退出。

候选方案：

| 方案 | 结论 |
|---|---|
| 继续复用 Visual Studio generator | 保留给现有 `build`、12C 历史 lane 和 CTest；不作为新 Runtime 默认入口 |
| Ninja Multi-Config | 当前机器没有可用 Ninja，暂不引入新工具依赖 |
| NMake Makefiles + VS x64 Developer Environment | 本轮采用；使用 `Hostx64/x64 cl.exe`，fresh Release/Debug 构建均通过 |

新 Runtime 脚本主动导入 Visual Studio x64 环境，并使用单配置 NMake build directory，避免 Debug/Release cache 混用。

## 5. Runtime 发布决策

`PrepareSliceSoftRuntime.ps1` 负责：

```text
配置 NMake x64 build；
构建 slicer_cli、rip_reader_test、slicer_debug_ui；
调用 windeployqt；
部署 Qt Widgets、platform plugin 和 MSVC runtime；
部署场景索引、Profile 配置、模型/纹理资源和 Profile 引用文档；
拒绝包含外部绝对模型路径或绝对输出路径的 Profile；
通过 staging + rename 发布 runtime；
生成 slicesoft.runtime.1 manifest。
```

运行目录中的 UI 必须优先调用同目录：

```text
slicer_cli.exe
rip_reader_test.exe
```

禁止 Release UI 回退调用 Debug CLI。OpenVDB candidate 不进入默认 Runtime 包。

Release UI 将包含场景索引的 EXE 所在目录识别为应用资源根目录，不依赖启动快捷方式的工作目录。运行包可以整体复制，但不得拆散 `samples/`、`model/`、Qt plugins 和 EXE 的相对目录结构。

## 6. 性能专项边界

后续优化优先级固定为：

```text
P0 Release Runtime 与公平 benchmark；
P1 支撑生成区间化、重复扫描消除；
P1 layer compose 扫描融合和 buffer 复用；
P2 relief heightfield 避免完整 3D model mask 物化；
P2 几何/支撑中间结果缓存与增量切片；
P3 tile/layer 并行；
独立 I/O 线：preview 按需/异步生成。
```

## 7. 固定红线

```text
不修改 p0.rgbwsv.2；
不修改 RGBWSV channel order；
不修改 uint8 / black_is_print；
不默认启用 OpenVDB；
不替换 legacy production slicer；
Debug 数据不作为产品性能结论；
算法优化必须通过 Release core-only before/after 和语义一致性 gate。
```

## 8. 当前状态

```text
12F-R0 Runtime 环境：IMPLEMENTED；
Debug Runtime self-test：PASS；
Release Runtime self-test：PASS；
Release 算法 benchmark 刷新：NOT STARTED；
支撑/合成/稠密 mask 优化：NOT STARTED。
```
