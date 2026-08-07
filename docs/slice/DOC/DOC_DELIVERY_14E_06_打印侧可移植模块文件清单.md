# DOC_DELIVERY_14E-06 打印侧可移植模块文件清单

> 状态：**SLICER-SIDE READY / PRINT-SIDE ACK PENDING**
> 日期：2026-08-07
> 任务：14E-06
> 机器真源：`contracts/slicer_ui_host_portability_manifest.json`
> 上游合同：`print_module_spi.h`、`slicer_capability_dtos` v1.3、`slicer_three_lane_contract` v1.1、`slicer_ui_view_spec` v1.0

## 1. 使用范围

本清单用于打印软件评估如何复用 `apps/slicer_ui_host_sim/`。它不是要求打印侧复制一套完整窗口，而是把参考宿主拆成三类：

1. **可直接复制**：切片 ABI、交互、相机、渲染和设置语义已经冻结，可原样纳入打印软件工程；只允许调整命名空间、目录、信号接线和构建注册，不得改写合同语义。
2. **需改写**：参考程序的入口、窗口壳和 CMake 只演示集成方式，必须接入打印软件现有主窗口、作业系统和构建体系。
3. **验收专用**：可以直接复制到打印侧测试工程，但不应编入生产 UI。

打印侧不得复制 `src/slicer_core/`、`src/slicer_base/`、`src/slicer_engine/`、`src/slicer_module/` 或 `apps/slicer_worker/` 源码来绕过 ABI。生产集成只运行时装载 `slicer_module.dll`。

## 2. 可直接复制文件

### 2.1 ABI 与请求构造

| 文件 | 用途 | 打印侧动作 |
|---|---|---|
| `apps/slicer_ui_host_sim/ModuleClient.h` | 11 个 `pm_*` 导出、模块/作业 RAII 与调用计数 | 直接复制；模块路径由打印软件注入 |
| `apps/slicer_ui_host_sim/ModuleClient.cpp` | `LoadLibrary/GetProcAddress`、缓冲三态、错误读取 | 直接复制；禁止静态链接模块内部实现 |
| `apps/slicer_host_sim/HostRequestBuilder.h` | 公开能力请求构造接口 | 直接复制 |
| `apps/slicer_host_sim/HostRequestBuilder.c` | Scene/Model/Package/Slice 请求 JSON 构造 | 直接复制；CMake 当前按 C++ 编译 |
| `apps/slicer_host_sim/JsonText.h` | 最小 JSON 文本辅助 | 直接复制 |
| `apps/slicer_host_sim/JsonText.c` | 转义与文本拼接实现 | 直接复制 |

### 2.2 三车道交互

| 文件 | 用途 | 打印侧动作 |
|---|---|---|
| `apps/slicer_ui_host_sim/SceneInteractionController.h` | Transient/Commit/Stale 控制器接口 | 直接复制 |
| `apps/slicer_ui_host_sim/SceneInteractionController.cpp` | 本地瞬时变换、权威提交、Stale 回滚 | 直接复制；鼠标移动不得新增 DLL 调用 |
| `apps/slicer_ui_host_sim/TransformCommitPolicy.h` | operationId、revision 与提交请求策略 | 直接复制 |
| `apps/slicer_ui_host_sim/TransformCommitPolicy.cpp` | 三车道请求和响应判定 | 直接复制 |
| `apps/slicer_ui_host_sim/MoveOptimizationPolicy.h` | 拖拽本地预测与缓存策略 | 直接复制 |
| `apps/slicer_ui_host_sim/MoveOptimizationPolicy.cpp` | 局部移动、提交节流与回滚策略 | 直接复制 |

### 2.3 相机与视图切换

| 文件 | 用途 | 打印侧动作 |
|---|---|---|
| `apps/slicer_ui_host_sim/camera/CameraController.h` | orbit/pan/zoom/预设视角的后端中立接口 | 直接复制 |
| `apps/slicer_ui_host_sim/camera/CameraController.cpp` | 七向相机与投影状态 | 直接复制；相机操作保持 0 次 DLL 调用 |
| `apps/slicer_ui_host_sim/camera/ViewModeSwitch.h` | `top`/`three_d` 状态合同 | 直接复制 |
| `apps/slicer_ui_host_sim/camera/ViewModeSwitch.cpp` | 视图模式解析与切换 | 直接复制 |

### 2.4 渲染与外观缓存

| 文件 | 用途 | 打印侧动作 |
|---|---|---|
| `apps/slicer_ui_host_sim/render/IRenderBackend.h` | 后端中立场景、相机、网格与纹理接口 | 直接复制；可在接口后替换为打印侧 GPU 后端 |
| `apps/slicer_ui_host_sim/render/AppearanceCache.h` | appearance/texture identity 缓存 | 直接复制 |
| `apps/slicer_ui_host_sim/render/AppearanceCache.cpp` | 外观资源复用和失效规则 | 直接复制；实例移动不得使纹理缓存失效 |
| `apps/slicer_ui_host_sim/render/CpuRasterBackend.h` | 当前参考 CPU 后端 | 直接复制作为基准/回退后端 |
| `apps/slicer_ui_host_sim/render/CpuRasterBackend.cpp` | CPU 帧缓存与绘制调度 | 直接复制 |
| `apps/slicer_ui_host_sim/render/CpuRasterDecor.h` | 网格、平台、轮廓与辅助显示 | 直接复制 |
| `apps/slicer_ui_host_sim/render/CpuRasterDecor.cpp` | 1 mm/10 mm 网格和白纹理对比辅助 | 直接复制；不得修改纹理像素 |
| `apps/slicer_ui_host_sim/render/CpuRasterResources.h` | CPU 纹理/网格资源结构 | 直接复制 |
| `apps/slicer_ui_host_sim/render/CpuRasterizer.cpp` | 三角形、纹理与深度栅格化 | 直接复制作为参考实现 |
| `apps/slicer_ui_host_sim/render/SceneRenderPolicy.h` | `three_d` ViewData 解析、缓存与渲染策略 | 直接复制 |
| `apps/slicer_ui_host_sim/render/SceneRenderPolicy.cpp` | 三维场景请求和刷新 | 直接复制 |
| `apps/slicer_ui_host_sim/render/SceneRenderPolicyData.cpp` | 三维 DTO、相机和构建体积数据解析 | 直接复制 |
| `apps/slicer_ui_host_sim/render/SceneRenderPolicyMesh.cpp` | mesh/UV/submesh/material/texture 转换 | 直接复制；纹理失败必须 fail-closed |
| `apps/slicer_ui_host_sim/render/TopViewRenderPolicy.h` | `top` surfacePreview 与局部缓存策略 | 直接复制 |
| `apps/slicer_ui_host_sim/render/TopViewRenderPolicy.cpp` | 俯视纹理、轮廓、选择与布局绘制 | 直接复制 |
| `apps/slicer_ui_host_sim/render/TopViewRenderPolicyData.cpp` | top DTO、appearance 和实例解析 | 直接复制 |

> 当前后端为可交付参考实现，不是打印侧最终 GPU 技术选型。打印侧若替换为 OpenGL/Direct3D，只改写 `IRenderBackend` 的实现，不得改变 ViewData、缓存 identity、三车道或纹理失败语义。

### 2.5 Qt 可复用控件与设置

| 文件 | 用途 | 打印侧动作 |
|---|---|---|
| `apps/slicer_ui_host_sim/ViewWorkspaceWidget.h` | 中央 top/three_d 视图切换控件 | 直接复制或嵌入打印侧中央工作区 |
| `apps/slicer_ui_host_sim/ViewWorkspaceWidget.cpp` | 视图入口、堆叠页和本地切换 | 直接复制；切换保持 0 次 DLL 调用 |
| `apps/slicer_ui_host_sim/settings/ViewPresentationSettings.h` | 默认视图和投影设置 | 直接复制 |
| `apps/slicer_ui_host_sim/settings/ViewPresentationSettings.cpp` | session config 读写和回退 | 直接复制；可把存储位置接到打印侧设置服务 |
| `apps/slicer_ui_host_sim/Qt515MsvcCompatibility.h` | 新版 MSVC 编译 Qt 5.15 的条件兼容垫片 | 相同工具链时直接复制；不满足条件时不强制包含 |

### 2.6 验收专用，可直接复制到测试目标

| 文件 | 用途 | 生产 UI |
|---|---|---|
| `apps/slicer_ui_host_sim/CapabilityCoverageRequests.h` | 15 项能力请求 fixture | 不编入生产 UI |
| `apps/slicer_ui_host_sim/CapabilityCoverageRequests.cpp` | 请求、负例和资源定位 | 不编入生产 UI |
| `apps/slicer_ui_host_sim/CapabilityCoverageRunner.h` | 覆盖矩阵执行器 | 不编入生产 UI |
| `apps/slicer_ui_host_sim/CapabilityCoverageRunner.cpp` | 模块作业轮询、校验和证据采集 | 不编入生产 UI |
| `apps/slicer_ui_host_sim/CapabilityCoverageWorkflow.cpp` | P0/P1/P2 能力闭环工作流 | 不编入生产 UI |

## 3. 需改写文件

| 文件 | 原因 | 打印侧改写目标 |
|---|---|---|
| `apps/slicer_ui_host_sim/Main.cpp` | 参考程序命令行、自测和窗口启动入口 | 接入打印软件既有进程启动、模块发现与退出策略 |
| `apps/slicer_ui_host_sim/HostMainWindow.h` | 参考窗口不是打印软件信息架构 | 将 `ModuleClient`、视图工作区和状态绑定到现有主窗口/文档模型 |
| `apps/slicer_ui_host_sim/HostMainWindow.cpp` | 仅演示模块状态、双视图和设置页 | 重写为打印软件现有作业栏、实例树、Context Inspector 和任务区接线 |
| `apps/slicer_ui_host_sim/CMakeLists.txt` | 包含本仓库 target、测试和证据目录 | 改写为打印软件 target；保留 Qt、Windows、公开 ABI 和运行时部署要求 |

这四个文件不应整文件复制覆盖打印软件现有实现。可逐段参考，但最终所有权属于打印软件宿主层。

## 4. 必须同步的合同文件

| 文件 | 用途 |
|---|---|
| `contracts/print_module_spi.h` | 唯一编译期公开 C ABI；SPI v1、11 个导出 |
| `contracts/slicer_capability_dtos.json` / `.md` | 15 项能力请求/响应、ViewData v1.3 语义 |
| `contracts/slicer_three_lane_contract.json` / `.md` | Transient/Commit/Recovery 调用规则 |
| `contracts/slicer_cancel_contract.json` / `.md` | Cancelling/Cancelled 与超时清理 |
| `contracts/slicer_error_codes.json` | 稳定错误码和 UI 映射 |
| `contracts/slicer_module_info.schema.json` | `pm_module_info` 校验 |
| `contracts/slicer_module_manifest.schema.json` | `module.json` 部署校验 |
| `contracts/slicer_ui_view_spec.json` | top/three_d、网格、白纹理辅助与切换不变量 |

打印侧不得根据字段名自行猜测语义；JSON 与对应 Markdown 必须作为同一版本一起回签。

## 5. 运行时交付物

以下文件由切片侧 14F 打包，不从源码目录复制：

```text
modules/slicer/slicer_module.dll
modules/slicer/slicer_worker.exe
modules/slicer/module.json
modules/slicer/<runtime dependency DLLs>
```

打印软件负责模块发现、加载前 manifest 校验、进程级模块生命周期和 UI 作业映射；切片侧负责模块/Worker/依赖 DLL 的版本一致性和分发清单。

## 6. 推荐移植顺序

1. 先接入 `print_module_spi.h`、`ModuleClient` 和四个请求构造文件，完成模块加载、自述和缺模块负例。
2. 复制三车道交互策略，接入打印软件 Scene/Document；验证鼠标移动 0 次 DLL 调用、Commit 不追加 snapshot、Stale 才恢复。
3. 复制 `top` 渲染链路和 `ViewWorkspaceWidget`，验证真实纹理、白纹理可辨识和实例移动缓存不失效。
4. 复制 `three_d` 相机、外观缓存和渲染策略；打印侧可保留 CPU 后端或在 `IRenderBackend` 后替换 GPU 实现。
5. 把 `CapabilityCoverage*` 编入独立验收 target，跑完 15 项能力、取消、错误码和纹理 fail-closed。
6. 最后改写主窗口与构建接线，接收 14F 运行时包并执行打印侧书面回签。

## 7. 验收与未决项

本仓库可复用以下验证文件作为打印侧测试依据：

```text
tests/stage14e_01/ValidatePureCHost.py
tests/stage14e_02/ValidateMissingModule.py
tests/stage14e_02/ValidateQtHostBoundary.py
tests/stage14e_03/Stage14E03InteractionTests.cpp
tests/stage14e_04/Stage14E04TopViewTests.cpp
tests/stage14e_04/Stage14E04RenderBenchmark.cpp
tests/stage14e_04c/Stage14E04CThreeDTests.cpp
tests/stage14e_04d/Stage14E04DViewSwitchTests.cpp
```

切片侧清单已经就绪；打印侧尚未完成成本评估和书面确认，因此 `DEMO_14` 的 D14-E-07 继续保持 `NOT RUN`，不得提前写成 PASS。
