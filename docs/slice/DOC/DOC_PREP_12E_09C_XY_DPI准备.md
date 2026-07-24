# DOC_PREP 12E-09C X/Y DPI 准备

> 状态：PREPARATION COMPLETE / WAIT 12E-09B-06
> 日期：2026-07-24

## 1. 结论

X/Y DPI 不是单一 UI 控件修改。当前 parser 和两套 raster 已能表达独立 DPI，但核心 validator、RIP
Reader 和一键切片仍固定 600/600。相关需求、设计、验证和原子任务已补齐。

## 2. 前置

```text
12E-09B-06 完成双模式 UI session、Effective Config 和生产 package 绑定；
保持 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
保留所有显式 600/600 fixture；
确认当前工作树不混入未收口的 UI 状态机改动。
```

## 3. 准备材料

```text
DOC_DECISION_12E_09C_XY_DPI非等方分辨率兼容.md
PRD_12E_09C_XY_DPI配置与生产协议兼容.md
DEV_12E_09C_XY_DPI配置Reader与UI设计.md
DEMO_12E_09C_XY_DPI验证方案.md
TASKS_12E_09C_XY_DPI任务清单.md
CODEX_PROMPT_12E_09C_XY_DPI执行指令.md
```

## 4. 当前不可执行内容

09B 尚未完成时，不修改：

```text
OutputConfig 默认；
固定 600 validator；
RIP Reader；
MainWindow 一键切片 DPI；
QuickConfigPanel；
golden/hash。
```

## 5. 启动 Gate

| Gate | 状态 |
|---|---|
| 用户目标 X=635/Y=600 | CONFIRMED |
| 影响面审计 | COMPLETE |
| PRD/DEV/DEMO/TASKS | COMPLETE |
| 09B session/Effective Config | WAIT 09B-06 |
| 09C-01 开发 | WAIT |

## 6. 准备完整性审计

```text
需求范围：COMPLETE；
核心/Reader/两引擎/外侧光油/Qt/preview 影响面：COMPLETE；
PRD/DEV/DEMO/TASKS/PROMPT：COMPLETE；
旧 600/600 兼容与禁止批量改 fixture：FROZEN；
默认 X=635/Y=600：CONFIRMED；
开发前置：仅剩 09B-06。
```

因此 09C 无需再补准备文档，但当前不得提前实现。09B-06 完成后从 09C-01 按顺序启动。
