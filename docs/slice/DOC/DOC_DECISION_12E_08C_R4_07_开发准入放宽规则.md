# DOC_DECISION_12E-08C-R4-07 开发准入放宽规则

> 决策状态：ACCEPTED / 用户明确授权  
> 日期：2026-07-22  
> 修订对象：R4-06 Development Intake、R4-07 Four-case Development Gate
> 后续扩展：2026-07-23 11:32 +08:00，受限生产候选 Gate 由
> `DOC_DECISION_12E_08C_R4_08_R1_受限生产候选准入规则.md` 定义

## 1. 决策

R4-07 不再要求爱神、玫瑰、梯田三个 required family 全部 `3/3 admitted` 后才能开始开发。新的开发启动
条件为：

```text
model 目录中至少存在一个模型；
该模型通过 R4-06 intake；
admitted=true；
完整审计 complete；
strict PASS；
confirmed/coplanar=0；
资源、几何、属性和 audit hash 可重复；
productionOutputWritten=false。
```

满足上述条件即设置 `r4_07DevelopmentAllowed=true`，允许实现和运行 R4-07 diagnostic four-case matrix。

## 2. 双 Gate 定义

| Gate | 条件 | 解锁范围 |
|---|---|---|
| Development Gate | `development_model_pool` 至少 1 个 admitted 模型 | R4-07 代码、脚本、四 case diagnostic、开发性能基线、legacy 回归 |
| Final Required-family Gate | 爱神、玫瑰、梯田各 1 个 admitted，矩阵 3/3 | 最终真实族 Release Gate、预算冻结、R4-08 与 12E-08D 评审 |

开发 Gate PASS 不能写成 required-family PASS，也不能将 clean control 的性能阈值冻结为生产预算。

## 3. 当前证据

`model` 目录已有两个跟踪资产通过 R4-06 development intake：

```text
development_xiao_ma_damuzhi -> admitted；
development_yecan_3 -> admitted；
admittedDevelopmentCandidateCount=2；
r4_07DevelopmentAllowed=true。
```

因此 R4-07 开发 Gate 已解锁。爱神、玫瑰、梯田仍为 `0/3`，只阻断最终验收和生产路径。

## 4. 安全边界

```text
不放宽 strict；
不接受 incomplete/sampled 自相交审计；
不修改或覆盖 model 原始资产；
不把 development_model_pool 计入 required family 3/3；
不冻结生产预算；
不写 global production TIFF/package；
不修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
不允许 global silent fallback 到 legacy。
```

## 5. 被替代条款

此前文档中“required family matrix 少于 3/3 时不得开始 R4-07 任何开发”的条款被本决策替代。
“required family 3/3 才能完成最终 R4C、启动 R4-08/08D”的条款作为 2026-07-22 历史判断保留。
2026-07-23 起，至少两个独立 strict/admitted 真实模型族可以进入受限生产候选验证；是否启动 08D
仍由预算、CI、协议证据和用户独立授权共同决定。
