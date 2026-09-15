# SliceSoft 日志与崩溃转储使用说明

适用范围：包含 LOGDUMP 更新的 Windows x64 切片软件、切片 DLL 日志扩展及本地诊断组件。源码分支合入不自动更新已有 EXE，请使用从对应提交成套构建部署的运行包。
真实 PrintAppLogging 组件适配已在 E01A 验收通过；正式 PrintApp 几何切片业务入口尚未实现，不能据此宣称打印 GUI、干净机器或物理打印验收完成。

## 1. 日志在哪里

正常业务启动后，软件为当前进程创建独立会话，默认位于：

```text
%LOCALAPPDATA%/SliceSoft/diagnostics/ssdiag_<时间标识>_<PID>_<计数>_<角色>/
    session.owner
    session.lock
    app.log
    slicer_module.log
    rip.log
    dumps/
```

| 文件 | 内容 |
|---|---|
| `app.log` | 软件启动、版本、任务阶段及程序自身事件 |
| `slicer_module.log` | 切片 DLL 通过回调发送的内部事件，保留模块实例及源进程/线程信息 |
| `rip.log` | 已接入的 RIP 启动、输出、退出、超时和取消诊断事件 |
| `dumps/*.dmp` | 受支持的未处理异常转储；正常结束不生成 |
| `dumps/*.dmp.partial` | 写入失败后保留的部分转储，不作为完整 DUMP 使用 |
| `dumps/*.dmp.json` | 对应转储的异常码、进程/线程、写入成功状态和文件大小 |

日志采用逐行 JSON，包含时间、等级、业务阶段、错误码、`jobId`、`moduleInstanceId` 等字段。
这些文件独立于切片 `layers`、RIP 输出和任务暂存目录，不改变生产数据合同。
版本/合同查询等无业务入口不一定创建日志会话，不能据此判断模块不可用。

软件菜单“诊断 → 打开日志目录”打开当前宿主会话；“诊断 → 日志与转储状态”显示初始化状态、已写入/丢弃/失败数量和最近日志错误。Worker 和 CLI 采用各自进程会话，排查时也应保留相关时间范围内的会话。

默认等级为 `info`。文件后端每类当前文件上限为 10 MiB，并保留最多 3 份轮转文件；超过队列预算时统计丢弃。RIP 的 stdout/stderr 按最多 2048 字节的 UTF-8 块记录，长输出可能对应多条事件，应结合时间顺序阅读。

新会话启动时执行历史会话保留检查：可识别会话的日志预算为 256 MiB，DUMP 预算为 512 MiB/5 份。清理以完整旧会话为单位；当前/活跃会话、无匹配归属标记的目录、未知文件或重解析点目录会跳过。`session.owner` 是归属标记，`session.lock` 是独占使用凭据，不能手动伪造或复制来“接管”其他目录。

helper 生成的 `crash_<时间>_<PID>_<计数>.dmp.partial` 和同名 `.dmp.json` 也纳入保留统计；每个 `.dmp` 或 `.dmp.partial` 计作一份转储，JSON 只计字节。删除任一旧会话失败后，记录错误并停止本轮清理，保留剩余用量的保守上界，等下次检查重新扫描；不根据可能已经过时的字节统计继续删除其他会话。

上述是尽力维护的保留预算，不是磁盘硬配额：活跃会话仍会增长，跳过的文件不属于可清理范围，系统 I/O 失败也可能导致超限。清理不遍历任意用户目录，不处理切片/RIP 生产文件。需要长期留存的故障证据应按维护流程另行保存。

## 2. 临时启用详细记录

以下 PowerShell 变量只影响从当前终端启动的程序及其子进程，不修改系统环境。应在启动软件前设置：

```powershell
$env:SLICESOFT_DIAGNOSTICS_DIR = "E:\SliceSoftDiagnostics"
$env:SLICESOFT_LOG_LEVEL = "debug"
& ".\runtime\slicesoft\Release\slicer_ui_host_sim.exe"
```

目录必须是可写的普通绝对路径，支持中文目录；诊断根目录经过重解析点检查，不使用符号链接或目录联接绕过归属边界。`SLICESOFT_LOG_LEVEL` 可设置 `trace`、`debug`、`info`、`warn` 或 `off`，未识别的值按 `info` 处理。`off` 关闭普通日志接收，不等于关闭 DUMP。

如需只记录日志，启动前设置 `$env:SLICESOFT_DUMP_ENABLED = "0"`。移除或改为其他值后，自有软件进程恢复尝试启用 reporter。目录不可写、helper 缺失或初始化失败时会显示/记录诊断降级，原业务错误仍保留。

仅在诊断开关 A/B 或明确停用时，启动前设置 `$env:SLICESOFT_DIAGNOSTICS_ENABLED = "0"`，关闭进程诊断会话、文件日志及 DUMP 初始化。设置为 `"1"` 后重新启动可恢复。关闭期间不会有本模块生成的故障日志与转储，不能把此时缺少记录判为故障未发生。

## 3. RIP 故障怎么定位

1. 保留故障会话中的 `app.log`、`slicer_module.log`、`rip.log`，按时间和 `jobId` 对齐；同时保留软件版本与部署包的 `runtime_manifest.json`。
2. 区分 RIP 进程没启动、正常返回失败、执行超时和真正异常崩溃。`exitCode=1` 或 `GetLastError=126` 并不表示软件发生了未处理异常。
3. 遇到 DLL 加载失败，核对报错所指 `RipSlicer.dll` 的实际路径、文件是否存在、依赖是否成套、x64 位数及配置是否匹配。错误 126 也可能来自它依赖的另一个 DLL，不能仅凭主 DLL 存在就排除依赖问题。
4. 对照原有 Package/RIP 验证报告；日志和 DUMP 不会修正缺失依赖，也不会放宽颜色、墨量或 S2 输出规则。

外部 `rip_cli.exe` 的崩溃不由切片宿主进程的异常过滤器自动捕获。首版保留其运行诊断；其 DUMP 仍需供应方 reporter 或另行授权的外部进程方案。不要为了获取转储而随意修改系统 WER 或替换第三方库。

## 4. 如何判断 DUMP 可用

自有 UI、Worker、CLI 进程分别拥有 reporter；`slicer_crash_reporter.exe` 在故障前启动，并在收到本进程的故障信息后写入转储。切片 DLL 不安装全局异常处理器。

成功应有 `.dmp` 和 `success: true` 的 `.dmp.json`。失败且已创建的输出使用 `.dmp.partial` 标识，不能作为完整转储。目录不可写、系统状态损坏或强制结束时，连失败元信息也可能无法保存。

当前本地测试覆盖未处理访问异常、有效异常/线程/模块流、匹配 PDB 符号解析、中文目录并发转储及 helper 超时。尚不承诺所有 `terminate`、`abort`、栈溢出、内存耗尽、fail-fast 或强制结束都能生成转储。崩溃前最后一条异步日志也不保证落盘。

普通运行无须启动 `slicer_crash_reporter.exe`；它由软件管理。不要在真实作业或用户正在使用的进程中执行故障注入。专用测试程序 `diagnostics_crash_child` 不随产品运行时分发。

## 5. 集成到打印软件

日志分为两层：DLL 负责产生事件；软件负责存储、等级、目录和展示。

打印隔离分支 `codex/slicer-logging-adapter` 已提供 `PrintSolution/integrations/slicer_logging/PrintAppSlicerLogAdapter`，通过实际 SpdlogMgr 与真实切片 DLL 的本地验收，详见 [E01A 报告](../slice/REPORT/REPORT_LOGDUMP_E01A_PrintApp真实日志适配验收.md)。主工程选项 `PRINTSOLUTION_ENABLE_SLICER_LOGGING_ADAPTER` 默认 OFF；启用提供可链接组件，尚未替打印 GUI 建立几何切片业务 loader。组件用显式源码路径复用通用实现，已有图像 SliceService 保持原职责。

最小接入不必迁移整套 SliceSoft 日志运行时：使用 `contracts/print_module_spi.h`、`contracts/slicer_logging.h` 和匹配的切片模块部署包，打印软件自行实现轻量 C 回调，把事件复制到自己的队列，再转入现有 `SpdlogMgr`。此路径无需链接 SliceSoft 的宿主日志 target；缺少日志扩展的旧 DLL 继续使用原业务接口。原有 Worker、模型和业务依赖仍按切片模块部署要求提供。

| 接口/组件 | 使用方式 |
|---|---|
| `contracts/slicer_logging.h` | 可选的版本化 C 回调，向后兼容原 SPI v1；不跨 DLL 传递 Qt、STL 或 spdlog 对象 |
| `slicer_log_api_version` | 动态查询日志扩展版本；旧 DLL 缺少扩展时继续原有业务接口 |
| `slicer_set_log_callback_v1` | 为一个模块实例注册一个回调和宿主 context |
| `slicer_clear_log_callback_v1` | 注销并等待已进入的回调；只有返回成功后才能释放 context/模块并卸载 DLL |
| `ModuleLogBinding` | 本仓库的宿主适配器，负责可选导出探测、事件复制及注销生命周期 |
| `LogSessionOptions::sink` | 注入打印宿主出口，可转入已有 `SpdlogMgr`；注入后不另外创建 SliceSoft 文件后端 |

回调只复制/入队，不直接操作 GUI、不做阻塞文件 I/O、不重新调用模块。任何非 `SLICER_LOG_OK` 返回都不是“已经注销”；必须保留 context、模块和 DLL，并继续协调注销，不能释放或卸载。日志扩展返回码独立，不覆盖业务 `pm_last_error`。

同一 DLL 分别接文件后端和模拟打印宿主的可编译示例见仓库 `tests/diagnostics/HostAdapterIntegrationTests.cpp`。宿主注入的核心形式如下，实际打印端需映射它自己的等级枚举和 logger 生命周期：

```cpp
LogSessionOptions options;
options.sink = [printHostSink](LogChannel channel, int level, std::string_view json) {
    printHostSink(channel, level, json); // Host-owned adapter, returns promptly.
};
auto session = LogSession::Create(std::move(options), &error);
ModuleLogBinding binding(ResolveModuleLogging(loadedDll), module, session);
// Run existing SPI requests. A non-OK result must retain every bound resource.
const int result = binding.Clear(5000);
```

打印宿主仍拥有全局 logger、Qt 消息处理器和崩溃处理器。可复用 `CrashReporter` 默认拒绝替换已有异常过滤器；仅自有 EXE 在统一管理启动/退出时显式接管，停止时恢复原处理器。初始化/注销必须与其他异常处理器所有者串行，不能假设可与任意第三方并发替换。

`ModuleLogBinding` 的 RAII 销毁会持续重试直到注销成功，以避免在活动回调下面释放内存。宿主日志关闭也会等待消费线程结束。永久不返回的宿主 sink、错误生命周期或永久失败的注销不能安全地强制取消，因此这些路径不承诺硬性的退出时限；打印宿主适配器必须及时返回，并在非 GUI 的受控停机流程中完成注销。

如需直接复用本项目的宿主适配器，可迁移以下最小源码集合；`.h/.cpp` 表示同名头文件和实现文件：

| 用途 | 文件集合 | 对应 target / 依赖 |
|---|---|---|
| 事件与合同 | `contracts/print_module_spi.h`、`contracts/slicer_logging.h`、`src/diagnostics/LogEvent.h/.cpp` | `slicesoft_diagnostics`；`nlohmann_json::nlohmann_json` |
| 现成日志适配器 | `src/diagnostics/host/LogSession.h/.cpp`、`ModuleLogBinding.h/.cpp`、`SessionRetention.h/.cpp`，以及 `src/diagnostics/spdlog/FileLogSink.h/.cpp` | `slicesoft_diagnostics_host`；上述事件 target 及 `spdlog::spdlog` |
| 可选崩溃客户端 | `src/diagnostics/windows/CrashReporter.h/.cpp`、`CrashProtocol.h` | `slicesoft_diagnostics_windows`；Windows API |
| 可选转储 helper | `src/diagnostics/windows/CrashCapture.h/.cpp`、`CrashProtocol.h`、`apps/slicer_crash_reporter/main.cpp` | `slicer_crash_reporter`；Windows `dbghelp`，随所选 EXE 部署 |

当前 `LogSession.cpp` 仍引用默认 `FileLogSink`，所以注入 `sink` 只改变运行时出口，**并未移除现成适配器的编译/链接期 spdlog 依赖**。打印软件应复用其已选定的 spdlog/fmt 配置，保证 C++20、x64、CRT 及 Debug/Release 一致，不另建全局默认 logger。表中 `SessionRetention` 是当前宿主 target 的组成部分，只有宿主明确调用时才执行会话创建/清理。

`ProcessDiagnostics` 和 Qt `HostDiagnostics` 属于 SliceSoft EXE 的启动、目录、环境变量和菜单策略，不默认迁入 PrintApp；打印端沿用自己的策略和异常处理器所有权。Worker 日志管道和 DLL 内部 `ModuleLogRegistry` 留在切片 SDK 内部，不要求打印宿主重新实现。

`cmake/SliceSoftDiagnostics.cmake` 当前是本仓库装配入口，包含仓库根路径、固定目标的符号配置及测试注册，不能直接作为独立 SDK 的 CMake 包引入。E01A 的打印适配目录已提取所需 target 声明，通过 `SLICESOFT_DIAGNOSTICS_SOURCE_DIR` 调整根目录与 include 路径，复用打印工程的依赖查找；这些基础组件不依赖 Qt、`slicer_core` 或 RIP。适配组件的构建/注销/日志共存已验证，正式业务 loader 接线和整个 PrintApp 退出流程仍需在 E01B 完成。

### 独立源码包

现在可用 [源码 SDK 导出脚本](../../scripts/ExportSliceSoftDiagnosticsSdk.ps1) 自动生成最小可迁移集合，无须手工挑选文件。在切片源码根执行，输出必须是全新的绝对路径：

```powershell
$sdkRoot = Join-Path (Get-Location) ('output/logdump/sdk-' + [Guid]::NewGuid().ToString('N'))
./scripts/ExportSliceSoftDiagnosticsSdk.ps1 -OutputDirectory $sdkRoot
```

包内包含 23 个源码/合同/构建/说明文件和 `sdk_manifest.json`，清单记录文件 SHA 与源版本。包顶 CMake 可独立构建，也可 `add_subdirectory` 后链接 `SliceSoft::DiagnosticsHost`；构建步骤见 [SDK 说明](../../sdk/diagnostics/README.md)。消费者提供现有 spdlog/fmt/nlohmann_json，保持 MSVC x64 和 CRT 配置一致。

PrintApp 现有适配工程可直接把 `SLICESOFT_DIAGNOSTICS_SOURCE_DIR` 指向导出包，继续复用自己的 SpdlogMgr。E02 已用该方式完成真实成功切片和原负例共 2 项工程验收，详见 [交付报告](../slice/REPORT/REPORT_LOGDUMP_E02_源码SDK交付与成功切片验收.md)。正式产品 loader 仍需另行挂接。

转储源码随包提供，`SLICESOFT_DIAGNOSTICS_BUILD_CRASH` 默认 OFF；显式启用后由宿主部署 helper 并管理过滤器。源码包不含 DLL、第三方运行库、PDB 或故障数据，不替代最终二进制模块包；LOGDUMP F01 已为 `PackageSlicerModule.ps1` 补齐 helper 和日志合同头部署。使用时仍须核对实际包清单，不能只复制源码包就认为 Worker 转储运行时已齐全。

## 6. 打包与符号

运行时包增加 `slicer_crash_reporter.exe`，并按构建系统实际选中的 Debug/Release spdlog、fmt 库复制 DLL 和许可证。静态链接不附加对应 DLL，但仍保留依赖清单和许可证。不要混用 Debug/Release 文件；系统 `dbghelp.dll` 不从开发机复制进包。

打包脚本为 6 个自有 EXE/DLL 核对 PE CodeView 与实际 PDB 的 GUID/age，同时核对归档二进制与待发布暂存二进制的 SHA-256，并在构建目录的 `symbol-archive/<配置_时间_标识>/` 下归档二进制、PDB、构建清单及 `symbols_manifest.json`。运行时的 `diagnostics.symbols.archiveId` 用于定位维护侧归档；该新增诊断清单不记录机器私有的绝对符号路径，PDB 不放入普通运行时包。缺少、不匹配 PDB 或归档与实际待发布文件不一致时，打包会失败，应重建对应版本，不能拿同名旧 PDB 补齐。

对一次故障进行分析时，需要对应版本的二进制、匹配 PDB 和 DUMP；仅文件名相同不足以证明符号匹配。当前解析范围为常见 MSF 7 PDB，异常或不支持的格式明确失败，不绕过校验。[MSF 格式](https://llvm.org/docs/PDB/MsfFile.html)、[PDB Info Stream](https://llvm.org/docs/PDB/PdbStream.html)

DUMP 可能包含进程内存、业务路径和其他敏感内容，PDB 可能包含私有源路径及实现细节。默认只在本地保存，不自动上传；分享前按项目流程确认范围，普通故障反馈不必附带模型本体、纹理和生产 TIFF。

开发维护侧可在专项完整包部署后执行以下命令，复制到新的中文测试目录，以受限 PATH 和包内 Windows Qt 平台运行 `--diagnostics-self-test`，验证启动、日志与依赖；不保存用户工作区设置，保留证据、不删除已有目录。这属于本机部署冒烟检查，不替代干净机器验收：

```powershell
& .\tests\diagnostics\TestDiagnosticsRuntime.ps1 `
    -RuntimeRoot "runtime/logdump/Release" `
    -EvidenceRoot ("build-slicesoft/main/diagnostic-evidence/runtime_" + [Guid]::NewGuid().ToString("N"))
```
