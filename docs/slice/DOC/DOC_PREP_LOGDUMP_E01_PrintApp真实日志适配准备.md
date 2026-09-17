# LOGDUMP E01 PrintApp 真实日志适配准备

> 2026-09-14，PREPARATION COMPLETE。用户本轮授权按既定计划继续，先准备再开发，并允许自行决定并行。
> 关联：[专项任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)。本轮不提交、合入或控制硬件。
> 准备后执行结果：E01A 已于 2026-09-14 完成，真实后端验收 1/1 PASS；详见 [E01A 报告](../REPORT/REPORT_LOGDUMP_E01A_PrintApp真实日志适配验收.md)。下文 Current State 为开工前快照，正式状态以任务卡为准。

## Implementation Plan

### Problem Type

原 LD-E01 将日志后端复用和打印产品业务入口挂接合在一项，当前源码不具备后者前置。需拆分可完成的适配器/真实组件验收与未来业务接线，不能把已准备图像 SliceService 当几何切片模块。

### Layer(s) Involved

切片通用事件/宿主回调适配层，PrintAppLogging 实际日志后端，独立工程验收入口。打印的设备层、调度层和业务 UI 不在范围内。

### Official Documents

切片 LOGDUMP A00、现行 C 头与本地验证报告；打印仓库 AGENTS.md、.agents/docs/always-on-rules.md、architecture-runtime-boundary.md、build-cmake-vcpkg.md。打印文档明确真正 SlicerService 属于第二阶段目标。

### Historical Documents

LOGDUMP v0.2 准备/草案保留为历史；上一轮模拟 PrintApp sink 的 PASS 不能当真实 SpdlogMgr 集成证据。

### AI Workspace Evidence

切片工作树 `E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump`，分支 codex/feature-logging-dump，基线 e2546797；已有日志实现未提交。

打印原仓库 `E:/__Code/__Work/ry_print_demo`，分支 codex/test-engineering-consolidation，基线 22bfcd3d235d767c45f6a3af3ad7bed6f275a581，存在未跟踪图片/文档/任务目录，原位保留。已创建独立工作树 `E:/__Code/__Work/ry_print_demo-slicer-logdump`，分支 codex/slicer-logging-adapter，worktree 创建退出 0。

### Current Code Reality

- PrintApp 当前无 pm_create/pm_destroy/slicer_module 的消费入口；不能在 SliceService 上假设该生命周期。
- SpdlogMgr::ModuleLogger 已提供专属模块文件和全局异步线程池，PrintAppLogging 为内部静态库。SpdlogMgr 拥有初始化/Flush/Shutdown，适配器不能代为调用这些生命周期函数。
- 日志 sink 为阻塞入队模式，因此切片回调仍复用 LogSession 的有界队列，使打印 sink 阻塞隔离在消费线程；永久阻塞时仍不能承诺有界安全关闭。
- 切片 ModuleLogBinding 已实现可选版本探测、同步 clear 及保留回调上下文；直接复用源文件，不复制一套生命周期实现。
- 两项目现有 vcpkg baseline 相同且都有 spdlog/nlohmann_json。此次不引入或升级依赖。

### Current State

切片侧已完成；模拟宿主通过，真实打印日志后端尚待本轮验收。完整 PrintApp 未有几何切片服务，不能声称已经形成产品业务链路。

### Target State

LD-E01A：打印仓库提供独立可选 PrintAppSlicerLogging target，显式路径引用切片通用源码，注入真实 SpdlogMgr 模块 logger；正常打印构建默认不启用。独立验收 EXE 只链接实际日志后端/适配器，动态加载真实切片 DLL，验证版本、内部日志、注销、宿主共存及错误隔离，不链接或调用设备 SDK。

LD-E01B：未来几何切片业务 loader 创建时，在其 create/clear/destroy/unload 生命周期挂接；当前状态 ENTRY_NOT_IMPLEMENTED，不在本专项建设完整第二阶段业务。

### Historical State

原任务表 E01 WAITING_AUTH 已由本轮“按计划继续”授权推进至可独立实施部分；没有据此授权设备控制、替换厂商 SDK、合入或改写历史。

### Pending Confirmation

本轮可独立开发项无待确认参数。正式业务 loader 的接口、UI 操作、作业归属与分发路径是 E01B 产品输入，不能猜测后填。

### Risk Points

异步日志进入真实 SpdlogMgr 后还需要宿主 flush；清理只释放自身会话/回调，不清除宿主 logger、线程池或 DUMP filter。缺失日志扩展只能降级日志，不能伪造模块加载或切片成功。部署证据需区分两个项目版本与未提交源码状态。

### Files To Change

打印隔离工作树：PrintSolution/integrations/slicer_logging 内适配器、独立 CMake 和验收程序/说明；PrintSolution/CMakeLists.txt 的默认 OFF 可选入口。复用 PrintApp/src/utils/SpdlogMgr.cpp 和切片 src/diagnostics，不修改其默认后端或业务接口。

切片隔离工作树：本准备文档、任务卡、E01 验证报告与入口索引；构建审查后补充 LogEvent.cpp、ModuleLogBinding.cpp 的 WIN32_LEAN_AND_MEAN 防重复定义保护，避免打印主工程已有宏在 /WX 下冲突。不修改 RGBWSV、S1/S2、Worker 文件协议或已完成切片算法。

### Verification Plan

先核对两个工作树状态和依赖版本；主验证使用 MSVC DevShell、Visual Studio 18 2026、RelWithDebInfo 和独立 build/logdump-adapter-vs，不使用旧 Ninja/Debug。构建独立验收 EXE 和 adapter，并执行真实 DLL 与真实 PrintAppLogging 的事件/等级/版本/注销/宿主共存测试。记录丢弃与转发语义、旧模块降级与中文路径结果；检查无设备 SDK 静态依赖、无额外 DUMP 所有权、默认构建不启用外部源路径。

根执行者控制 CMake/构建/任务表，代理负责适配器实现与独立测试审查，不并发操作同一构建目录。验证失败先定责，不将未测试的 PrintApp GUI、打印或干净机器标记通过。
