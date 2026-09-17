# LOGDUMP G 产品合入与复用交付收口

日期：2026-09-15。范围以 [G 准备](../DOC/DOC_PREP_LOGDUMP_G_产品合入与复用交付准备.md) 和 [任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md) 为准。

## 当前结论

用户已明确授权实际产品合入。在原日志隔离工作树切换到 `product/packaged-slicer` 后，执行 `git merge --ff-only codex/feature-logging-dump` 成功，产品分支从 `d28b6451` 前进至 `7354a616`。功能分支已被产品包含，保留作追溯，不删除、不推送；本报告随后以文档提交保存在 product。

切片侧日志/DUMP 的开发、复用交付和本地产品合入完成。打印 E01B 仍等待 P23 几何切片 loader，属于后续打印产品接线，不伪装成已完成。原工作树 `slice_soft_demo` 仍用于 P0FIX，其未提交代码/模型/资料未被修改、切换或纳入合入；原目录中的旧运行包没有被覆盖。

## 本轮实际验证

构建和运行时证据位于当前产品工作树 `build-slicesoft/main/diagnostic-evidence/`，不是原 P0FIX 树的构建目录。

| 项目 | 结果 / 证据 |
|---|---|
| 干净源码 Release 全目标重建 | `cmake --build build-slicesoft/main --config Release --clean-first --parallel 6` 退出0；`g-product-build.txt` |
| 与 F 相同的67项回归 | 62通过/5失败，162.60s，CTest退出8；`g-product-ctest.txt`。不是全仓全绿 |
| 完整应用部署 | `PrepareSliceSoftRuntime.ps1 -DeployOnly` 退出0，31场景；`g-product-deploy.txt`，目标 `runtime/slicesoft/Release` |
| 中文目录/仅系统PATH启动 | `TestDiagnosticsRuntime.ps1` PASS，`g-product-runtime/summary.json`；helper就绪、DLL回调注册、Qt消息及正常关闭落盘，无意外转储 |
| 源码SDK导出 | `output/logdump/product-7354a616-sdk`，23文件；导出前后源码均clean |
| SDK导出负例 | `TestDiagnosticsSdkExport.ps1` PASS；`g-sdk-export`，文件/SHA、中文目录、禁止覆盖/交叠/reparse均验证 |
| SDK独立构建 | `build-slicesoft/logdump-product-sdk` 配置退出0；RelWithDebInfo日志、转储客户端、helper三个目标构建退出0，`g-sdk-build.txt` |
| 真实打印后端重新消费 | 在打印隔离树新建 `PrintSolution/build/logdump-product-consumer`，从新导出SDK配置/编译退出0；使用新产品DLL的成功作业及原负例CTest 2/2 PASS，0.60s；不是旧EXE或旧DLL验收 |
| 文档与提交边界 | 6份交付/任务文档22个相对链接及围栏检查通过，git diff --check通过；最终仅状态文档更新，无新增生产代码修改 |

打印证据：`PrintSolution/build/logdump-product-consumer/ctest.txt`、`evidence/ok_日志 57723b7b0d22`、`evidence/printapp_日志_34824_491590046`。本轮未修改打印仓库源码，也没有合入打印产品分支或操作硬件。

5项失败与F收口一致：ViewData安全用例的成功/失败预期不一致；S1脚本旧输出路径；依赖S1的F05汇总门；宿主516行边界门；缺贴图被灰模型替代的断言。未放宽测试或改生产逻辑让它们转绿。诊断/DUMP、SPI、Worker、RIP及相关宿主验收通过；本次产品合入不等于完整Stage14或生产准入通过。

## 产物与启动

本次清洁构建源码为 `7354a6163bbe9abcdeb59f8b176724cb43f1f35e`，版本 `0.2.471-dev`。最终状态文档提交不会改变本批二进制的源码身份。

- 产品工作树：`E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump`，当前为 `product/packaged-slicer`。
- 启动：`runtime/slicesoft/Release/slice_soft_test.exe`；兼容入口 `slicer_ui_host_sim.exe` 保留。
- 日志：默认 `%LOCALAPPDATA%/SliceSoft/diagnostics/<会话>/`；启动时自动开启info，可在“诊断”菜单打开日志与查看状态。已有EXE不会因Git合入自动更新。
- 源码SDK：`output/logdump/product-7354a616-sdk`，可从清单追溯合同版本及文件SHA。
- DLL SHA256：`944504E73A8AA74097A4E357528D61540470E6E9DEE6E970C43D5121EBB53902`。
- Worker SHA256：`B2C52936E5F462FDB24D98CDE717A708F0CBDCA07C3FC2C69B6199D1D04062A0`。
- UI SHA256：`54022951C45ABA62B4C8CF3CA92EB4DB5F29109CDA1D9D563AAB7569B4AF1CC5`。
- SDK清单 SHA256：`4B8CFD355EECEC45BB1C2F2907F9F82151537FC54FC22AEA4B02C792CF84AC3C`。

## 后续复用合同

见 [SDK使用规范](../../../sdk/diagnostics/README.md) 和 [用户说明](../../user_guides/SLICESOFT_日志与转储使用说明.md)。仅通过可选C接口跨DLL边界；事件为UTF-8结构化日志。宿主用 `LogSessionOptions.sink` 接入现有后端，或使用现成 `PrintAppSlicerLogAdapter` 复用打印 `SpdlogMgr`。不跨DLL共享Qt/C++/spdlog对象，也不双重初始化全局logger。

必须在释放模块/DLL/回调上下文前成功注销，注销非成功时保活重试。打印EXE继续拥有异常过滤器；不要直接迁移SliceSoft EXE的启动策略。可选源码target不等于正式打印业务已经挂接。

E01B正式产品接线、干净机器/其他ACP/实际打印GUI/物理打印及既有长路径限制继续保留。外部RIP进程不由宿主helper自动捕获。队列溢出、磁盘故障和极端异常下不承诺每条日志或每种崩溃均能保存。
