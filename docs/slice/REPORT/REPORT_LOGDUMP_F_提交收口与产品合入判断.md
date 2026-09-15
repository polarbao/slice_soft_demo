# LOGDUMP F 提交收口与产品合入判断

日期：2026-09-15。任务状态见 [LOGDUMP 清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)。

## 边界与结论

切片侧双层日志、DLL 可选回调、Worker 诊断 IPC、EXE 转储、二进制部署和源码 SDK 已交付。正式打印软件 P23 几何切片 loader 不存在，E01B 不能完成；本专项不代建整个打印业务入口。也不将 GUI 人工操作、干净机器、其他 ACP 或物理打印标成通过。

产品基线 `d28b6451` 已在专项分支通过 `eafbfec7` 整合，四处共同修改自动合并，保留短 EXE 名、兼容别名、高 DPI/交互、工艺恢复和 RIP 进度。`product/packaged-slicer` 本身未移动，未推送。其他 P0FIX 提交和原工作树新增的代码、模型、缓存均未纳入。

合入判断为 **CONDITIONAL GO（仅切片侧诊断功能合入）**：专项分支已包含产品基线，可快进；本轮未发现剩余的新增诊断功能回归。条件是明确承接下述既有红灯及外部未验收项，不能把合入当作生产准入。若 product 的准入要求 Stage 14 所有 Gate 全绿，则当前仍为 **NO-GO**，必须先处理这些独立问题。本次按用户要求完成判断，不直接移动 product。

## 本轮修复

1. 旧 `PackageSlicerModule.ps1` 无法从 PE 导入表发现动态启动的 reporter，现显式携带它并扫描其依赖，同时携带两个配套 C 头。
2. 模块打包及验包仍拿源版本 `0.2.0-dev` 比较 Git 派生版本，现按现有运行包规则核对版本线，同时严格核对构建清单、DLL、Worker 的完整版本。
3. 全目标构建发现 `ModuleClient.h` 将日志实现头暴露给仅引用客户端声明的测试。改用前置声明，完整头移至实现单元；修复后全目标 Release 重建退出 0。
4. 冻结 JSON 在新 worktree 被自动转为 CRLF，原 SHA 校验报错。只为该文件固定 LF 检出，未修改 JSON 内容或预期 SHA，校验重新通过。

原始 SHA 为 `a42f05beeb499ca47c9920f8d5b780c0e555d748dc346f24451a453608bc88c7`；LF 还原后为冻结值 `377b68067c542bdca320ac8e8dcc7537c85c2cf07bfdd0caaa9b4b6a80aa0da2`。

## 验证

证据根为专项工作树 `build-slicesoft/main/diagnostic-evidence/`。先确认构建退出码再执行测试，不对失败构建的旧程序报通过。

| 项目 | 实际结果与证据 |
|---|---|
| 合并前日志专项 | 13 项通过；首轮 12/13，Windows PowerShell 继承的模块路径导致 Get-FileHash 不可见，设置进程级 PSModulePath 后该项通过；未改系统环境 |
| 整合 configure | 退出 0，未升级依赖；使用已安装 vcpkg 版本 |
| Release 全目标构建 | 首轮失败于上述头依赖，`f-integrated-build.txt`；修复后 `f-integrated-rebuild.txt` 退出 0 |
| 首轮扩大回归 | 67 项，61 PASS / 6 FAIL，141.74s；`f-integrated-ctest.txt`。非全仓回归 |
| 交付校验复测 | fixture 通过；F05 部署、纯 C 宿主成功切片、M1 本地验收均通过，最后仍在既有 S1 目录期望处失败；`f-gate-retest.txt` |
| 最终扩大回归 | 67 项，62 PASS / 5 FAIL，150.66s；`f-final-ctest.txt`，退出 1。日志/DUMP、SPI、Worker、RIP 和高 DPI/工艺恢复相关新增验收通过；不宣称全仓或所有 Gate 全绿 |
| 独立运行包部署 | `f-runtime-deploy.txt` 退出 0，31 个场景；`runtime/logdump-f/Release`，不覆盖用户正在使用的原树运行包 |
| 中文路径、受限 PATH | `f-portable/summary.json` PASS；新宿主启动、reporter ready、DLL 回调、Qt 日志及正常关闭通过，未产生意外 DUMP |
| 模块实际验包 | `output/logdump/module-package-99fa88b33aa641d9ac73c08d3201c428`：helper/依赖/SHA/无 PDB 通过；`output/logdump/f-module-validation`：纯 C 宿主完成 3 层写包并 Reader 验证通过 |
| 新 DLL 与打印后端 | 成功和负例 EXE 分别退出 0；证据在打印专项树 `PrintSolution/build/logdump-sdk-consumer-vs/final/ok_日志 add456114368`、`final/printapp_日志_35484_483943812`。EXE 从 E02 导出的 SDK 构建，本轮更换为新整合 DLL 运行，未声称重编整个 PrintApp |
| 源码门禁 | `f-source-size.txt`：PASS，0 ERROR / 74 既有 WARNING，基线 product/packaged-slicer |
| SDK 导出重验 | `sdk-export-test-c64350a1b91f433c959a605cd771da1d`：23 文件、哈希、中文路径、拒绝覆盖/交叠/reparse 均通过 |

最终失败逐项归属：

- `slicer_stage14c04_sync_capability_safety_test`：ViewData 请求成功，但历史安全用例预期失败；与产品基线 P0-00 所录一致。
- `stage14f03_single_model_s1_gate`：宿主已成功写 3 层并验证，脚本却仍查旧 `stage14e01_package` 路径；与基线一致。
- `stage14f05_local_closure_gate`：本轮修正打包版本后通过部署/M1 子门，继续被同一 S1 目录期望阻断。该门在历史 Debug 基线显式跳过，不能简单算作 Debug 的既有失败数。
- `slicer_stage14e02_qt_host_boundary_test`：既有 `HostMainWindow.cpp` 516 行，不在债务名单；与基线一致。
- `slicer_stage14e04d_dual_view_contract_test`：既有缺贴图被显示为灰模型的断言失败；与基线一致，不修改规则掩盖。

## 产物身份

构建时快照 `eafbfec7b1db.dirty`、版本 `0.2.467-dev`；dirty 对应随后独立提交的客户端头隔离修复。后续提交不将这些程序冒充为新 SHA 的干净发布构建。

- DLL：`DC716D3A10597A96D1A7F15E9B15419F3187138144ADF2812509FF902E6E8670`
- Worker：`F2CADFEB89F8A428335E9E92BA649ADEF08E9A6B0CF07C62BC62095D57102A1A`
- `slice_soft_test.exe`：`B8DDD73DEC29FE28FAFE3BCCC2DC198A19BCD7D2EBC4049F2893BA2AD78CFD37`

## 提交分组

| 提交 | 任务 |
|---|---|
| 19065268 | 双层日志、可选 DLL 回调、Worker IPC、宿主/CLI 和转储运行时及测试 |
| 48e175eb | 部署、私有符号归档、模块 helper 和用户说明 |
| cf019c48 | 可迁移源码 SDK、导出清单与拒绝测试 |
| 34392284 | 专项方案、准备、任务清单和前期验收证据 |
| eafbfec7 | 在专项分支整合 product 基线，不移动 product |
| 557b3eed | 全目标构建发现的客户端声明依赖隔离 |
| 859fa483 | 模块包派生版本验证与冻结样本 LF 检出 |
| 打印仓库 c63b2510 | 可选 PrintApp 日志适配及真实成功作业验收 |

日志、DUMP、私有 PDB、运行包、用户模型和缓存不在提交中。打印分支仍为独立 `codex/slicer-logging-adapter`，未合并到打印产品分支。

## 保留事项

- E01B 等待 P23 几何切片 loader；已有图像通道化服务不能替代该入口。
- 264 字符输出路径受既有 Windows 文件 I/O/进程与系统长路径配置限制；本轮不修改注册表、不宣称已经支持。
- S1 目录期望、ViewData 安全/缺贴图断言和宿主行数问题保持独立归属。本轮源码对比确认相应生产实现、S1 脚本与 product 一致，不通过放宽规则让它们转绿。
