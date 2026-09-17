# LOGDUMP E02 源码 SDK 交付与成功切片验收

> 2026-09-15，LD-E02-01 / LD-E02-02 COMPLETE。
> 状态真源：[专项任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)；前置：[E02 实施准备](../DOC/DOC_PREP_LOGDUMP_E02_可迁移源码包与成功作业验收准备.md)。

## 1. 本轮结论

已交付可独立构建的日志/可选转储源码 SDK，并通过真实 PrintAppLogging + 切片 DLL/Worker 的成功切片验收。打印消费工程的四个通用诊断编译源均来自导出包，不依赖完整切片源码目录。原故意失败请求验收保留，最终 CTest 2/2 PASS，0.93 s。

本轮完成的是可迁移组件及独立工程验收。E01B 正式 PrintApp 业务接线仍缺 P23 loader；没有修改产品几何入口、设备、算法、业务合同或原工作树。切片 DLL 继续使用先前可信包，未重新编译或替换它。

## 2. 交付物

切片专项工作树：`E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump`，分支 `codex/feature-logging-dump`，起点 e2546797，工作区仍有未提交实现。

- `scripts/ExportSliceSoftDiagnosticsSdk.ps1`：只导出 23 个白名单文件，记录源 HEAD、dirty 状态、导出器和逐文件 SHA/尺寸；拒绝覆盖、交叠、源/目标 reparse。
- `sdk/diagnostics/`：包内相对路径 CMake、使用与依赖说明；提供 `SliceSoft::Diagnostics`、`SliceSoft::DiagnosticsHost`、显式可选 `SliceSoft::CrashReporter` 和 helper。默认不编译/启用转储，不引入或升级依赖。
- `tests/diagnostics/TestDiagnosticsSdkExport.ps1`：正例及覆盖/交叠/junction 拒绝检查。
- `src/diagnostics/host/SessionRetention.cpp`：Windows 宏防重复定义，兼容打印父工程 `/WX`，不改会话清理行为。

打印专项 `E:/__Code/__Work/ry_print_demo-slicer-logdump` 的 `integrations/slicer_logging` 新增成功作业 EXE、固定模型/Profile fixture 和准备脚本，更新可选 CMake 与说明；根 `.gitignore` 为该 OBJ 源夹具精确放行，避免被通用 `*.obj` 编译产物规则漏交付。旧负例 CPP 和正式 PrintAppLogging 实现不变。

最终源码包位于切片专项：

```text
output/logdump/E02_20260915_72b3397f/日志源码SDK/
output/logdump/E02_20260915_72b3397f/verification.json
```

源码包不含切片 DLL、第三方二进制、模型、日志、DUMP、PDB 或 PrintApp 源码。实际验收生成的模型/切片包保留在打印测试证据目录，不混入 SDK。

| 身份 | 实测值 |
|---|---|
| SDK 文件数量 | 23 + `sdk_manifest.json` |
| SDK manifest SHA256 | `2F9639A26629F97E55E171F166F5342CBDC8A978544AA7192CB24C6CA309FD59` |
| SDK 源码 | e25467976635… + dirty；实际内容以清单逐文件 hash 为准 |
| 切片 DLL | `runtime/logdump/Release/slicer_module.dll`，0.2.450-dev，Release /MD，SPI 1、日志 API 1 |
| DLL SHA256 | `1E7435461811725D591AB410A2C8E2339BBDF8E0B0D733751D512A4BA1A89CA3` |

## 3. 实际验证

主环境为 Visual Studio 18 2026 / MSVC 19.51 / Windows x64 / RelWithDebInfo。依赖复用打印现有 x64-windows prefix，没有安装或升级；包装脚本加载 Qt 环境变量不表示这些 targets 链接 Qt。

| 检查 | 结果 |
|---|---|
| 新 SDK 的日志、转储客户端、helper | configure/build 退出 0；显式启用可选 crash，三个目标通过，并预定义宿主常见的两个 Windows 宏验证兼容性 |
| PrintApp 两个验收 EXE | configure/build 退出 0；诊断源码根是导出的中文 SDK 目录，XML 构建项目核对四个源均在包内 |
| 原负例 | PASS，0.24 s；非法中文 jobId 仍由原合同拒绝，两个实例分别转交 7/1 条日志 |
| 成功作业 | PASS，0.68 s；真实导入、场景创建/快照、Worker 切片、Package Reader、模型释放六项调用均成功 |
| 切片数据 | 3 个 TIFF、16×16 像素，Reader `valid=true`、`errors=[]`；127 DPI / 0.2 mm 为测试配置 |
| 成功日志 | 接收/转交均为 18，丢弃/转交失败断言均为 0；实例 1、Host PID 36824、Worker PID 50980，作业成功终态落盘 |
| 生命周期 | 回调成功注销、业务错误保持、默认 logger/等级/线程池保持；注销后 app/module 文件继续可写 |
| 导出及证据保护 | 中文路径、23 文件 SHA/尺寸、拒绝覆盖/相对路径/源码交叠/源与目标 junction 均通过；重用成功证据目录返回 1，旧 summary SHA 不变 |
| 源码行数门禁 | PASS，0 ERROR / 74 个既有或允许 WARNING，基于 e2546797；不是全量业务回归 |
| 最终文档/改动检查 | 14 份相关文档、80 个仓内链接均通过，围栏/空白通过；两个专项工作树 `git diff --check` 退出0；新成功验收CPP 378行 |

SDK 构建证据位于切片 `build-slicesoft/main/diagnostic-evidence/e02-sdk-final-vs/build-corrected.txt`。

打印证据根为 `PrintSolution/build/logdump-sdk-consumer-vs`：

- `ctest-buffer-fix.txt`：最终 2/2 PASS，0.93 s。
- `build-final.txt`、`build-buffer-fix.txt`：实际编译结果。
- `evidence/ok_日志 6b2870268a56/summary.json`：成功数据、日志及每个请求/终态；同目录 `package/` 为真实切片输出。
- `evidence/printapp_日志_42112_479191531/summary.json`：原负例结果。
- `freshness-rejection.txt`：旧证据目录拒绝测试。

切片导出拒绝测试证据：`build-slicesoft/main/diagnostic-evidence/sdk-export-test-41a46595d1bd427ab64ad44b22443ff7`。完整身份关系记录在最终包同级 `verification.json`；它不是生产运行时清单。

## 4. 发现与处理

1. `SessionRetention.cpp` 无条件定义 Windows 宏，会与打印宿主冲突；补保护后在预定义宏的 SDK 编译中通过。
2. 导出长度和摘要读取之间可能发生源变化；复制后及最终检查同时核对长度和 SHA，不产出具有旧尺寸的完成清单。
3. 成功验收异常路径可能覆盖既有 summary，或错误保留 PASS；新增本次证据可写标志、catch 强制 FAIL，并实测拒绝旧目录不改变文件。
4. 首次 SDK 宏验证命令覆盖 `CMAKE_CXX_FLAGS` 时遗漏 CMake 默认的 `/EHsc`，导致 C4530；修正验证命令保留 `/EHsc /GR`，不是依赖或源码故障。初始失败日志保留为 `build-final.txt`，最终日志另存。
5. 首次成功作业在 264 字符报告路径打开失败。保留中文/空格，仅缩短测试根标签；完整作业随后通过。本轮未修生产 Writer 的超长路径支持，最终产品长路径需另验，不能宣称任意深度路径可用。
6. 成功作业进度快速变化，暴露测试 helper 错把两次查询长度视为恒定的问题；按原 SPI 允许变化，限定 4 MiB / 8 次重试，校验实际返回长度、NUL 和缓冲不足无部分写入。业务 DLL/合同未改变。

## 5. 复现与剩余边界

从切片根导出到全新绝对路径，再从打印 `PrintSolution` 目录构建：

```powershell
# $sdkRoot、$sliceModule、$dependencyPrefix 由本机现有源码包、可信 DLL 和依赖目录提供。
../scripts/invoke_printsolution_cmake.ps1 -S integrations/slicer_logging -B build/logdump-sdk-consumer-vs -G 'Visual Studio 18 2026' -A x64 -T host=x64 "-DCMAKE_PREFIX_PATH=$dependencyPrefix" "-DSLICESOFT_DIAGNOSTICS_SOURCE_DIR=$sdkRoot" "-DSLICESOFT_LOGGING_TEST_MODULE=$sliceModule"
../scripts/invoke_printsolution_cmake.ps1 --build build/logdump-sdk-consumer-vs --config RelWithDebInfo --target PrintAppSlicerLogAcceptance PrintAppSlicerSuccessfulJobAcceptance --parallel 4
ctest --test-dir build/logdump-sdk-consumer-vs -C RelWithDebInfo --output-on-failure
```

E01B 仍依赖正式几何装载器及产品关闭流程；E02 不实现它。当前 P0FIX 主线没有合入/验证，原有 S1/ViewData 失败也不因这两个工程测试而关闭。GUI、RIP、设备、干净机器和其他 ACP 未验证；本轮可选转储仅验证源码构建，未重跑异常注入。

旧 `PackageSlicerModule.ps1` 未显式部署 crash helper 的缺口仍在，不能把本源码 SDK 当成已修复的二进制模块发行包；已部署应用包内使用说明也未随本轮重新发布。最终二进制交付及符号/指南同步需按 E01B 新基线准备重新处理。
