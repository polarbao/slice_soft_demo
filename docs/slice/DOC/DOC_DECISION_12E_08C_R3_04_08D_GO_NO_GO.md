# DOC_DECISION_12E-08C-R3-04 12E-08D GO/NO-GO

> 决策状态：NO-GO / FROZEN
> 日期：2026-07-21

## 1. 决策

12E-08D 当前不得启动 production adapter 或统一 TIFF 写包接入。R3-03 任务证据已完成，但生产准入
Gate 未通过。

## 2. Gate 矩阵

| Gate | 结果 | 说明 |
|---|---|---|
| required case strict/post-strict | FAIL | 三个 OBJ confirmed self-intersection |
| provenance | PARTIAL | 3MF PASS；三个 OBJ 无 admitted candidate |
| global full chain | FAIL | 1 completed / 3 skipped_due_topology |
| Release budget frozen | FAIL | 真实 OBJ 无可代表 global core 数据 |
| legacy TIFF invariant | PASS | 30/30 层 SHA-256 一致 |
| RIP/protocol | PASS | p0.rgbwsv.2/RGBWSV/8-bit/black_is_print |
| repair/OpenVDB defaults | PASS | 均保持 OFF |
| Quick CI | FAIL / KNOWN | material_process_top2 48/226 baseline 未决 |
| global production output | NOT EXECUTED | 符合 R3 非生产边界 |

## 3. 解除 NO-GO 的必要条件

```text
1. 对三个 required OBJ 进行外部人工修复或受审计的独立重建；
2. 以原 required-case 身份登记新 source hash，重新跑完整 self-intersection 和 post-strict；
3. 四个 case 全部执行 global partition/texture/raster/full closure；
4. 冻结真实模型 Release 核心预算；
5. 解决或显式批准 material_process_top2 golden 基线变更；
6. 用户再次明确启动 12E-08D。
```

不得通过放宽 strict、跳过 OBJ、用 3MF 单例代表全部真实模型、自动 fallback legacy 或把 diagnostic TIFF
当 production TIFF 来解除 Gate。

## 4. 阶段状态

12E-08C-R1/R2/R3 已完成非生产证据闭环；R3-04 决策 COMPLETE / NO-GO。12E-08D 继续 BLOCKED。

## 5. 后续插入专项

R3-04 后新增 `12E-08C-R4 模型导入预检与修复资产准入`。R4 允许使用正常闭合模型继续正向功能和 UI
验证，同时保留三个 required OBJ 的真实 Gate 身份；只有修复资产审计、四 case 全链和 Release budget
全部通过，R4-08 才可重新输出 08D GO。
