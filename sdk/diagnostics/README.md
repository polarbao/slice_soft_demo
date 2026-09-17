# SliceSoft 日志与转储源码 SDK

本包由 `ExportSliceSoftDiagnosticsSdk.ps1` 从明确的源文件清单导出。它提供切片 DLL 的可选日志合同、宿主队列适配器和可选 Windows 转储组件，不包含切片 DLL、Worker、模型、Qt、打印设备 SDK 或第三方库实现。

`sdk_manifest.json` 记录导出时的源码 HEAD、工作区实际 dirty 状态以及逐文件 SHA-256。HEAD 不能代替未提交源码的身份，应同时保留完整清单。日志 API 和 SPI 版本从随包头文件读取；使用时仍需核对实际 DLL 的版本、配置与运行库，源码包存在不等于二进制已通过准入。

## 文件与依赖

| 内容 | 位置 |
|---|---|
| 原 SPI 与可选日志合同 | `contracts/print_module_spi.h`、`contracts/slicer_logging.h` |
| 事件、异步队列、DLL 回调绑定、文件出口与可选会话保留 | `src/diagnostics/` |
| 可选崩溃客户端和 helper 源码 | `src/diagnostics/windows/`、`apps/slicer_crash_reporter/` |
| 导出身份和文件完整性 | `sdk_manifest.json` |
| 消费依赖及许可证说明 | `DEPENDENCIES.md`、`licenses/` |

依赖由消费者已有的 CMake/vcpkg 环境提供：`spdlog::spdlog`、其配置选用的 fmt，以及 `nlohmann_json::nlohmann_json`。不会联网安装或升级依赖。注入宿主 sink 仍有编译期 spdlog 依赖，因为 `LogSession` 保留默认文件出口。

仅支持 Windows x64 / MSVC / C++20。本包 targets 使用 Release/RelWithDebInfo 的 `/MD` 与 Debug 的 `/MDd`；消费者和对应依赖必须匹配。独立配置时，未指定的 RelWithDebInfo/MinSizeRel 导入映射默认为 `Release;`，兼容无配置的 imported target；通过 `add_subdirectory` 消费时不改宿主映射政策。

## 构建与消费

独立配置示例，变量指向本机实际路径，构建目录必须与源码目录分开：

```powershell
cmake -S $sdkRoot -B $buildRoot -G "Visual Studio 18 2026" -A x64 `
    "-DCMAKE_PREFIX_PATH=$dependencyPrefix"
cmake --build $buildRoot --config RelWithDebInfo --target slicesoft_diagnostics_host
```

宿主使用 `add_subdirectory` 后链接 `SliceSoft::DiagnosticsHost`；仅需要事件编码时链接 `SliceSoft::Diagnostics`。它们不会初始化全局 logger、线程池、Qt 消息处理器或异常过滤器。

```cmake
add_subdirectory("${SLICESOFT_DIAGNOSTICS_SDK}" "${CMAKE_BINARY_DIR}/slicer-diagnostics")
target_link_libraries(MyHost PRIVATE SliceSoft::DiagnosticsHost)
```

原 PrintApp 的 `integrations/slicer_logging` 可以把 `SLICESOFT_DIAGNOSTICS_SOURCE_DIR` 指向本包根目录，继续使用其真实 `PrintAppLogging` 及适配器。该入口按自己的 CMake 编译同一份源码，无须再 `add_subdirectory` 本包，避免重复编译/定义。打印适配器和 `SpdlogMgr` 属于打印工程，本包不复制它们。

## 日志生命周期

宿主先初始化日志、加载可信 DLL 并创建模块实例，之后探测三个可选导出并创建 `ModuleLogBinding`。旧 DLL 缺少扩展时保留原业务调用。回调只复制/入队，不阻塞、不操作 GUI、不重入 DLL；源事件中的 UTF-8 和身份字段原样保留，业务 `pm_last_error` 不由日志扩展覆盖。

停止业务后，必须先成功注销回调，再销毁适配器、模块并卸载 DLL，最后关闭宿主日志。`Clear` 返回任何非成功码都必须保留绑定及借用资源，随后重试。RAII 注销和消费线程排空可能等待；永久阻塞的 sink 不承诺硬性的退出时限。`LogSessionStatus.written` 表示已转交 sink，不代表外部异步 logger 已落盘。

`SessionRetention` 可由宿主明确调用；链接库本身不会创建或清理目录。`ProcessDiagnostics`、SliceSoft 环境变量、Qt 菜单与 Worker 内部管道不属于本包，宿主保留自己的策略。

## 可选转储

默认 `SLICESOFT_DIAGNOSTICS_BUILD_CRASH=OFF`。明确启用后构建 `slicesoft_diagnostics_windows` 和 `slicer_crash_reporter`，宿主链接 `SliceSoft::CrashReporter` 并部署 helper：

```powershell
cmake -S $sdkRoot -B $buildRoot -DSLICESOFT_DIAGNOSTICS_BUILD_CRASH=ON
cmake --build $buildRoot --config RelWithDebInfo --target slicesoft_diagnostics_windows slicer_crash_reporter
```

只有 EXE 所有者决定安装异常过滤器，在正常启动时设置 helper 的完整路径、可写转储目录并调用 `Start`。默认拒绝替换已有过滤器；只有自有 EXE 统一管理其他处理器时才显式设置 `replaceExistingFilter=true`。启动/停止须与其他处理器所有者串行，切片 DLL 不承担这项所有权。

helper 使用系统 DbgHelp，不随包复制系统 DLL。生产二进制与匹配 PDB 应由宿主独立归档；本源码包不配置宿主的符号发布策略。完整转储和 `.partial` 必须区分；不承诺强制结束、永久系统损坏或所有异常类型均可捕获，也不会捕获外部 RIP 进程的崩溃。

本源码包不替代二进制模块包。LOGDUMP F01 已修复 `PackageSlicerModule.ps1`，模块包显式部署运行时启动的 helper 和两个 C 合同头；消费时仍应核对实际包的文件清单及 SHA，不能用旧包冒充新交付。完整应用包和最终打印业务 loader 的验收仍分别管理。

## 复用准入清单

1. 使用同一版本的模块二进制包与日志合同头；动态探测日志 API v1，不向 DLL 传递 C++、Qt 或 spdlog 对象。
2. 由宿主初始化自己的 logger，并通过 `LogSessionOptions.sink` 接收结构化事件；打印端沿用 `PrintAppSlicerLogAdapter`，不重复初始化或关闭 `SpdlogMgr`。
3. 每个模块实例绑定自己的上下文；回调只复制入队，事件中的源进程、线程、作业及实例标识不可用消费线程信息替代。
4. 停止业务后成功 `Clear` 才可释放上下文、模块和 DLL，最后关闭宿主日志；注销失败必须保活并重试，不能先卸载 DLL。
5. 嵌入打印软件时不照搬 SliceSoft 的 `ProcessDiagnostics` 启动策略；转储由打印 EXE 所有者决定，避免接管其现有异常过滤器。
6. 成套部署 Worker、helper、资源和运行库，保留与二进制匹配的私有 PDB；用实际成功/失败/取消作业与注销后宿主继续写日志验证最终接线。

这些接口和组件已实现；正式打印几何切片 loader 的业务挂接属于 E01B 后续任务，并非仅链接源码 target 即自动完成。
