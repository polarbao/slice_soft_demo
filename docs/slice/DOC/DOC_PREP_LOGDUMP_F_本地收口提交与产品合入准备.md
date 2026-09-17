# LOGDUMP F 本地收口、提交与产品合入准备

日期：2026-09-15。用户授权完成可执行余项、按任务提交，并判断是否可合并到 product 分支；先在专项分支整合验证，不移动 product 或推送远端。

## Implementation Plan

- Problem Type：专项收口、部署遗漏修复、产品分支兼容整合。
- Layer(s)：诊断基础设施、EXE 部署、宿主入口、SDK 适配和任务文档。
- Official/Historical Docs：LOGDUMP A00 决策、E01B/E02 准备与报告；历史验证仅作背景，不作为新基线通过证据。
- AI Workspace Evidence：专项清单 v1.4；两个隔离工作树及现有构建/验收产物。
- Current Code Reality：切片专项基于 e2546797，已有双层日志、可选 C 回调、Worker IPC、EXE crash helper、源码 SDK；尚未提交。旧 PackageSlicerModule.ps1 未显式部署动态启动的 helper。
- Current State：本地组件及源码消费验收已完成；E01B 正式打印业务入口缺失。原切片工作树已在 codex/feature-p0fix-contract-robustness，HEAD 3dd204ad；只有未跟踪缓存、协作资料和模型，保留原位。
- Target State：补齐模块包；任务分组提交；在专项分支整合 product/packaged-slicer 固定基线 d28b6451aa1479d7a51ac9f2cc7d7a516c5d7ab0，验证后报告产品合入结论。产品分支后的 12 个 P0FIX 提交不通过本任务隐式带入。
- Historical State：E02 的 264 字符报告路径失败发生于既有文件 I/O；ReportWriter/Utf8Path 未由本专项改动，当前系统 LongPathsEnabled=0。不得以修改系统策略或缩短测试目录声称解决生产长路径支持。
- Pending Confirmation：正式打印几何 loader/P23 的产品建设、GUI 人工/干净机器/物理打印验收属于后续工作；不是本地日志组件合入的虚构完成项。
- Risks：合并交叠包括 UI CMake/Main、RIP 控制器和运行包脚本；必须保留短 EXE 名、兼容别名、交互与进度。原 S1/ViewData 失败必须核对基线。日志 clear 非成功必须继续保活 context 和 DLL。
- Files To Change：PackageSlicerModule.ps1、诊断部署测试、必要的审查修复、上述交叠文件和专项状态/报告；不改切片算法、S1/S2、RGBWSV 或整个打印业务。
- Verification Plan：脚本语法与源码门禁；Release 构建成功后再运行日志/DUMP、SPI/Worker、宿主/UI/RIP 相关回归；实打独立模块包核对 helper/依赖/SHA/无私有 PDB；验证产品整合后源码 SDK 消费与成功切片；git diff --check。真实失败和未验证面保留。

## 任务与执行顺序

1. LD-F01：补模块包 helper、可选日志头与部署验证；审查清理可复现问题。代理负责打包和独立代码审查，根执行者控制共享构建。
2. LD-F02：按通用诊断/DLL 与 Worker、宿主接线、部署/源码 SDK、文档任务提交；打印适配器在独立打印分支提交。提交不含运行数据、私有符号、用户模型或缓存。
3. LD-F03：在切片专项分支整合 product 固定基线，解决交叠后重新构建和验收；报告是否可快进 product。保留专项分支与原工作树，本次不移动 product、不推送。

当前开工前检查已完成。状态与实际结果回写专项任务清单；本文件是准备而非验证完成报告。
