# DOC_PREP 12E-09C X/Y DPI 准备

> 状态：COMPLETE / 09C-01..06 PASS
> 日期：2026-07-24

## 1. 结论

X/Y DPI 不是单一 UI 控件修改。初始审计时 parser 和两套 raster 已能表达独立 DPI，但核心
validator、RIP Reader 和一键切片仍固定 600/600。09C-01..05 完成核心合同、Reader/writer
非等方兼容、两引擎 Raster、外侧光油、Qt 配置透传和物理比例 Preview；09C-06 已完成
Release 真实模型生产矩阵、RIP strict、Debug/Release 与回归收口。

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
| 09B session/Effective Config | COMPLETE / 09B-06 PASS |
| 09C-01 开发 | COMPLETE / 2026-07-24 |
| 09C-02 开发 | COMPLETE / 2026-07-24；Reader/writer 非等方 DPI、物理像素一致性与 bad package PASS |
| 09C-03 Raster/外侧光油 | COMPLETE / 2026-07-24；两引擎独立 X/Y pitch 与外侧光油离散 PASS |
| 09C-04 Qt 配置与一键切片 | COMPLETE / 2026-07-24；三窗口尺寸、保存回读和 Effective Config PASS |
| 09C-05 物理比例 Preview | COMPLETE / 2026-07-24；635/600 校正与方形降级提示 PASS |
| 09C-06 生产矩阵与收口 | COMPLETE / 2026-07-24；四 case Release、RIP strict、Debug/Release、schema/golden/Quick CI PASS |

## 6. 准备完整性审计

```text
需求范围：COMPLETE；
核心/Reader/两引擎/外侧光油/Qt/preview 影响面：COMPLETE；
PRD/DEV/DEMO/TASKS/PROMPT：COMPLETE；
旧 600/600 兼容与禁止批量改 fixture：FROZEN；
默认 X=635/Y=600：CONFIRMED；
开发前置：COMPLETE；
09C-01..06：COMPLETE；
下一正式路线：先补齐并执行 09A-02..06，再进入 12E-10A..D。
```

09C 已完成收口。09C-01/02 完成配置、Reader/writer 与协议严格校验；09C-03 完成
Legacy/Global 两引擎非等方 Raster 和外侧光油物理离散；09C-04 完成 Qt 配置、一键切片
透传及端到端 Gate；09C-05 完成 Layer/Overlay Preview 物理比例显示；09C-06 完成生产矩阵、
真实模型、Release、RIP strict、schema/golden/Quick CI 和文档收口。
