# LOGDUMP 本地实现与验证状态

> 2026-09-14，LOCAL COMPLETE / PRINTAPP ADAPTER COMPLETE / BUSINESS ENTRY DEFERRED；对应分支 `codex/feature-logging-dump`，起点 `e2546797`。本地收口保留两项既有回归失败与下文验证限制，不表示全仓或外部生产 Gate 通过。
> 唯一任务状态源：[专项任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)。
> 2026-09-15 最新提交、产品基线整合与扩大回归见 [F 收口报告](REPORT_LOGDUMP_F_提交收口与产品合入判断.md)；本文件下方“未提交/未合入”等为前期记录。

## 1. 已实现与影响范围

DLL 通过 `contracts/slicer_logging.h` 三个独立可选 C 导出提供实例日志，宿主管理文件、等级和关闭生命周期。原 11 个 SPI v1 导出签名不变。DLL 不依赖 spdlog/Qt、不安装全局异常处理器；Worker 使用单独诊断 IPC，不改变业务文件协议。独立切片软件默认使用私有 spdlog 后端，注入打印宿主 sink 时不重复落盘。

UI、CLI、Worker 接入进程日志和预启动 Windows crash helper；外部 RIP 记录命令、stdout/stderr、退出和失败信息。RIP 失败日志不等于自动修复供应 DLL 加载错误，外部 rip_cli 也不由本进程 reporter 覆盖。helper 仅在专用故障子进程中注入异常验证。

新增会话保留规则只处理带正确所有权标记且不活跃的已知文件；重解析点、硬链接、未知内容跳过。`.dmp.partial` 计入转储预算，删除失败停止本轮回收。依赖固定现有 vcpkg baseline 的 spdlog 1.17.0、fmt 12.1.0，未升级 baseline。

切片源码、文档与运行包均在 `E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump`；原工作树未提交 UI/RIP/模型改动保留原位。后续 E01A 在打印隔离工作树 `E:/__Code/__Work/ry_print_demo-slicer-logdump` 增加可选日志适配器并通过真实组件验收，见 [E01A 报告](REPORT_LOGDUMP_E01A_PrintApp真实日志适配验收.md)；两个原工作树未改，均未提交、未合入。

## 2. 已执行验证

证据根为本工作树 `build-slicesoft/main/diagnostic-evidence`。

| 验证 | 实际结果 | 证据或说明 |
|---|---|---|
| Debug/Release 所需目标构建 | PASS，退出 0 | 日志/IPC/helper/宿主/Worker/CLI/Reader/纯 C 宿主及相关测试，未宣称全仓构建 |
| Debug 专项 CTest | 16/16 PASS，6.64 s | `ctest-debug-final.txt`；回调清理失败重试、partial 保留、RIP 超长输出包含在内 |
| Release 组合 CTest | 17/18 PASS，11.75 s | `ctest-release-final.txt`；S1 固定目录期望失败，见第 4 节 |
| 补充 Debug 合同测试 | 8/9 PASS | ViewData 既有期望失败；S2、Worker 取消/协议与纯 C 边界通过 |
| Debug UI | 实际启动/模块日志绑定/Qt 消息/正常关闭 PASS | `ui-debug-initial`；早期自检参数未禁用 QSettings，已改为含 self-test 的新参数；最终包验证使用新参数 PASS |
| Debug 切片 A/B | fixture、segment_101 均逐层 TIFF SHA256 一致，Reader PASS | `ab_Debug_1789380431469363700`、`ab_Debug_1789381393987652200`；每组 1 次预热+1 次实测/模式 |
| Release fixture A/B | 逐层 TIFF SHA256 一致，Reader PASS | `ab_Release_1789382676863125400`；每模式 1 次预热+3 次实测 |
| Release 真实模型 A/B | 184 层 TIFF SHA256 一致，Reader PASS | `ab_Release_1789382924409966000`；每模式 1 次预热+3 次实测 |
| 独立 Release 部署 | PASS，退出 0 | `runtime/logdump/Release`，锁定版本、RIP 完整性、Qt/诊断依赖、6 组匹配 PE/PDB 归档；未覆盖原运行包 |
| 中文路径/受限 PATH 运行包 | PASS，退出 0 | `runtime-release-final/summary.json`；复制 644 文件，Worker/Host 退出 0，PATH 只有 System32，依赖许可 SHA、模块日志绑定、Qt 桥接与 helper 就绪均 PASS |
| 源码与文档检查 | PASS | SourceSizeGuard 0 ERROR/74 个既有或允许警告；diff/新增文本空白、Markdown 链接、脚本语法检查 |

上述 CTest 内的 PowerShell 5.1 测试需要 Windows 模块路径。本机 Codex PowerShell 7 继承的模块路径最初令 Get-FileHash 自动加载失败，使用下列局部环境修正后 Debug 全部通过；未通过失败的配置/构建结果推断测试成功。

```powershell
$env:PSModulePath="$env:ProgramFiles/WindowsPowerShell/Modules;$env:SystemRoot/System32/WindowsPowerShell/v1.0/Modules"
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R '^(diagnostics_.*|session_retention_tests|module_log_dispatcher_tests|module_event_pipe_tests|module_logging_integration_tests|host_rip_diagnostics_tests)$'
```

## 3. Release A/B 基线

| 输入 | 层数 | off / info 中位耗时 | 增量 | off / info CLI 峰值工作集 |
|---|---:|---|---|---|
| material_process_top2_fixture | 20 | 205.778 / 269.835 ms | +64.057 ms（31.13%） | 14,221,312 / 14,958,592 B |
| segment_101 实际 OBJ | 184 | 4218.424 / 4592.998 ms | +374.574 ms（8.88%） | 49,655,808 / 50,429,952 B |

真实输入为 `model/obj/reality/260805-11-50-11-034-segment_101.txt.obj`，配置沿用 `samples/configs/relief/relief_nail_white_support.json`，635x600 DPI、0.038 mm、白墨实体及支撑。这不是历史 21 um 几何采样比较。每组共 8 次运行，首对预热不计中位数，所有运行均比较全层 SHA256 并经 Reader 校验。

这是同一二进制的本机初始观测，包含启动、异步日志关闭和 helper 成本；机器未作为独占基准环境，未建立跨机器性能阈值。工作集只测 CLI，不含 helper；不能声称零性能开销或把单次百分比外推到所有模型。

```powershell
python scripts/VerifySliceSoftDiagnostics.py --build build-slicesoft/main --config Release --repetitions 3
python scripts/VerifySliceSoftDiagnostics.py --build build-slicesoft/main --config Release --repetitions 3 --source-config samples/configs/relief/relief_nail_white_support.json --model model/obj/reality/260805-11-50-11-034-segment_101.txt.obj
./tests/diagnostics/TestDiagnosticsRuntime.ps1 -RuntimeRoot runtime/logdump/Release -EvidenceRoot build-slicesoft/main/diagnostic-evidence/runtime-release-new
```

运行包测试只写新的证据目录，不删除已有目录。Windows 已有 VC 运行时等机器状态没有隔离，因此其 PASS 不能替代干净机器验收。

## 4. 已有失败与限制

- `slicer_stage14c04_sync_capability_safety_test`：scene.get_viewdata 实际 succeeded，测试期望 failed。
- `stage14f03_single_model_s1_gate`：纯 C 宿主完成 3 层切片并输出 `HOSTFLOW_HA03_PASS`，测试仍查找 `stage14e01_package`，实际目录为 `package_46004_406965812`。对实际产出单独执行 Reader 已 PASS，不能据此把原 CTest 记为通过。
- 两者已分别记录在 [PRESET 任务清单](../../codex_task/current/TASKS_PRESET_工艺可配置面收敛与自测用例收口专项任务清单.md) 第 13 节；本专项不修改原期望或 S1/S2 合同隐藏失败。
- 实测 DUMP 覆盖为专用子进程未处理访问异常；abort/terminate、栈溢出、OOM、fail-fast、强制结束及外部 RIP 不在支持承诺内。UI 初始化 reporter 在 QApplication 创建之后，之前的加载/初始化故障不覆盖。
- 任意宿主回调或磁盘永久阻塞时，不承诺有界安全卸载/日志关闭，也不强杀日志线程。异步队列允许丢弃并计数；崩溃前最后一条日志不保证已持久化。
- 磁盘故障验证使用可控 sink 失败，不实际填满用户磁盘。跨线程性能只作本机观测，未验证所有高负载场景。早期 Debug UI 冒烟曾采用未禁用设置持久化的旧参数；已修为 `--diagnostics-self-test`，最终包验证使用新参数。
- 模拟 PrintApp sink 共存已验证；后续 E01A 也已完成真实 PrintAppLogging + DLL 组件验收。正式 PrintApp GUI/几何切片业务、其他 ACP、干净机器、物理打印尚未验证。

## 5. 交付与后续边界

本轮本地工作完成。Release 包位于 `runtime/logdump/Release`，默认日志根 `%LOCALAPPDATA%/SliceSoft/diagnostics`，菜单“诊断”可打开本次日志目录或查看日志/转储状态。符号归档为 `build-slicesoft/main/symbol-archive/Release_20260914T104238379_c38386fb`；PDB 不进入用户运行包。

PrintApp 最小消费方式为 C 合同头及宿主回调，完整适配器复用清单见 [日志与转储使用说明](../../user_guides/SLICESOFT_日志与转储使用说明.md)。LD-E01A 已完成真实日志后端适配和本地验收；LD-E01B 因尚无正式几何切片 loader 保持 ENTRY_NOT_IMPLEMENTED。E01A 没有重新部署本报告的独立切片运行包。后续合入需重审原工作树未提交 UI/RIP 接线差异。
