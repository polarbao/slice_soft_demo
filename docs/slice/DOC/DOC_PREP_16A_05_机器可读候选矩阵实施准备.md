# DOC_PREP_16A-05 机器可读候选矩阵实施准备

> 状态：**PREPARATION GATE PASS / IMPLEMENTED**
> 日期：2026-08-12
> 对应任务：`16A-05`

## 1. 准备结论

16A-01 已冻结六组合成 fixture 和逐层/通道差异合同，16A-02..04 已提供 S0/S2/S3/S4
四种可执行策略。16A-05 只增加独立诊断工具，不修改采样算法、生产默认、配置解析或输出协议。

## 2. 矩阵合同

```text
策略：S0 Legacy；S2 Layer Slab + Pixel Center；S3 2x2 >=2/4；S4 2x2 >=1/4；
资产：合成 fixture、Reality 101..105、Stage 15 白区载体；
逐层：合成 fixture 保留逐层 RGBWSV/占用/连通分量差异；
真实资产：保留汇总差异，避免将约 20 MB 逐层明细提交到仓库；
尺寸：记录 X/Y/Z 像素和毫米偏差；
性能：记录 wall/core 分项和进程内存诊断；
协议：合成 fixture 与 Stage 15 四策略均写包并执行 RIP strict；
默认：S0 保持生产默认，矩阵不得自动选择候选。
```

峰值内存是同一进程的全局计数，不作为独立候选峰值。独立 Release p50/p95 和峰值内存属于
16C-02，本卡在报告中显式标注该边界。

## 3. 输出

```text
工具：stage16_sampling_matrix
schema：slicesoft.stage16.sampling_matrix.1
开发输出：output/benchmarks/stage16/sampling_matrix.json
归档证据：docs/slice/REPORT/assets/sampling_matrix.json
CTest：--quick，仅执行小 fixture 与四策略 Package/RIP gate
```

完整 Reality/Stage 15 矩阵显式运行 Release 工具；CTest 不重复执行耗时约三分钟的完整矩阵。

