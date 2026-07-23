# DOC_DECISION_12E-08C-R4-08-R1 受限生产候选准入规则

> 决策状态：ACCEPTED / 用户明确授权
> 首次生效时间：2026-07-23 11:32 +08:00
> 修订对象：R4-07 最终候选验证、R4-08 GO/NO-GO、12E-08D 启动 Gate
> 取代范围：只取代“必须先由爱神/玫瑰/梯田 3/3 才能开展生产候选验证”的单一前置条件

## 1. 背景

原 R4 最终 Gate 要求爱神、玫瑰、梯田三个指定真实模型族各有一个 strict/admitted 资产。当前
`model` 目录的完整审计证明，这三个模型族仍为 `0/3`，但 `xiao_ma_wu_yu_new` 与 `yecan`
两个独立模型族已经存在可重复的 strict PASS 资产，并完成 R4-06 intake 与 R4-07 四用例开发验证。

用户已授权放宽生产候选证据来源。放宽目标是允许已通过严格审计的真实模型继续验证生产候选链，
不是把任意单个简单模型直接声明为正式生产覆盖，也不是降低几何安全标准。

## 2. 决策

新增“受限生产候选 Gate”，启动条件由指定模型族 `3/3` 调整为：

```text
model 目录中至少有两个相互独立的真实模型族；
每个模型族至少有一个 R4-06 intake admitted 资产；
候选均通过完整 strict 审计；
confirmedIntersectionPairs=0；
coplanarOverlapPairs=0；
boundary/non-manifold/duplicate/opposite-duplicate/winding blocker=0；
资源、几何、属性和 audit hash 可重复；
候选必须覆盖真实 UV/材质/纹理，不以纯几何 generated fixture 代替；
productionOutputWritten=false，直到 12E-08D 获得独立授权。
```

当前满足候选身份：

```text
family=xiao_ma_wu_yu_new
  model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj

family=yecan
  model/obj/yecan/3.obj
```

`samples/models/3mf/texture2d_checker_cube.3mf` 继续作为 Texture2D 3MF 控制组，但不计入两个真实
模型族。

## 3. 准入层级

| 层级 | 条件 | 允许范围 | 不允许范围 |
|---|---|---|---|
| Development | 至少 1 个 admitted 模型 | diagnostic 开发与四用例 | 生产预算、生产写包 |
| Restricted Production Candidate | 至少 2 个独立 admitted 模型族，完整候选矩阵通过 | Release/closure/performance/legacy/RIP/CI 候选验证 | 未授权的 global TIFF 写包、全产品覆盖声明 |
| Production GO | 候选矩阵、预算、CI、协议、用户授权全部闭环 | 启动 12E-08D 原子任务 | 默认替换 legacy、静默回退 |

`Restricted Production Candidate` 只说明证据范围足以进入后续生产候选验证。它不等于
`productionAdmission=passed`。

## 4. 受限候选验证要求

两个独立模型族必须共同覆盖：

```text
minimum width；
intermediate width；
allTexture endpoint；
OBJ UV/纹理传递；
partition/raster/full material closure；
Release warm-up + 至少 3 次核心计时；
peak working set；
legacy repair-disabled TIFF invariant；
RIP Reader strict；
Quick CI 或有正式批准、隔离边界的既有 baseline 结论。
```

核心耗时不包含 TIFF/PNG/JSON 写盘。预算阈值必须独立冻结，不能直接把一次测量值当成正式上限。

## 5. 逐输入 fail-closed

放宽的是“证据模型族名称”，不是输入安全规则：

```text
每个导入模型仍必须执行 fresh preflight；
任一 strict blocker 仍阻断 global_surface_shell；
爱神/玫瑰/梯田现有失败资产继续 blocked；
不允许 global 失败后静默回退 legacy；
legacy 继续为默认生产模式；
未获准模型不得继承 xiao_ma/yecan 的 PASS。
```

原爱神/玫瑰/梯田 `0/3` 结论继续作为“复杂浮雕模型覆盖缺口”，必须在阶段报告中保留，不得改写为
已经通过。

## 6. 当前 Gate

| 条件 | 当前证据 | 状态 |
|---|---|---|
| 独立真实模型族至少 2 个 | xiao_ma + yecan | PASS |
| 每族至少 1 个 admitted | R4-06 development intake 2/2 | PASS |
| minimum/intermediate/allTexture | R4-07 development four-case | PASS |
| partition/texture/raster/full closure | R4-07 development four-case | PASS |
| Release 三次测量 | 已有开发测量，需按新口径刷新 | READY |
| 受限生产预算冻结 | 尚无正式阈值 | BLOCKED |
| legacy TIFF/RIP | 已有 PASS，需纳入刷新摘要 | READY |
| Quick CI | 既有 golden 差异未决 | BLOCKED |
| 12E-08D 明确授权 | 本决策仅授权准入规则与候选验证 | BLOCKED |

因此，本决策立即解锁 `R4-07-R1 受限生产候选验证`，但尚不解锁 12E-08D。

## 7. 对旧决策的影响

以下条款被本决策部分取代：

```text
爱神/玫瑰/梯田 3/3 是生产候选验证和生产预算测量的唯一启动条件。
```

以下条款继续有效：

```text
strict 不放宽；
完整自相交审计不可 sampled/incomplete；
属性和纹理资源必须可审计；
预算必须冻结；
Quick CI 必须 PASS 或有正式隔离决策；
12E-08D 必须取得独立用户授权；
协议、writer 和 legacy 默认行为不变。
```

## 8. 修订记录

| 时间 | 修改内容 | 原因 |
|---|---|---|
| 2026-07-23 11:32 +08:00 | 新增两独立真实模型族的受限生产候选 Gate | 用户授权采用推荐的多族准入方案，避免由任意单模型直接代表生产覆盖 |
