# LOGDUMP E01A PrintApp 真实日志适配验收

> 2026-09-14，LD-E01A COMPLETE；LD-E01 总项 PARTIAL_COMPLETE，E01B ENTRY_NOT_IMPLEMENTED。
> 状态唯一真源：[专项任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)。
> 前置：[E01 实施准备](../DOC/DOC_PREP_LOGDUMP_E01_PrintApp真实日志适配准备.md)。未提交、推送或合入。

## 1. 结论与交付

真实 `PrintApp/src/utils/SpdlogMgr.cpp` 与真实切片 DLL 的日志接入已通过本地工程验收。新增 `PrintAppSlicerLogAdapter` 复用切片 `LogSession` 和 `ModuleLogBinding`，把原始 UTF-8 JSON 转入 `SpdlogMgr::ModuleLogger("slicer_module")`。原 SPI、Worker 文件合同和生产数据语义保持不变。

本轮是可复用组件交付。PrintApp 当前没有几何切片 DLL loader；已有 SliceService 处理图像通道化，不能当作该入口。没有启动 PrintApp GUI、设备 SDK、运动控制或物理打印。E01B 等待正式业务入口及其生命周期设计。

| 工作树 | 分支 / 起点 | 本轮内容 |
|---|---|---|
| `E:/__Code/__Work/ry_print_demo-slicer-logdump` | `codex/slicer-logging-adapter` / `22bfcd3d235d767c45f6a3af3ad7bed6f275a581` | `PrintSolution/integrations/slicer_logging` 内适配器、验收、CMake、说明；主 CMake 默认 OFF 可选入口 |
| `E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump` | `codex/feature-logging-dump` / `e2546797` | 准备/任务/报告/手册，以及两个公共源文件的宏防重复保护 |

两个原工作树的无关未提交文件均留在原位。打印 SpdlogMgr 源实现没有修改；两工作树按 Git 规范化后的 blob 均为 `e500b74480344ede30c292aeae8f90cc4119e486`，文件 SHA 因 LF/CRLF 不同不能直接当作内容差异。

## 2. 实测结果

打印主证据根：`E:/__Code/__Work/ry_print_demo-slicer-logdump/PrintSolution/build/logdump-adapter-vs`。

| 检查 | 实际结果 / 证据 |
|---|---|
| 主工程默认 OFF | `build/logdump-default-off-vs` configure/generate 退出 0；cache 为 OFF，无适配 target；未构建完整 PrintApp |
| 主工程显式 ON | `build/logdump-adapter-parent-vs` 最终 configure/generate、`PrintAppSlicerLogging` RelWithDebInfo 构建退出 0；复用已有 PrintAppLogging target |
| 独立实际后端验收 | `PrintAppSlicerLogAcceptance` 最终构建退出 0；CTest 1/1 PASS、0.26 s；`acceptance-ctest-final.txt` |
| 两实例与 Worker | `evidence/printapp_日志_40320_410616281/summary.json`：实例 1/2，转交 7/1 条，丢弃/转交失败断言为零；Worker PID 30680 与宿主 PID 40320 不同，事件精确归属实例 1 |
| 中文与错误保持 | 同一证据目录及 `logs/slicer_module.log` 保存 UTF-8 原事件；`worker_terminal.json` 保存完整结果与 poll 终态 |
| 等级与可选扩展 | 7 种等级映射，Off=6 到 -1，未知值拒绝；无扩展句柄和扩展版本 2 fixture 降级，未误调用未知版本 set/clear，业务错误不变 |
| 宿主所有权 | clear 正常成功、负 timeout 参数拒绝；注销后 app/module logger 继续落盘；默认 logger、等级、全局线程池、EXE 原异常过滤器不变，无额外 SliceSoft 私有日志会话 |
| 依赖核查 | dumpbin：验收 EXE 导入 `spdlog.dll`、`fmt.dll` 和系统/Release CRT；没有 Qt、PrintSDK 或运动 SDK 导入；动态业务加载范围仅指定切片 DLL |
| 切片兼容回归 | 两处宏保护后 Release 目标重建退出 0；diagnostics_host_tests、diagnostics_host_adapter_tests 2/2 PASS、0.29 s；`build-slicesoft/main/diagnostic-evidence/ctest-e01-compat-release.txt` |
| 最终静态/文档核查 | SourceSizeGuard PASS、0 ERROR/74 个既有或允许警告；8 份 LOGDUMP 文档的 62 个仓内链接通过；切片 60 个新文本、打印 6 个新文件无行尾空白，两个工作树 diff --check 退出 0；交叉审查无剩余确定缺陷 |

本测试特意传入不符合既有 ASCII jobId 规则的中文身份，验证真实 Worker 拒绝时日志仍保留原始 UTF-8。业务终态为 `failed`、`PM-SLICER-CONTRACT-0060`，detail 为 jobId 文件合同错误；这是预期负例，不是成功切片或允许中文业务 jobId 的证据。

验收中的 `written` 只表示已转交打印 sink；文件持久化由测试中的宿主 Flush 和实际文件轮询确认。此 EXE 没有注入真实注销超时或高压丢弃；相关底层测试证据仍见 [本地实现报告](REPORT_LOGDUMP_本地实现与验证状态.md)。Off 的零 accepted 是组合路径证据，不单独证明 DLL 从未执行回调。

## 3. 身份与构建复现

VS 18 2026、MSVC 19.51.36256、x64、RelWithDebInfo `/MD` 宿主；切片包 DLL 为 Release `/MD`、版本 0.2.450-dev、SPI 1、日志 API 1。此 CRT 兼容验收不替代未来 loader 的正式 buildConfig/版本准入。

固定加载上一轮已部署的 `runtime/logdump/Release/slicer_module.dll`，SHA256 为 `1E7435461811725D591AB410A2C8E2339BBDF8E0B0D733751D512A4BA1A89CA3`。本轮切片兼容回归使用重新构建的 build 目录，没有重新发布运行包或符号归档。

依赖复用打印项目既有 x64-windows 安装目录，spdlog 1.17.0、fmt 12.1.0、nlohmann_json 3.12.0；没有安装或升级依赖。验收运行目录 Release `spdlog.dll` SHA256 为 `47EFE514831D8D47864B886CB67D7AC9597A203E1EBECEA8064D7CCF96FF6CE7`，`fmt.dll` 为 `590EB77FFEA0794661B7C70B2CE97316B3D28AFF104C4E91905DCF084E26F91F`。

在打印隔离工作树的 PrintSolution 下执行：

```powershell
../scripts/invoke_printsolution_cmake.ps1 -S integrations/slicer_logging -B build/logdump-adapter-vs -G 'Visual Studio 18 2026' -A x64 -T host=x64 '-DCMAKE_PREFIX_PATH=E:/__Code/__Work/ry_print_demo/PrintSolution/build/vcpkg_installed/x64-windows' '-DSLICESOFT_DIAGNOSTICS_SOURCE_DIR=E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump' '-DSLICESOFT_LOGGING_TEST_MODULE=E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump/runtime/logdump/Release/slicer_module.dll'
../scripts/invoke_printsolution_cmake.ps1 --build build/logdump-adapter-vs --config RelWithDebInfo --target PrintAppSlicerLogAcceptance --parallel 4
ctest --test-dir build/logdump-adapter-vs -C RelWithDebInfo --output-on-failure
```

完整主工程采用相同源码路径、`PRINTSOLUTION_ENABLE_SLICER_LOGGING_ADAPTER=ON`、`BUILD_TESTING=OFF` 及现有 Qt/dependency prefix，配置到独立 `build/logdump-adapter-parent-vs`；本次未使用 vcpkg toolchain 自动映射，而是明确传入 `'-DCMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO=Release;'` 和 `'-DCMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL=Release;'`。结尾空项保留原厂商 SDK 无配置导入目标，不把它强制视为名叫 Release 的目标。本轮只构建 adapter target，不替换或运行 SDK。

## 4. 验收发现与修复

1. 打印工程预定义 WIN32_LEAN_AND_MEAN，切片两个复用源再次定义导致 `/WX` 下失败。已在 LogEvent.cpp、ModuleLogBinding.cpp 增加 `#ifndef` 保护，并通过父工程编译和切片回归。
2. 测试版本 fixture 的导出声明不一致导致 C2375。已改为测试 DLL 自身单一导出定义，正式 C 合同不变。
3. 仅使用 dependency prefix 时，RelWithDebInfo 首次错误选择了 Debug spdlog/fmt，实际运行出现 `0xc0000374`。独立 CMake 现在在查找依赖前设置缺省 Release 映射，并在验收启动、日志 Init 前拒绝已加载的已知 Debug 日志 DLL。重新链接的导入表和最终真实验收均通过。父工程通过显式配置映射验证，不在子模块改写全局依赖政策。
4. LoadLibraryExW 在 DLL_LOAD_DIR 搜索模式下收到 CMake 正斜杠路径，返回 126。同一 DLL 规范化为 Windows 反斜杠路径后加载成功；验收 loader 已加 lexically_normal/make_preferred，没有放宽搜索范围或替换 DLL。
5. 交叉审查修正 Worker 只检查属于“任一实例”的不足，现在精确匹配第一实例；完整读取业务终态。真实合同在更早的 jobId 层拒绝，因此校验该既有错误，没有改业务规则来让测试通过。

## 5. 后续入口

E01B 需要正式几何切片 loader 后按“停止作业 → clear 成功 → 销毁适配器 → 销毁模块/卸载 DLL → 打印日志 Shutdown”接线。当前 PrintApp main 在部分局部对象析构前调用 Shutdown，届时必须显式安排先后顺序。

正式 PrintApp GUI、有效模型切片、设备并发、打印宿主转储、其他 ACP 和干净机器尚未验收。本轮不变更产品版本清单里的切片业务集成状态，不自动合入两个工作树。
