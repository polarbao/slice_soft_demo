# DOC_REVIEW_12G-TCWS 现有 RIP 白区合同与六通道策略比对

> 文档状态：REVIEWED FACTS / DECISION PARTIALLY CLOSED / SPECIALTY REMAINS FROZEN
> 版本：v1.1 ｜ 日期：2026-08-03
> 适用范围：全 RGB 甲片、白色/透明意图、固定六通道 TIFF

> Stage 14 补充：当前阶段已明确不新增逐层 sidecar。对 RGB 黑哨兵的完整配置与贴图碰撞审计、
> 既有 `WSV=000` 兼容条件及 W-only Profile 候选，见
> `docs/slice/DOC/DOC_ANALYSIS_14_Q2_RIP白区带内信号与配置冲突审查.md`。本专项仍保持冻结。

## 1. 本轮确认事实

用户确认当前生产链存在以下行为：

```text
1. 同一组“全 RGB 切片”可以由不同 RIP 工艺生成透明模型或白色/不透明模型。
2. 当前 RIP 将正常 RGB 纹理区作为颜色数据，并使用 W/S/V=0/0/0 区域区分模型中的白色区域。
3. 白色/透明选择当前由 RIP Profile 决定，不希望为两种结果重复切片。
4. 本专项保持单 TIFF 六通道 R/G/B/W/S/V，不增加第七通道。
5. 本专项本轮不设计“纹理铺底层”或额外铺底层数。
```

以上是 RIP 业务事实，不自动等于 `p0.rgbwsv.2` 的物理通道合同已经允许
`WSV=000` 作为哨兵。

## 2. 关键协议冲突

SliceSoft 当前固定协议是：

```text
black_is_print；
0=打印；
255=不打印；
W=白墨；
S=支撑；
V=光油。
```

因此按当前生产协议逐通道解释：

```text
R/G/B/W/S/V = 255/255/255/0/0/0
```

表示同一像素打印白墨、支撑和光油，而不是“白色区域标记”。如果 RIP 实际把
`WSV=000` 拦截为私有 mask 并重新映射，它消费的就是一个 RIP-bound 中间合同，而不是
可脱离该 RIP 独立解释的物理 RGBWSV package。

在决定实现前必须确认：

```text
S=0 是否真的进入支撑喷印；
W/V=0 是否同时进入材料输出；
还是 RIP 在物化前把 WSV=000 识别为白区选择码并重写通道。
```

不得在没有样本证据时把三者混为一个结论。

## 3. 方案一：保留当前 RIP-managed `WSV=000`

### 3.1 工作方式

```text
普通纹理像素：RGB 表达颜色；
白色模型像素：RGB 为空白值，WSV=000 作为 RIP 私有白区 mask；
RIP Profile A：把白区物化为透明/光油；
RIP Profile B：把白区物化为白墨；
同一切片 TIFF 可复用。
```

### 3.2 优点

```text
同一切片包可在 RIP 阶段选择透明或不透明；
不需要重复模型加载、切片和 TIFF 生成；
贴近当前已存在的 RIP 工作流；
保持六通道文件数量和 channel count 不变。
```

### 3.3 缺点

```text
WSV=000 与 black_is_print 物理通道含义直接冲突；
S 同时承担支撑和白区标记，存在真实支撑重叠歧义；
当前 RIP Reader、材料闭环和像素探针会把它解释为三材料打印；
切片包不能脱离特定 RIP contract 独立审计；
错误 RIP Profile 可能产生错误材料而非显式失败；
需要版本化 RIP 合同、输入/输出证据和 fail-closed。
```

### 3.4 适用条件

只有满足以下条件才可继续：

```text
明确声明 packageClass=rip_bound_intermediate；
绑定 ripContractId/version；
RIP 在物化前不会把 S=0 当真实支撑；
RIP 输出有可机读六通道/作业报告；
pre-RIP Reader 不再把该 package 宣称为 self-contained production；
生产验收包含 RIP 后 effectiveProcessClosure。
```

## 4. 方案二：切片端显式 W/V

### 4.1 工作方式

```text
opaque white：W=0，V/S=255；
transparent white：V=0 或由已确认透明载体通道表达，W/S=255；
普通颜色：RGB 正常，W/V 按工艺 Profile；
背景：六通道全 255。
```

### 4.2 优点

```text
每个像素符合固定通道语义；
RIP Reader、材料闭环和像素探针可直接解释；
白墨、支撑、光油互不复用为哨兵；
package 可独立审计、回归和交付；
错误 Profile 更容易在切片阶段 fail closed。
```

### 4.3 缺点

```text
透明和不透明通常需要两份不同切片包；
工艺选择从 RIP 前移到切片 Profile；
同一 TIFF 无法仅靠 RIP 无损切换白色意图；
需要明确近白色阈值、Alpha 和 RGB/W/V 叠加规则。
```

## 5. 方案三：六通道 Hybrid Mask

### 5.1 候选方式

不新增通道，但在现有通道中选择一个“RIP 白色意图 mask”承载位：

```text
不得使用 S，因为 S 已有支撑物理职责；
候选优先使用 V，前提是当前透明产品本来就把该区域作为透明/光油载体；
透明 RIP：按 V 物理输出；
不透明 RIP：按受控合同把 V mask 重映射到 W；
普通 W/S/V 像素仍按物理通道输出。
```

### 5.2 优点

```text
保持同一六通道切片包；
避免 S 与白区 mask 冲突；
透明 Profile 下可让 pre-RIP package 具有一种自洽物理解释；
不需要增加 sidecar 像素通道。
```

### 5.3 缺点

```text
不透明 Profile 仍依赖 RIP 重映射；
V 同时是物理光油和白色意图来源，必须有来源 mask/合同区分；
若 V 已用于表面光油或外侧光油，可能无法无歧义复用；
仍需版本化 ripContractId 和 RIP 后验收；
是否适用于当前设备必须通过样件和 RIP 输出验证。
```

## 6. 对比结论

| 维度 | 当前 WSV=000 | 切片显式 W/V | Hybrid V mask |
|---|---|---|---|
| 同包透明/白色切换 | 强 | 弱 | 强 |
| 固定物理通道自解释 | 差 | 最好 | 中 |
| 与真实 S 支撑冲突 | 高 | 无 | 低 |
| RIP 耦合 | 高 | 低 | 中高 |
| 当前工作流迁移成本 | 低 | 高 | 中 |
| pre-RIP closure | 冲突 | 可 PASS | 需分模式 |
| 六通道保持 | 是 | 是 | 是 |

推荐决策顺序：

```text
第一候选：验证 Hybrid V mask 是否能在不冲突 Surface/Outer V 的情况下复用同包；
第二候选：若 V 无法区分来源，使用切片显式 W/V，接受两份 package；
不推荐：继续把 W/S/V 三个物理通道同时置 0 作为无版本私有哨兵。
```

这只是推荐，不是已冻结实施决策。必须先审计真实 RIP 输入和输出。

## 7. 本专项明确排除

本轮不讨论：

```text
纹理铺底层；
额外的 W/V underbase 层数；
新增第七通道；
sidecar 像素图；
TIFF 压缩或 LibTIFF；
RIP 半色调算法；
支撑铺底 13G。
```

允许讨论的是“现有六通道内，白色意图和透明/不透明 Profile 如何形成可审计合同”。

## 8. 仍需证据

专项继续冻结，直到取得：

```text
1. 一份包含普通 RGB、纯白、真实 S、真实 V 的最小输入 TIFF。
2. 透明 RIP Profile 的物化输出/通道统计。
3. 不透明 RIP Profile 的物化输出/通道统计。
4. WSV=000 在两个 Profile 中的逐像素映射表。
5. S=0 是否被消费为支撑的明确答案。
6. V mask 与 SurfaceVarnish/OuterVarnish 冲突规则。
7. 纯白/近白/Alpha 的识别来源和阈值。
8. RIP contract owner、版本、错误回退和验收产物。
```

## 9. 当前状态

```text
同包透明/白色：CONFIRMED PRODUCT INTENT；
六通道固定：CONFIRMED；
纹理铺底层：OUT OF SCOPE；
WSV=000 的物理/哨兵身份：UNCONFIRMED；
最终 A/B/C 方案：UNDECIDED；
12G-TCWS：FROZEN / NO CODE AUTHORIZATION。
```

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-07-31 | v1.0 | 首版。记录既有 RIP `WSV=000` 业务事实、固定六通道冲突和三条候选路径 |
| 2026-08-03 | v1.1 | 同步 Stage 14 Q2 约束：当前阶段不新增逐层 sidecar；链接完整配置/贴图碰撞审计；12G 仍冻结，不授权代码实现 |
