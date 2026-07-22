# DOC_PREP_12E-08C-R4-04 Qt Preflight UI 准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-22
> 原子任务：12E-08C-R4-04
> 前置任务：R4-01/02/03 COMPLETE

## 1. 任务目标

R4-04 将 R4-01..03 已完成的 ModelPreflight 合同、两阶段服务和模式准入 gate 接入现有 Qt 5.15 调试
工作台，使所有“启动切片”的 UI 路径在子进程启动前完成同一套预检，并以中文展示通过、警告、阻断、
过期和取消状态。

本任务不实现模型修复，不改变切片算法，不把 `global_surface_shell` 提升为生产模式，不修改 RGBWSV/TIFF/
manifest/RIP 协议。

## 2. 当前代码现实

现有切片入口：

```text
OnImportModelAndSlice -> GenerateEffectiveConfig -> RunGeneratedConfig -> QProcess；
runSlicer -> GenerateEffectiveConfig -> QProcess；
OnImportModelOpenVdbCandidate -> CreateOpenVdbCandidateConfig -> QProcess；
OnImportModelOpenVdbDiagnostic -> 纯实验诊断，不是切片生产入口。
```

当前三个切片入口都没有 UI 级异步 preflight；R4-03 已在 CLI/pipeline 内提供最终 fail-closed 兜底，因此
blocked 输入不会进入 writer，但用户只能在子进程失败后看到日志，无法在启动前获得可操作的中文诊断。

可复用能力：

```text
ConfigDocument::changed：配置变化通知；
ProcessRunner：切片子进程启动/停止与日志；
HelpTextProvider：中文说明真源；
UiSmokeTestRunner：无人工交互的 Qt 验证入口；
ToolPaths：普通与 OpenVDB ON slicer_cli 路径；
ModelPreflightService：同步、可缓存、backend-neutral；
EvaluateModelPreflightAdmissions：双模式准入；
ModelPreflightGate：pipeline 最终守门。
```

## 3. 冻结架构

### 3.1 模块边界

计划新增：

```text
apps/slicer_debug_ui/services/ModelPreflightController.h/.cpp；
apps/slicer_debug_ui/services/ModelPreflightPresenter.h/.cpp；
apps/slicer_debug_ui/services/SlicePreflightCoordinator.h/.cpp；
apps/slicer_debug_ui/widgets/ModelPreflightPanel.h/.cpp。
```

职责：

| 模块 | 职责 | 禁止事项 |
|---|---|---|
| `ModelPreflightController` | 异步执行、generation、取消、cache 复用、backend capability 合并 | 不启动切片、不决定中文布局 |
| `ModelPreflightPresenter` | stable code 到中文摘要/建议的只读映射 | 不修改 severity/admission |
| `SlicePreflightCoordinator` | 保存一个待执行切片动作，只在当前 generation admitted 后放行 | 不做几何诊断、不自动切换 mode |
| `ModelPreflightPanel` | 状态、模式、问题列表、重新检测命令 | 不拼装业务规则、不提供忽略 global blocker |
| `MainWindow` | 生成 effective config、提交待执行动作、收到 admitted 后启动现有 QProcess | 不直接判断 `MESH_*` code |

Qt 继续只存在于 `apps/slicer_debug_ui`。`slicer_core` 的 ModelPreflight DTO/service/policy/gate 不依赖 QObject、
QString、QWidget。

### 3.2 异步执行与生命周期

R4-04 使用 `QThreadPool/QRunnable` 执行同步 `ModelPreflightService::Run`，不新增 Qt Concurrent 依赖：

```text
UI 线程创建 request/generation/cancel token；
线程池任务持有 shared_ptr<ModelPreflightService> 和 shared cancellation token；
完成后通过 QMetaObject::invokeMethod + QPointer 回到 UI 线程；
Controller 已销毁或 generation 不是当前值时丢弃回调；
同一时刻只允许一个逻辑 active request，变化期间记录 latest pending request；
旧任务到阶段边界读取 cancel token，结束后立即启动最新 pending request；
关闭窗口只设置取消、终止 capability probe 并使 QPointer 失效，不等待几何任务阻塞 UI。
```

不得把 `ModelPreflightExecutionResult` 作为未注册 Qt queued signal 参数跨线程发送。worker 回调先回到 controller
所属线程，再由 controller 发出基于函数指针连接的 `Sig*` 信号。

所有新增槽以 `On` 开头，新增信号以 `Sig` 开头；Public 接口使用 Doxygen，C++ 使用 Allman 风格。

### 3.3 Global backend capability

默认 UI runtime 与 OpenVDB ON candidate executable 是两个构建产物，不能用 UI 进程自身
`GetOpenVdbStatus()` 代表外部 candidate 的真实能力。

R4-04 冻结一个只读 CLI 探针：

```text
slicer_cli --openvdb-capability-json
schema=slicesoft.openvdb_capability.12e_r4.1
字段：compiledWithOpenVdb/runtimeAvailable/version/reason
不要求 config，不加载模型，不创建 package/report 文件。
```

global UI 预检先验证 `ToolPaths::openvdb_slicer_cli`，再用独立异步 `QProcess` 调用上述探针。只有
`runtimeAvailable=true` 才向 admission context 注入 `global_backend_available=true`。文件存在但 DLL/初始化失败
仍显示 `E_12E_PREFLIGHT_BACKEND_UNAVAILABLE`。

探针只提供 UI 提前反馈；真正启动 candidate 时，R4-03 pipeline gate 仍必须重新探测并作为最终安全边界。

## 4. UI 状态机

冻结状态：

| UI 状态 | 中文显示 | 切片动作 |
|---|---|---|
| `not_run` | 待检测 | 禁止直接启动，点击切片先检测 |
| `running` | 检测中 | 禁用切片与重复检测，允许取消 |
| `passed` | 检测通过 | 放行当前显式 mode |
| `warning` | 检测有警告 | legacy 需明确确认后放行；global 按 admission 决定 |
| `blocked` | 检测阻断 | 不启动进程，不 fallback |
| `stale` | 检测结果已过期 | 点击切片必须重跑 |
| `cancelled` | 检测已取消 | 不启动进程，可重新检测 |

共享结果必须同时展示 legacy/global admission；主动作只读取当前请求 mode。切换 mode 不重做共享几何算法，
但必须对 fresh 结果立即重新计算 admission。若切换到 global，还必须具有 fresh capability probe。

## 5. 一键动作闭环

所有切片 UI 路径统一为：

```text
生成并校验 effective config
-> SlicePreflightCoordinator::Request
-> ModelPreflightController 异步 EnsureFresh
-> blocked/stale/cancelled：展示结果，process start count=0
-> warning：仅 legacy 显示“继续传统切片/取消”确认
-> passed/confirmed warning：发出 SigActionAdmitted
-> MainWindow 调用现有 RunGeneratedConfig 或 RunOpenVdbCandidate
-> CLI/pipeline 再执行 R4-03 最终 gate
```

必须接入：

```text
“导入模型并切片”；
“运行切片”；
“导入模型并 OpenVDB 候选切片”。
```

“导入模型并 OpenVDB 诊断”不是生产/候选切片动作：它可以在 topology blocked 时继续生成只读诊断，但必须
先显示共享 preflight 事实，且不得被描述为已通过 global 切片准入。

任何 global blocker 都不能弹出“继续”选项，也不能改写为 legacy。legacy warning 对话框必须显示有效模式
“传统切片”及稳定 warning code。

## 6. Stale 与缓存规则

UI 侧触发 stale：

```text
ConfigDocument::changed；
模型/Profile/场景切换；
modelPath、transform/scale/autoOrient、missingTexturePolicy、preflight options 变化；
所选 mode 变化时，仅 admission 视图 stale/re-evaluate；
外部模型/MTL/贴图变化时，文件 watcher 尽快标记 stale。
```

文件 watcher 是体验优化，不是信任边界。每次切片动作都重新调用 service；service 会重新计算 source/resource/
transform/options identity，未变化时命中 cache，变化时执行完整检查。即使 watcher 漏报，也不能沿用旧结果放行。

generated config 的 output/package/preview-only 变化可以保守标记 stale，但不得更改 cache key 合同来掩盖 UI
状态管理问题。

## 7. 中文展示与布局

`ModelPreflightPresenter` 至少覆盖 R4-01 稳定 `E_12E_PREFLIGHT_*` 和 R4-03 八类 `MESH_*` code。未知 error
显示“检测到未识别问题，已按安全策略阻断”，并保留原 code，不能静默忽略。

UI 布局：

```text
左侧运行区：一行状态图标/中文状态、当前有效模式、重新检测/取消图标按钮；
右侧新增“模型预检”页：级别、问题、数量、建议四列；
长建议列 stretch，单元格自动换行，最小宽度稳定；
阻断/警告/信息使用图标与文字，不只依赖颜色；
不新增嵌套卡片，不把问题列表塞入日志文本；
状态栏显示 cache hit、generation 和 fast/full 完成状态，供调试追溯。
```

支持 1366x768、1440x900、1920x1080；最长中文不得覆盖按钮、表头或相邻区域。面板允许滚动，不强行拉伸
主预览区。

## 8. 计划修改文件

```text
apps/slicer_cli/main.cpp；
apps/slicer_debug_ui/CMakeLists.txt；
apps/slicer_debug_ui/MainWindow.h/.cpp；
apps/slicer_debug_ui/services/ModelPreflightController.h/.cpp；
apps/slicer_debug_ui/services/ModelPreflightPresenter.h/.cpp；
apps/slicer_debug_ui/services/SlicePreflightCoordinator.h/.cpp；
apps/slicer_debug_ui/services/HelpTextProvider.cpp；
apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp；
apps/slicer_debug_ui/widgets/ModelPreflightPanel.h/.cpp；
tests/unit/model_preflight_pipeline_gate/main.cpp；
CMakeLists.txt（仅 capability CLI/unit 注册确有需要时）。
```

原则上不修改 `src/slicer_core/preflight` 与 production writer。若实现中发现必须改变 R4-01..03 合同或生产协议，
立即停止并请求确认。

## 9. 验证矩阵

### 9.1 Controller/Presenter

```text
clean config：running -> passed；
self-intersection：legacy warning/global blocked；
unknown error：中文 fallback + blocked；
mode switch：共享诊断 cache hit，admission 立即变化；
config/resource change：stale；
连续三次请求：只有最新 generation 可更新 UI/放行动作；
取消/关闭窗口：无崩溃、无 use-after-free、action count=0；
OpenVDB executable missing/runtime unavailable：global blocked。
```

### 9.2 一键入口

```text
blocked “导入模型并切片”：ProcessRunner started=0；
blocked “运行切片”：ProcessRunner started=0；
blocked global candidate：candidate started=0、legacy started=0；
legacy warning 确认：legacy started=1、effective mode=legacy；
legacy warning 取消：started=0；
clean global + available probe：candidate started=1、legacy started=0；
诊断按钮：可输出 diagnostic，但 UI 不显示 global admitted。
```

UI Smoke 不弹真实文件对话框、不依赖用户点击，并通过 coordinator/fake action counter 验证调用次数。

### 9.3 布局与文案

```text
最长中文 blocker/recommendation 不遮挡；
状态图标同时有文字；
问题列表支持滚动；
1366x768 与 1440x900 主预览区仍可见；
关闭、取消、重试、模式切换 smoke PASS。
```

## 10. 验证命令

R4-04 实施时至少运行：

```powershell
cmake --build build --config Debug --target slicer_debug_ui slicer_cli model_preflight_pipeline_gate_unit_tests
ctest --test-dir build -C Debug -R "model_preflight_(contract|service|admission|pipeline_gate)" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-preflight-states
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-preflight-one-click-gate
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-preflight-lifecycle
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case workspace-layout-sizes
.\scripts\run_ci_quick.ps1
git diff --check
git status --short
```

若 `build-openvdb-09p` 存在，再验证真实 capability probe；默认 Debug lane 的 UI Smoke 使用 fake probe，不把
OpenVDB 变为普通构建必需依赖。Quick CI 的既有 `material_process_top2 widthPx expected=48 actual=226`
baseline 必须继续如实记录，不能在 R4-04 改 golden。

## 11. 停止条件

```text
需要修改 RGBWSV/TIFF/manifest/RIP：停止；
需要 global 自动 fallback legacy：停止；
需要让 UI 忽略 global blocker：停止；
需要默认开启 OpenVDB 或令普通构建依赖 OpenVDB：停止；
需要在 UI 线程同步执行完整预检：停止；
无法证明 blocked 时切片 QProcess started=0：R4-04 不得 COMPLETE；
关闭窗口仍可能销毁运行中的 QThread/回调目标：R4-04 不得 COMPLETE。
```

## 12. 后续任务准备判断

| 任务 | 准备状态 | 结论 |
|---|---|---|
| R4-04 | READY FOR DEVELOPMENT | 本文已冻结线程、状态机、三条切片入口、capability、UI 与 Smoke |
| R4-05 | PREPARED / WAIT R4-04 | PRD/DEV/DEMO、clean OBJ/3MF、三点 width 和材料矩阵已冻结，无需新增模型 |
| R4-06 | CONTRACT READY / EXTERNAL INPUT BLOCKED | intake 字段与 Gate 已定义，但三个 required 修复资产尚未到位 |
| R4-07 | DEPENDENCY PREPARED / WAIT R4-06 | 四 case Release/global/legacy 矩阵已定义，不能提前执行 |
| R4-08 | DEPENDENCY PREPARED / WAIT R4-07 | 只刷新 GO/NO-GO，不实现 08D adapter |

因此下一次可明确启动 R4-04 代码开发；R4-05 的基础准备已完成，R4-06 以后不是文档缺失，而是外部修复
资产依赖未满足。
