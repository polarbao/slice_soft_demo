# slice_soft_demo

这是 UV 3D 打印切片软件的工程化验证仓库，当前已经从 P0 Demo 演进到 Stage 12 切片语义、引擎性能和 Qt 工作台产品化阶段。

当前阶段：

```text
12A：材料填充、支撑和光油语义当前 P0/P1 范围基本完成
12B：R0/R1/R2 已完成，OpenVDB 定位为默认关闭的 SDF utility candidate
当前执行：12C-R0 Qt 工作台构建兼容和基线准入
当前分支：main，执行前仍需通过 git 命令确认
下一任务：12C-R0-01 Qt/MSVC Fresh Build Lane
```

OpenVDB 当前不替代 production `slicer_cli`，不默认启用，也不从 utility/experimental path 写生产 RGBWSV TIFF。生产协议保持 `p0.rgbwsv.2`、`R G B W S V`、uint8 和 `black_is_print`。

12C 只做 Qt 工作台收口：fresh build、Profile、设置生成配置、统一预览和诊断布局。12C-R1/R2 必须等待 R0 fresh Qt UI build gate 通过。

## Codex 接手顺序

请在 VS Code Codex 中先让 Codex 阅读：

1. `AGENTS.md`
2. `docs/slice/README.md`
3. `docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md`
4. `docs/slice/REPORT/REPORT_12C_Qt工作台启动状态.md`
5. `docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md`

第一条 Codex 指令建议：

```text
请先阅读 AGENTS.md、docs/slice/REPORT/REPORT_12C_Qt工作台启动状态.md 和 docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md。
现在只执行 Task 12C-R0-01，不执行后续 Task。
```

确认当前任务完成并提交后，再按 `TASKS_12C_Qt_UI配置预览任务清单.md` 的顺序逐个执行后续任务。
