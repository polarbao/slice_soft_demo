# LOGDUMP 日志与崩溃转储模块设计及实施准备

> 日期：2026-09-14；版本：v0.3（状态索引修订）；状态：PREPARATION_SNAPSHOT / SUPERSEDED_BY_IMPLEMENTATION。
> 本文保留 v0.2 的准备阶段正文供追溯；正文中的“当前”“本轮”“未实现”“待准入”均指准备快照，不能作为当前功能或授权状态。此前仅准备的授权范围已由 [A00 开发准入与日志扩展定案](DOC_DECISION_LOGDUMP_A00_开发准入与日志扩展定案.md) 承接为本地实现与验证授权。
> 当前接口依据：[slicer_logging.h](../../../contracts/slicer_logging.h)；实现与实际验证：[本地实现与验证状态](../REPORT/REPORT_LOGDUMP_本地实现与验证状态.md)；唯一任务状态源：[专项任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)。历史建议与这些依据不一致时，以当前依据为准。
> 双层职责的历史补充见 [DLL 日志回调与双层复用接口草案](DOC_DESIGN_LOGDUMP_DLL日志回调与双层复用接口草案.md)。真实 PrintApp 集成、干净机器与物理打印不因本地实现完成而自动通过。

## 0. 当前实现与准备建议差异索引

以下按 2026-09-14 工作树源码核对；正文第 1 节起保留原准备快照。本索引不替代报告中的逐项验证结果，也不宣称所有 Stage 14 门禁通过。

| 准备快照位置 / 建议 | 当前实现与适用边界 | 当前依据 |
|---|---|---|
| §1、§3、§9：尚缺日志服务、可选 C 导出待准入、仅文档准备 | 已实现 DLL 实例日志、宿主日志、Worker 独立诊断 IPC 与 EXE crash helper；保留原 11 个 pm_* 签名并新增 3 个可选日志导出。当前完成度和未完成项只读任务表与报告 | [A00](DOC_DECISION_LOGDUMP_A00_开发准入与日志扩展定案.md)、[扩展头](../../../contracts/slicer_logging.h)、[实现报告](../REPORT/REPORT_LOGDUMP_本地实现与验证状态.md) |
| §5：独立 slicesoft_diagnostics_spdlog target | 文件后端源码仍位于 diagnostics/spdlog，但编入 slicesoft_diagnostics_host；另外有 diagnostics_transport、diagnostics_windows、diagnostics_process。DLL 不链接 spdlog/Qt；注入宿主 sink 时不创建默认文件后端 | [实际 target](../../../cmake/SliceSoftDiagnostics.cmake)、[LogSession](../../../src/diagnostics/host/LogSession.cpp) |
| §5：timestampUtc、threadRole、elapsedMs 等建议字段 | 正式事件使用整数 timestampUtcMs、sourcePid/sourceTid/sourceProcessRole；未独立定义 threadRole、ok、elapsedMs、pathShort。JSON 上限 16 KiB，message 上限 2048 字节、其他文本字段 128 字节，编码截断标记为 truncated；模块损失摘要为可选 diagnosticLoss | [事件编码](../../../src/diagnostics/LogEvent.cpp) |
| §6、§9：正常退出 flush 有等待上限、关键失败有应急记录 | 队列有界且溢出计数，但没有保证关键日志必达的独立应急文件路径。LogSession::Close 排空并 join；ModuleLogBinding 对所有非成功 clear 结果保留上下文并重试。宿主回调或磁盘永久阻塞时不承诺有界安全关闭，不能强杀日志线程或超时即卸载 DLL | [LogSession](../../../src/diagnostics/host/LogSession.cpp)、[ModuleLogBinding](../../../src/diagnostics/host/ModuleLogBinding.cpp)、[A00](DOC_DECISION_LOGDUMP_A00_开发准入与日志扩展定案.md) |
| §6：会话命名、轮转与全局预算建议 | 会话采用带 owner 标记及独占 lease 的 ssdiag_* 目录；每类日志 10 MiB 主文件加 3 个归档。健康启动按已知且不活跃的完整会话回收，默认日志 256 MiB、转储 512 MiB/5 份；活跃会话等原因可令预算无法满足，不作硬上限承诺 | [会话保留](../../../src/diagnostics/host/SessionRetention.cpp)、[启动接线](../../../src/diagnostics/host/ProcessDiagnostics.cpp) |
| §7、§9：转储覆盖与宿主复用测试计划 | 转储实测范围限专用子进程未处理访问异常（AV）；不保证 abort/terminate、栈溢出、OOM、fail-fast、强制结束或外部 rip_cli。已有过滤器默认不覆盖，自有 EXE 显式选择替换；真实 PrintApp 与干净机器仍需独立验证 | [CrashReporter](../../../src/diagnostics/windows/CrashReporter.cpp)、[验证范围](../REPORT/REPORT_LOGDUMP_本地实现与验证状态.md) |
| 接口草案 §5：未知等级警告并保留原值 | 无效等级、schema 或尺寸在日志路径拒绝；部分导出或未知扩展版本只禁用可选日志，不阻止旧 SPI 业务装载。未实现“未知等级降级后仍转发”的建议 | [事件校验](../../../src/diagnostics/LogEvent.cpp)、[宿主绑定](../../../src/diagnostics/host/ModuleLogBinding.cpp) |
| §6：路径脱敏与诊断导出建议 | 当前记录本地路径和 RIP 启动参数，未实现通用脱敏或一键导出流程；不得将建议当作已生效的隐私过滤能力。RIP stdout/stderr 以不超过 2048 字节的 UTF-8 片段写日志，原业务 capture 的 1 MiB 上限保持 | [RIP 日志出口](../../../apps/slicer_ui_host_sim/HostRipDiagnostics.h) |

## 1. 目标、基线与证据

问题类型是跨进程可观测性基础设施缺口，不是切片算法或 RIP 墨滴规则修复。

- 原工作目录：`E:/__Code/__Work/slice_test_demo/slice_soft_demo`，实际分支 `codex/host-visibility-navigation`。
- 新分支：`codex/feature-logging-dump`，基于原分支已提交的 `e2546797`。
- 独立工作树：`E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump`。
- 原工作树存在 UI、RIP 进度、手册、构建配置和模型等未提交内容。本专项没有复制、暂存、撤销或提交这些内容；新分支不包含它们。
- A 级依据：上述提交的切片代码、[SPI 头文件](../../../contracts/print_module_spi.h)，以及下表所列打印项目本地源码的 2026-09-14 读取快照。打印项目快照未绑定发布版本，不能视为干净机器运行证据。
- B 级依据：[Stage 14 集成边界](DOC_DECISION_14_切片能力包封装与打印软件集成专项.md)、[项目架构边界](../../../.agents/docs/architecture-boundary.md)。旧设计中的切片后端状态不替代当前代码。
- C 级背景：此前 RIP 退出、中文路径与依赖加载问题。历史截图不是本专项已经复现的测试结果。

### 当前、目标、历史和待确认状态

| 分类 | 结论 |
|---|---|
| 当前实现 | 已有错误码、stderr、Worker 结果、RIP 子进程输出采集和报告；缺少统一的持久化轮转日志与进程级 DUMP 生命周期，不能笼统说完全没有日志 |
| 目标实现 | DLL 按实例产生内部事件并通过版本化 C 回调输出；软件记录自身业务并接收模块日志，独占落盘/等级/目录管理；复用同一 adapter 接入打印宿主 |
| 历史参考 | 打印软件已封装 PrintAppLogging，但 DUMP 仍写在 main.cpp 内；不是可以直接搬入 DLL 的独立 DUMP 库 |
| 待确认 | 双层日志及 SDK 回调方向已由用户明确；开发准入需固化可选导出的受控增量、依赖和转储配置；真实打印项目接入与干净机器验收另行授权 |

## 2. 打印软件参考调查

参考根目录：`E:/__Code/__Work/ry_print_demo/PrintSolution`。只读分析，未执行打印程序或操作硬件。

| 已读源码 | 已核实行为 | 可复用点 / 不应照搬的部分 |
|---|---|---|
| `PrintApp/src/utils/SpdlogMgr.h/.cpp` | 单例、call_once、异步队列 8192/2 线程、轮转文件、模块 logger、控制台/VS sink、profile 分级 | 保留模块/等级/轮转思想；不能让切片 DLL 操作全局注册表、默认 logger 或全局 shutdown |
| `PrintApp/src/utils/SpdlogQtDataFormat.h` | 可选 Qt 类型格式支持 | Qt 适配留在 apps 层，不能进入 slicer_core 或公共日志接口 |
| `PrintApp/cmake/SharedBusinessTargets.cmake` | PrintAppLogging 静态 target，私有链接 spdlog::spdlog，关闭 AUTOMOC | 借鉴 target 化复用，不能假定静态链接就自动消除了 spdlog 全局状态冲突 |
| 根 `CMakeLists.txt`、`vcpkg.json` | find_package(spdlog CONFIG REQUIRED)，manifest 声明 spdlog | 用同一 vcpkg baseline 解析依赖；不从打印程序复制未知 ABI 的 DLL |
| `PrintApp/src/main.cpp` 的 SetupLogging | 启动建立目录、版本日志、10 MiB/3 文件配置、环境 profile | 学习启动环境与版本记录；检查初始化返回值，并提供可见的降级状态 |
| 同文件 InstallCrashHandler/UnhandledExceptionHandler | QApplication 前安装 SetUnhandledExceptionFilter；崩溃进程内调用 MiniDumpWriteDump；相对 logs/dumps；A 版路径 API | 学习早期安装与转储用途；重新设计路径、唯一命名、返回值诊断、进程归属和崩溃上下文可靠性 |
| `PrintApp/CMakeLists.txt` | Windows 链接 dbghelp | 仅 Windows 组件链接，不传播到纯 C++ 接口目标 |
| `A3DSDK/include/A3DDefs.h`、`A3DSDK.h` | LogCallback 为等级+字符串的 std::function；SetLogCallback 按实例注册，声明工作线程通知/空回调取消/异常隔离 | 复用职责和等级，改为符合切片边界的 C 函数指针与不透明 context；注销保证自行测试 |
| `PrintApp/src/business/printer/VendorEngineAdapter.cpp` | SDK 回调转入设备专属 m_sdkLogger，按等级映射，附带模块/阶段/线程信息 | 模块产生日志、宿主管理后端；切片无需绑定打印设备或共享 logger 对象 |
| `A3DSDK/CMakeLists.txt` | 当前只导入预编译 PrintSDK，不编译 src 下旧源码 | 不能将旧 PrinterEngineAdapter.cpp/Stub 或 GlobalLogger 声明当作现用 SDK 内部实现证据 |

参考实现的具体风险：

1. SpdlogMgr 使用全局 thread_pool、set_default_logger、apply_all 与 spdlog::shutdown；嵌入已有打印宿主时可能影响宿主日志。初始化失败被 call_once 消费，后续不能仅靠再次 Init 重试。
2. 参考队列溢出采用 block；直接放进高频切片或 UI 回调会引入阻塞。正常退出 flush 不能证明崩溃最后一条日志一定落盘。
3. DUMP 文件名只有秒级时间，CREATE_ALWAYS 可能覆盖同秒故障；相对路径和 CreateFileA 在工作目录/代码页变化时存在风险。
4. 转储调用未检查 MiniDumpWriteDump 成败；不能用“存在 .dmp 文件”代替有效异常流与可解析栈的证据。
5. 进程内异常过滤器不能捕获其他进程的异常；参考实现也不能据此保证覆盖 fail-fast、强制结束、OOM 或栈溢出。

Microsoft 建议尽可能由独立进程执行转储，原因包括崩溃上下文中的加载器死锁；DbgHelp 调用还需要串行化。本专项因此推荐预启动辅助进程，而非直接复制 main.cpp 中的异常处理器。[MiniDumpWriteDump 官方说明](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump)

## 3. 切片软件接入边界

| 当前文件 / 组件 | 拟接入职责 | 不改变的行为 |
|---|---|---|
| `apps/slicer_ui_host_sim` | EXE 级日志会话、Qt 桥接、显式安装本进程 crash reporter、诊断目录入口 | UI 不重新计算切片；错误状态保持真实 |
| `apps/slicer_worker/WorkerApplication.cpp` | 启动身份、请求阶段、耗时/退出日志；Worker 自己的转储注册 | --help / --contract-info 和业务 stdout 不被普通日志污染 |
| `apps/slicer_cli` | CLI 会话与错误持久化 | 已有机器可读输出和退出码不变 |
| `src/slicer_module/WorkerJobService.cpp`、能力 adapters | DLL 内部产生路由、校验、启动/退出与错误事件，通过本实例日志出口回调宿主 | 原 pm_* 签名和 Worker file_contract_v1 不加字段、不改错误传播；新增日志导出单独评审 |
| `apps/slicer_ui_host_sim/HostRipJobController.cpp` | 子进程启动/参数/退出/超时/取消，以及 stdout/stderr 有界持久化 | 不更换 RIP DLL，不绕过输入/输出验证，不改 S2 与发布结果 |
| `src/slicer_module/ModuleInitialization.cpp`、`DllMain.cpp` | 模块实例在显式回调注册后拥有日志调度资源，创建失败仍由既有错误接口报告 | 不在 DllMain 或信息查询时开文件、建线程、装过滤器；未注册时不落盘 |
| `src/slicer_module/logging`（拟新增） | C 回调扩展、实例隔离、有界投递、注销等待与 Worker 诊断接收 | 不链接 spdlog/Qt，不把 context 指针传入子进程 |

用户本轮明确要求 DLL 内部日志，因此首版必须包含可注册的模块事件出口，不能仅在宿主转写错误。建议新增独立版本的可选 C 日志扩展，保留原 11 个 pm_* 导出签名；C++ 对象、STL、Qt、spdlog logger 仍不跨 DLL 边界。新增导出是受控 ABI 增量，需配套扩展头文件、白名单与新旧宿主/DLL 兼容测试，不宣称 ABI 完全未变。

职责拆为 `slicer.module`（DLL/Worker 的内部事件）和 `slicer.app`（软件业务及持久化服务）。PrintApp 后续只需替换宿主 sink adapter，使用既有 SpdlogMgr；模块无需改变。`pm_spi_version`、`pm_module_info`、`pm_self_test` 不因查询而创建文件或启动日志服务，DUMP 仍由 EXE 管理。

原工作树中 RIP 进度和 UI 正在演进，LD-A03 开工前必须重新读取已合入版本，不能从旧基线覆盖这些实现。

## 4. 依赖方案比较

以下是选型准备，不是安装记录；具体端口版本、传递依赖和许可证副本在 LD-A01 固化，不更新整个 vcpkg baseline。

| 候选 | CMake / vcpkg | 许可证与维护风险 | 建议 |
|---|---|---|---|
| spdlog | find_package(spdlog CONFIG REQUIRED)，私有链接 spdlog::spdlog；manifest 添加 spdlog | MIT；核对 fmt 组合、编译宏、/MD 与 /MDd、Windows 文件名支持和传递 DLL；全局状态必须隔离 | 推荐，打印项目已有使用经验；不暴露后端类型 |
| Boost.Log | find_package(Boost COMPONENTS log)，Boost::log；vcpkg boost-log | Boost Software License 1.0；较大传递依赖和配置面，与当前打印封装不同 | 可行但不优先，避免为日志扩张依赖栈 |
| Windows DbgHelp + 自有 helper | Windows SDK dbghelp，非 Windows 保留明确 unsupported 状态；无需新增第三方 dump 库 | Windows SDK 使用/分发约束；IPC、异常上下文和 helper 生命周期由本项目维护 | 推荐首个 Windows 实现；不复制系统 DLL 进包 |
| Crashpad | 独立 handler；CMake/vcpkg 桥接及具体版本须另行验证 | 需核对所选版本 LICENSE 与传递依赖；部署/进程/升级维护成本更高 | 需要跨平台或更广异常覆盖时再评估，本轮不引入 |

依据：[spdlog 官方仓库](https://github.com/gabime/spdlog)、[Boost.Log 官方文档](https://www.boost.org/doc/libs/latest/libs/log/doc/html/index.html)、[Crashpad 官方介绍](https://chromium.googlesource.com/crashpad/crashpad/+/HEAD/README.md)。

切片与打印项目读取到的 manifest baseline 均为 `d13fa75214c258099923cf25a5e6311e58c07f3b`；这只是解析基础相同，不证明实际安装依赖和编译选项相同。

## 5. 拟建模块与所有权

以下目录和 target 是开发规划，本轮不建立空代码骨架。

```text
contracts/slicer_logging.h      版本化可选 C 回调扩展（拟新增）
src/diagnostics/                 纯 C++ 事件、会话配置、sink 接口
src/diagnostics/host/            宿主回调绑定、解码与等级/字段适配
src/slicer_module/logging/       DLL 实例日志出口、注销与 Worker 诊断接收
src/diagnostics/spdlog/          私有日志后端、轮转与队列生命周期
src/diagnostics/windows/         进程注册、受控 IPC 与转储元信息
apps/slicer_crash_reporter/      Windows 预启动转储辅助 EXE
apps/slicer_ui_host_sim/         Qt 桥接、诊断状态/目录入口
tests/diagnostics/              日志、宿主共存、故障子进程与符号测试
cmake/SliceSoftDiagnostics.cmake 目标和平台条件，根 CMake 只接入口
```

拟分 `slicesoft_diagnostics`、`slicesoft_diagnostics_host`、`slicesoft_diagnostics_spdlog`、`slicesoft_diagnostics_windows`；通用组件可独立构建，不依赖 slicer_core、TIFF、Qt、RIP 或 PrintApp。DLL 只链接轻量事件/调度部分及自己的出口，文件后端仅属宿主。UI bridge 只在应用 target。

软件日志会话显式拥有 logger、sink 和异步队列；初始化返回结果，失败可以显式重建。关闭只释放自己资源，不调用全局 shutdown、flush_every、apply_all 或 set_default_logger。被注入的宿主 sink 不由组件销毁。DLL 按实例管理回调队列，宿主须注销成功后再销毁 context/卸载 DLL；超时不等于可释放，详见接口草案。

事件采用独立 `diagnostics.event.v1` schema，不嵌入 Worker 请求或生产 manifest。字段以接口草案为准：区分 domain/moduleInstanceId/jobId，保留 source PID/TID，沿用打印宿主 module/action/phase/code/elapsedMs/threadRole 等语义；不让接收线程覆盖事件源线程。格式与字段上限在 LD-A00 固化、LD-A01 测试；不对每个像素重复写日志或每层 dump 完整配置。

父子关联优先复用已有 jobId；Worker 通过可选本机诊断 IPC 向所属 DLL 实例发送事件，端点/会话通过子进程专用环境块传递，不传递回调指针，不改宿主全局环境或冻结文件合同。缺失/不支持时保留既有 stderr 与独立日志策略，诊断失败不阻断业务。

Qt bridge 由 EXE 唯一安装，保存旧 handler，防递归并明确转发规则；嵌入 PrintApp 时默认不安装，沿用打印宿主所有权。spdlog Qt formatter 不进入公共接口。

## 6. 落盘、性能与隐私建议

- 默认根目录建议 `%LOCALAPPDATA%/SliceSoft/diagnostics`；允许显式配置可写目录。使用绝对路径和 Windows 宽字符 API，不修改 CWD，不要求管理员权限。
- 会话目录包含 UTC 时间、PID 和随机标识；软件 `app.log`、模块 `slicer_module.log`、RIP `rip.log` 分类保留，均由宿主管理；独立 Worker/CLI 会话单独命名，避免多进程竞争同一 rotating sink。嵌入 PrintApp 时采用宿主目录/轮转策略，不重复默认落盘。
- 正常日志默认 info；debug 由显式诊断选项启用。建议每角色 10 MiB x 3 文件，初始全局保留预算 256 MiB；转储保留 5 份/512 MiB，均为待开发准入确认的建议值。
- 异步队列有界，普通事件采用非阻塞溢出策略并累计丢弃计数；关键失败另有有界应急记录。正常退出 flush 设置等待上限，不能无限等待日志线程。
- 清理只能作用于本组件创建且身份校验通过的历史诊断会话，跳过活跃会话/重解析点；不删除 package、模型、现有 rip 输出或用户任意目录。清理实现需要单独负例测试。
- 转储大小受系统内容影响，保留预算不等于单次写入硬上限；磁盘空间不足时记录失败，不能静默承诺 dump 成功。默认不采集完整进程内存。
- 磁盘满/无权限/后端初始化失败应呈现“诊断降级”并保留已有错误输出；不得把正常切片变为业务成功的假象或吞掉原始错误。
- 日志限制单条长度和子进程总量，记录截断计数；不写纹理像素、模型本体、完整环境变量或秘密。路径/命令参数导出前做脱敏。
- DUMP 可能包含敏感内存，只在本地保存，不自动上传。导出需用户显式触发，默认不附带模型和生产 TIFF。

## 7. DUMP 生命周期与覆盖边界

1. UI/Worker/CLI 各自初始化进程级 reporter；DLL 不安装/撤销全局异常过滤器。打印宿主接入时，由宿主选择已有 reporter 或本组件，不能同时争抢 handler。
2. 推荐每个被监控进程预启动一个 helper，提前准备带访问控制的 IPC、目录和进程句柄；避免崩溃后再启动复杂运行时。
3. 异常路径只提交预先准备的异常上下文标识/线程信息并有界等待；不调用 Qt、spdlog、动态分配密集代码或持有业务锁。helper 必须在故障进程仍存活时读取有效异常上下文。
4. helper 校验目标 PID/会话身份和私有 IPC 版本，正确处理远端异常指针，串行调用 DbgHelp；记录 HRESULT/系统错误、异常码、模块列表与输出大小到独立元信息。
5. 唯一文件名且不覆盖已有文件；转储完成或超时后按原终止语义退出，不尝试从内存损坏继续切片。安装/卸载和既有 handler 共存策略须由 LD-B01 原型验证，不宣称可任意抢占/恢复过滤器。
6. helper 丢失、无权限或超时，明确给出 dump unavailable；不能为诊断阻塞取消或软件退出。

支持声明按实测列出：未处理访问异常作为首个验收；terminate/abort、栈溢出、OOM、fail-fast 分项实测并标注 supported/unsupported。强制结束进程不承诺 dump。崩溃前日志完整性不作保证。

外置 `rip_cli.exe`/`RipSlicer.dll` 不归宿主过滤器覆盖。首版采集其 stdout/stderr、退出码、超时、启动目录、选项、文件存在性和版本/哈希；外部 RIP 转储需要供应方配合或单独授权的进程监控方案。本轮不改系统 WER 注册表，不部署 ProcDump。

此前 `RIP_PROCESS_EXIT_FAILED`、`exitCode=1`、`GetLastError=126` 是依赖加载失败的诊断线索，不等于未处理异常。该类故障重点是日志和部署信息；添加 DUMP 不会自动修复缺失 DLL，也不保证出现 dump 文件。

## 8. 构建、部署与复用验收

首个实现覆盖基础设施、DLL/Worker 内部阶段与软件调用边界，不给 slicer.cpp 大面积加日志，不改 RGBWSV/RGBWSVT 通道、uint8、black_is_print、TIFF、S1/S2 或 Worker 文件协议。SPI 原有导出签名保持不变，日志扩展导出按 LD-A00 单独定案和验收。

根 `CMakeLists.txt` 定义 slicer_module/slicer_worker，UI 有独立 `apps/slicer_ui_host_sim/CMakeLists.txt`；新增配置优先抽入上述 cmake 文件。仅构建 Windows dump target 时链接 dbghelp。

版本信息沿用 `cmake/SliceSoftVersion.cmake` 的清单。Release 开启匹配调试符号的具体编译/链接配置在 LD-C01 处理；按 PE CodeView GUID/age 对应 PDB 归档，不能只凭同名 PDB。可分发包不必携带完整私有符号，但维护侧必须能检索对应版本。

复用验收分两层：本仓库用同一 DLL 对接独立 spdlog 宿主与 mock PrintApp 宿主，验证 C 回调生命周期、等级/字段映射、旧版本降级及不影响既有 logger/Qt handler/filter；真实 PrintApp 接入另行授权，不在参考项目直接修改代码或启动设备。

## 9. 验证计划与开工条件

| Gate | 必须观察的证据 |
|---|---|
| G0 准备 | 新分支基线、引用存在、任务状态一致、git diff --check；无代码/依赖变更 |
| G0-L 日志接口准入 | 接口草案 L0..L5：可选导出版本、新旧宿主/DLL 兼容、实例隔离、注销/卸载/回调异常，以及 DLL 真正发出内部事件 |
| G1 日志组件 | UTF-8/中文路径、轮转/保留范围、并发/队列满、单条超长、初始化失败可重建、磁盘满/无权限、关闭有界 |
| G2 进程与宿主 | host-worker-job 关联及 Worker 诊断 IPC 降级；stdout 合同不受污染；取消/失败后日志仍在；重复模块装卸不影响宿主 logger；注册/未注册两态查询与自检均不新增持久化副作用 |
| G3 转储 | 专用子进程触发故障，异常/线程/模块流有效，匹配 PDB 解析到预设崩溃函数；helper 失败、超时、同秒并发不覆盖、权限限制负例 |
| G4 兼容与预算 | SPI/Worker/S1/S2 回归、真实切片输出语义零漂移；Release 同机 A/B 耗时和峰值内存；先测基线再冻结门槛，不虚构性能百分比 |
| G5 部署复用 | 无开发 PATH 的普通账户包测试、ACP936/1252/65001 隔离路径测试、符号归档、模拟打印宿主共存；真实打印端和物理打印单独标记 |

故障注入仅限 `tests/diagnostics` 专用子进程，显式开关和超时，不主动崩溃用户正在使用的软件。新 CTest 名称以注册结果为准。

后续开发验证命令模板（本轮未运行）：

```powershell
# 在新工作树；不复用原工作树 CMakeCache 或陈旧 build/。
cmake --preset slicesoft-main
cmake --build build-slicesoft/main --config Debug
# 每一步先确认退出码为 0，再继续；先列出实际用例。
ctest --test-dir build-slicesoft/main -C Debug -N
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "diagnostics|stage14c03|stage14c07|stage14d03|rip_integration"
# 精确变更面验证后再执行全量回归，单独记录既有失败。
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure
```

准备结论：双层职责、SDK 参考证据、C 回调增量与兼容矩阵已补齐，可以进入开发准入评审；**不是日志/DUMP 功能已实现**。开发首卡调整为 LD-A00（日志扩展定案），随后是 DLL 出口和软件后端；本轮仅完成文档优化。

## 10. 修订记录

| 日期 | 内容 |
|---|---|
| 2026-09-14 | 建立准备方案；区分源码复用与冻结 DLL ABI；列出打印参考的全局状态/转储风险，明确外部 RIP 覆盖边界；未安装依赖或实施生产接线 |
| 2026-09-14 v0.2 | 按用户双层日志要求补读现用 SDK 头文件及宿主回调；首版增加 DLL 内部出口目标，规划可选 C 扩展、Worker 诊断 IPC、注销与新旧兼容验证；取代初版仅宿主记录的限制，未改实际 ABI |
