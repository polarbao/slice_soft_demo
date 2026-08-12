# DOC_PREP_16C-02 Release 性能基线实施准备

> 状态：**PREPARATION COMPLETE / IMPLEMENTED**
> 日期：2026-08-12
> 对应任务：`16C-02`

## 1. 边界

本卡只刷新 Stage 14、LibTIFF 和 Stage 16 采样候选之后的 Release 基线，不实施缓存、扫描融合、
并行或默认策略切换。生产默认保持 S0；S3/S4 只作为已批准的诊断候选参与同机比较。

## 2. 固定矩阵

```text
策略：S0 / S3 / S4
场景：1 / 11 / 12 / 22 实例
Cold：每个策略/场景的第一个独立进程
Warm：一次预热后，五个独立进程；以操作系统文件缓存热态为口径
Core-only：场景实例 raster producer，不包含 compose/TIFF/RIP
End-to-end：import/layout/admission/raster/compose/package/RIP strict
统计：warm p50/p95；cold 实测；独立进程峰值 Working Set
确定性：排除含性能计时和绝对输出路径的 multimodel_scene_report 后计算 package hash
```

`core-only` 与 `end-to-end` 使用同一次真实生产执行中的两个受测边界，不额外运行另一套语义不同的
切片实现。功能 fixture 固定为 127 dpi / 0.2 mm，用于策略横向对照，不冒充目标设备 SLA。

## 3. 构建身份

报告必须记录 Git commit、dirty 状态、Release 配置、生成器/编译器身份、可执行文件路径及 SHA-256、
TIFF backend、PowerShell 和操作系统。报告生成时工作树可为 dirty，因为本卡需要先编译待提交实现；
对应源码提交和报告 SHA 在状态报告中补充。

## 4. 输出与 Gate

机器可读 schema 为 `slicesoft.stage16.release_baseline.1`。必须满足：

1. 12 个策略/场景汇总项完整；
2. 每项 cold 1 次、warm 至少 5 次；
3. p50/p95、峰值内存、确定性输出 hash 和 RIP strict 完整；
4. 同策略/场景输出 hash 零漂移；
5. `productionStatus=INPUT_OPEN`，不得用功能 fixture 关闭正式设备/SLA 输入。

## 5. 验证入口

```powershell
./scripts/run_stage16_release_baseline.ps1 -Mode full -Iterations 5
python tests/stage16/ValidateReleaseBaseline.py `
  output/benchmarks/stage16/16c_02/release_baseline.json
```

`-Mode quick` 只覆盖 1 实例，用于脚本和合同快速验证，不构成 16C-02 完整证据。
