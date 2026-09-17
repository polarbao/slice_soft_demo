# LOGDUMP A00 开发准入与日志扩展定案

> 2026-09-14，ACTIVE；用户已授权按计划继续任务、先完成各任务准备，并由执行者决定并行。本决策承接 v0.2 设计，授权范围为本工作树本地实现/验证，不包含打印项目修改、硬件操作、合入或提交。

## 准备结论

已检查专项工作树（仅前期文档），现有 C SPI、模块生命周期、Qt 动态加载、Worker CreateProcessW、版本与部署入口。打印 SDK 参考以 IMPORTED 二进制头文件及宿主调用为准，不假设供应库内部实现。

- 保留 SPI v1 的 11 个 pm_* 签名；独立 `slicer_logging.h` 增加 3 个日志扩展符号，扩展版本 1，宿主动态探测，缺失/未知版本只关闭模块日志。
- C 调用约定 __cdecl；回调参数为宿主不透明 context、int 等级、UTF-8 JSON 指针和长度。字符串仅在回调期间有效，宿主必须复制；禁止跨边界对象/所有权转移。
- 扩展返回码：0 成功，-1 参数/无效句柄，-2 状态/重入，-3 注销超时，-4 资源失败；不覆盖 pm_last_error。Off=-1，事件等级 trace..critical 为 0..5。
- 单条 JSON 上限 16 KiB，模块每实例队列 256 条；宿主队列 1024 条。消息超长在编码前截断并标记，队列溢出累计计数；不在业务线程调用文件 sink。
- 每实例串行调回调，不持业务锁；clear 成功后零回调，超时不释放 context。已注册不支持原地替换；关闭先停作业/注销，再销毁和卸载。任意宿主回调永久阻塞无法保证安全有界卸载，不强杀线程。
- 无注册不开文件/不建日志线程。版本查询、自检和 pm_last_error 不记录可落盘事件。DLL 不链接 spdlog、Qt，不安装异常过滤器。

## 依赖与构建准备

既有方案比较 spdlog/Boost.Log，选择打印宿主同源 spdlog。固定现有 vcpkg baseline `d13fa75214c258099923cf25a5e6311e58c07f3b`；实际读取该提交端口为 spdlog 1.17.0/MIT，默认 fmt 功能，`x64-windows`。不升级 baseline，不复制安装树。新工作树独立 configure/build，工具链为已存在的 `D:/Program Files Tools/vcpkg`，Qt 5.15.2。

spdlog 编译库的 Windows 文件名类型受编译选项约束；本实现不单边定义 WCHAR_FILENAMES。通过私有 filesystem::path 文件 sink 实现 Unicode 路径与轮转，仍使用 spdlog logger/格式/等级，传递依赖部署由 CMake/vcpkg 与打包脚本验证。后端不修改 spdlog 全局默认 logger/registry/thread pool。

## DUMP 准备

B01 采用预启动 helper，匿名共享内存与 event，经显式 HANDLE_LIST 传递给 helper，不开放全局命名端点。健康阶段预备目标 process HANDLE，崩溃路径仅写异常上下文/通知/有界等待。helper 使用宽字符 CREATE_NEW、MiniDumpWriteDump 并记录成败。默认最小转储，不启用 full memory；仅宿主 EXE 启用。可复用组件默认拒绝覆盖已有过滤器；实际发现自有测试 EXE 启动已存在过滤器（地址归属本 EXE，未推断具体来源），因此自有 UI/Worker/CLI 明确设置 replaceExistingFilter=true，停止时恢复；不向 DLL 或 PrintApp 强制这一策略。启动等待 3 秒、转储等待 5 秒；故障注入仅专用子进程。

日志建议目录为 LOCALAPPDATA/SliceSoft/diagnostics，app/module/rip 各 10 MiB、3 个归档；数据保留与转储清理只在组件拥有的历史会话目录执行，活跃/重解析点跳过。初版资源与退出表现必须实测，不承诺磁盘设备无限阻塞时的有界 flush。

## 分工与验收顺序

根执行者负责合同、通用事件/宿主后端、构建/依赖、应用接线与文档；DLL 代理负责模块出口/内部调用/测试；DUMP 代理负责 Windows runtime/helper/故障测试。共享 CMake、manifest 和任务表由根维护。

先构建新组件/模块与针对性 C ABI、注销、双实例、宿主轮转测试，再接软件/Worker。DUMP 先独立故障子进程验证后再接入 EXE。随后进行冻结面回归、独立运行包/符号核对、文档收口；未通过的项不能标 COMPLETE。原工作树的 UI/RIP 未提交演进不纳入此分支，合入前重新解决接线差异。
