# CODEX_PROMPT 12E-09C X/Y DPI 执行指令

> 状态：09C-01..05 COMPLETE / 09C-06 READY

执行前阅读：

```text
AGENTS.md
docs/slice/DOC/DOC_DECISION_12E_09C_XY_DPI非等方分辨率兼容.md
docs/slice/PRD/PRD_12E_09C_XY_DPI配置与生产协议兼容.md
docs/slice/DEV/DEV_12E_09C_XY_DPI配置Reader与UI设计.md
docs/slice/DEMO/DEMO_12E_09C_XY_DPI验证方案.md
docs/slice/DOC/DOC_PREP_12E_09C_XY_DPI准备.md
docs/codex_task/current/TASKS_12E_09C_XY_DPI任务清单.md
```

固定要求：

```text
新默认 dpiX=635、dpiY=600；
显式 600/600 继续有效；
UI 分轴设置；
Legacy/Global 共用同一 DPI 合同；
Reader 严格校验非等方 package；
历史 fixture 不批量改写；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 不变。
```

一次只执行用户明确授权的一个 09C 原子任务。每个任务先写失败测试，再实现最小改动并运行定向验证。

2026-07-24 用户明确授权 09C-03 并允许继续后续任务。09C-03 已完成两引擎 Raster 和外侧
光油物理离散；提前实现的 09C-04 Qt 控件与一键切片透传通过端到端 Gate；09C-05 已完成
Layer/Overlay Preview 物理比例显示和缺失元数据降级提示。

2026-07-24 已完成 09C-02：RIP Reader 和共享 RGBWSV writer 接受并严格校验合法非等方
DPI、独立物理像素字段及可选冗余数组；显式 600/600 保持兼容，bad package 使用稳定
`E_GRID_INVALID` 失败。当前下一原子任务为 09C-06 生产矩阵与阶段收口。
