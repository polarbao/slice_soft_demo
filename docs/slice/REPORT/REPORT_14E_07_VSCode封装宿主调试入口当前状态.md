# REPORT_14E-07 VSCode 封装宿主调试入口当前状态

> 状态：COMPLETE
> 日期：2026-08-07
> 范围：只新增 Stage 14E 参考宿主的 VSCode 编译、运行、自检与调试入口，不替代原 `slicer_debug_ui`

## 1. 入口边界

VSCode 中保留两组相互独立的入口：

| 入口前缀 | 程序 | 用途 |
|---|---|---|
| `SliceSoft:` | `runtime/slicesoft/<Config>/slicer_debug_ui.exe` | 原切片调试 UI、CLI、RIP Reader 与 Runtime 部署 |
| `SliceSoft 14E:` | `build-slicesoft/main/apps/slicer_ui_host_sim/<Config>/slicer_ui_host_sim.exe` | 仅通过公开 SPI 装载 `slicer_module.dll` 的封装参考宿主 |

新增入口显式传入同配置的 `slicer_module.dll`，并通过 `QTDIR/bin` 提供 Qt 运行时。Debug 与 Release 不允许交叉装载。

## 2. 新增 VSCode 入口

### 2.1 运行与调试

- `SliceSoft 14E: Debug Packaged Host UI (Debug)`
- `SliceSoft 14E: Run Packaged Host UI (Debug)`
- `SliceSoft 14E: Run Packaged Host UI (Release)`

### 2.2 任务

- `SliceSoft 14E: Build Packaged Host UI (Debug/Release)`
- `SliceSoft 14E: Run Packaged Host UI (Debug/Release)`
- `SliceSoft 14E: Self-Test Packaged Host UI (Debug/Release)`

构建任务只指定 `slicer_ui_host_sim` 目标；该目标通过 CMake 依赖自动构建 `slicer_module` 和 `slicer_worker`，不会触发原 Runtime 部署。

## 3. 原入口复核

| 检查项 | 结果 | 证据 |
|---|---|---|
| 主构建配置 | PASS | `cmake --preset slicesoft-main` |
| 原 Debug 构建 | PASS | `cmake --build --preset slicesoft-debug` |
| 原 Release 构建 | PASS | `cmake --build --preset slicesoft-release` |
| Debug Runtime 部署 | PASS | `PrepareSliceSoftRuntime.ps1 -DeployOnly` |
| Release Runtime 部署 | PASS_WITH_WARNING | 部署完成；目录原子替换被系统拒绝后按既有逻辑执行原位部署 |
| 原 Debug/Release UI 启动 | PASS | 两个程序启动后持续存活 5 秒，随后由验证脚本主动结束 |

原 `slicer_debug_ui` 的编译、部署和正常启动链路可用；新增入口未修改原 target、Runtime 目录、参数或 Profile 逻辑。

补充观察：原 `SliceSoft: Test Runtime` 对应的 `--self-test` 在本轮离屏复核中未在 180 秒内退出。该现象不影响正常 UI 启动，也不是 14E 参考宿主入口引入；后续应作为原 UI smoke 生命周期问题独立排查，本卡不把该项写成 PASS。

## 4. 封装宿主验证

| 检查项 | Debug | Release |
|---|:---:|:---:|
| `slicer_ui_host_sim` 构建 | PASS | PASS |
| `slicer_module.dll` 装载与 SPI 自检 | PASS | PASS |
| 自检摘要 | `STAGE14E02_SELF_TEST_PASS spi=1 calls=6` | `STAGE14E02_SELF_TEST_PASS spi=1 calls=6` |
| 正常宿主启动 | PASS | PASS |

## 5. 阶段边界

- 14E-07 只解决开发入口可发现性，不等于 14F 安装包。
- 14F-01 仍需生成独立 `modules/slicer/` 分发目录，并验证依赖闭合、隔离装载及发布清单。
- 14F-02 及后续仍依赖打印侧和目标 RIP 外部联调证据。
