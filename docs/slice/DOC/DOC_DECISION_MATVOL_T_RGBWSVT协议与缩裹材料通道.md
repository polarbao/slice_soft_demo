# DOC_DECISION_MATVOL-T RGBWSVT 协议与缩裹材料通道

> 文档状态：**ACTIVE / EXPLICIT PRODUCTION OPT-IN ADMITTED**  
> 版本：v1.1 | 日期：2026-08-26  
> 基线：MATVOL MV-08C 已完成，旧生产默认仍为 `p0.rgbwsv.2`

## 1. 用户裁定

1. 开启 MATVOL-T 原型、方案、文档与后续开发。
2. 外部 RIP 对 T 通道的适配视为已完成；仓库内仍须保留可重复的合同验证。
3. 缩裹区域由材质级 RGB/Kd 配置识别，颜色不得硬编码在软件中。
4. `03/08/09` 中材质 `01` 是甲片，材质 `02` 是缩裹区域；此前相反推断作废。
5. 新工艺使用新版协议并写 T；旧工艺文件与 `p0.rgbwsv.2` 必须保留。
6. 后续开发在独立分支进行，原工艺新增新版副本，不原地覆盖旧文件。

## 2. 协议裁定

旧协议保持冻结：

```text
schema       = p0.rgbwsv.2
channelOrder = R G B W S V
channelCount = 6
```

新增协议：

```text
schema       = p0.rgbwsvt.1
channelOrder = R G B W S V T
channelCount = 7
bitDepth     = 8
polarity     = black_is_print
printValue   = 0
emptyValue   = 255
```

禁止把第七通道静默追加到 `p0.rgbwsv.2`。协议由工艺/Profile 显式选择，不得按模型内容静默切换。
新版工艺在未识别到缩裹区域时仍使用 `p0.rgbwsvt.1`，T 全空，其余六通道保持对应旧工艺语义。

## 3.0 术语统一：弹性材料 = 缩裹材料 = transfer = T 通道

> 用户 2026-08-31 澄清并授权统一。本节为该通道**材料语义的唯一权威定义**，
> 其余文档若与本节冲突，以本节为准。

```text
同一种物理材料，本仓库存在三个历史叫法，全部等价：
  弹性材料   产品/工艺侧叫法（材料本身的物理属性：可拉伸）
  缩裹材料   工艺侧叫法（该材料在甲片上的成型工艺：缩裹）
  transfer   代码侧标识（TransferChannelPolicyConfig / ResolveTransferMaterial 等）
通道字母 T 最初取自「弹性」拼音首字母（Tanxing）。
```

**裁定：保留通道字母 `T` 不变。** 理由三条：

```text
1  p0.rgbwsvt.1 已冻结：contracts/p0.rgbwsvt.1.schema.json 的 channelOrder
   以 {"const": "T"} 字面量固化，且 §1.2 记载外部 RIP 适配已完成。
   改字母等于毁弃已交付的外部契约，须重新协商并重跑外部验收。
2  T 在英文侧同样自洽：Transfer 的首字母亦为 T，
   与既有代码命名（transfer_channel_policy / TransferMaterialResolver）完全一致，
   故拼音来源与英文缩写在此处殊途同归，无需二选一。
3  若改按英文「弹性」取 Elastic/Flexible，首字母为 E/F，与既有 T 冲突；
   收益为零而代价极大。
```

**书写约定（新文档一律照此）：**

```text
通道字母      T（不得改写）
代码标识      transfer（不得改写，已冻结于配置键与类型名）
中文正式名    弹性材料（缩裹）    首次出现时括注，其后可简称「弹性材料」
不再单独使用  「缩裹材料」作为主名（保留为工艺别名，便于检索历史文档）
```

## 3. 区域语义

```text
material 01 = nail（甲片）
material 02 = transfer（弹性材料 / 缩裹）
```

名称仅是当前资产事实，不作为通用识别规则。正式识别顺序为：

```text
MaterialInfo.diffuse_rgb
  -> Profile 中 transfer.materialDiffuseRgbValues 精确匹配
  -> 匹配材质子网格拓扑 Gate
  -> MATVOL 有序交点/compact interval
  -> 单层 T mask
```

匹配的是导入后的材质级 `Kd/BaseMaterial`，不是纹理像素或最终 RGB。首版只允许精确 uint8 匹配；
允许多个颜色值作为显式别名，但不允许模糊距离、文件名特判或材质名 `02` 硬编码。

## 4. 合成规则

| 区域 | R/G/B/W/S/V | T |
|---|---|---|
| 缩裹 | 全部 `255` | 配置的打印值，首版为 `0` |
| 甲片 | 按所选旧工艺正常合成 | `255` |
| 支撑 | 沿用 S 语义 | `255` |
| 空白 | 全部 `255` | `255` |

T 与 R/G/B/W/S/V 对缩裹像素互斥。缩裹属于模型材料，沿用 Model > Support 优先级。
光油、白墨、全 RGB 等工艺只作用于甲片剩余区域，不覆盖 T。

## 5. 无缩裹与失败边界

- 未匹配到配置颜色：记录 `not_present`，T 全空，甲片沿用对应旧工艺语义。
- 匹配到颜色但拓扑不闭合/非流形/超预算自交：fail closed，禁止退回普通甲片。
- 多个材质同时匹配：首版 fail closed；后续若有真实多组件资产再单独授权。
- T mask 超出模型 occupancy：fail closed。
- 旧工艺/Profile：不得新增 T、不得改变 TIFF、manifest、Profile hash 或默认选择。

## 6. 当前资产事实

| 资产 | 材质 01（甲片） | 材质 02（缩裹） |
|---|---|---|
| `03.obj` | `[63,190,126]` | `[255,220,198]` |
| `08/09.obj` | `[63,190,126]` | 当前为 `[255,255,0]`，后续可由资产或配置统一 |

颜色值必须来自新版工艺配置。`08/09` 当前材质 02 在生产适配器口径下仍有 3 条开边和 8 对局部自交；
原型可完成颜色识别，但体积写 T 前仍须按本决策的拓扑 Gate 处理，RIP 已适配不等于几何已准入。

## 7. 架构边界

- Importer 只提供材质事实，不决定 T。
- transfer resolver 只识别材质角色，不写 TIFF。
- MATVOL 几何只生成 T owner/mask，不决定甲片工艺。
- layer composer 执行 T 排他和七通道交错。
- output writer 依据显式协议写 6 或 7 sample TIFF。
- UI 只展示有效配置、识别结果和已落盘 TIFF，不重算几何。

## 8. 生产 Gate

```text
G1  配置/角色解析与错误码
G2  T-only 几何 plan 与合成 fixture oracle
G3  03/08/09 真实资产识别及拓扑结果
G4  RGBWSVT stripped/tiled + none/packbits Writer/Reader
G5  旧协议与旧工艺逐字节零漂移
G6  新旧工艺双文件、Host/Profile hash 与持久化
G7  Package/内部 strict RIP/外部 RIP 合同
G8  取消、失败、无半包、内存与性能
G9  用户生产 opt-in 回签
```

### 8.1 T-09 准入裁定

- G1..G8 已由 T-01..T-08 的仓库内证据闭合；用户连续授权执行 T-09 计入 G9 回签。
- 生产准入只授予参考 Host/Worker 的显式 `slice.rgbwsvt`、minor=1、singleton Scene 路径。
- 准入包必须写 `productionAcceptance=admitted`；直接 CLI 仍写
  `rgbwsvt_candidate_unvalidated`，不得作为生产包分发。
- `host-reference-transfer-channel` 是显式、非默认生产 Profile；缺新版外部工艺时不可用，禁止回退旧协议。
- `03.obj` 是当前正例；`08/09.obj` 保持拓扑 fail closed，不开展修复且不计生产覆盖。
- 本裁定不代表设备、实物打印或现场 SLA 通过；外部 RIP 适配仅按用户输入接受。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-31 | v1.2 | MATOPQ 专项查证触发：新增 §3.0 术语统一，确认「弹性材料 = 缩裹材料 = transfer = T 通道」为同一物理材料；裁定保留通道字母 T（协议已冻结 + Transfer 英文首字母自洽 + 改名收益为零）；固定中文正式名为「弹性材料（缩裹）」。用户 2026-08-31 授权。 |
| 2026-08-26 | v1.1 | 完成 T-09 显式生产准入：限定 Host/Worker Scene admitted、direct CLI candidate、03 正例与 08/09 拒绝边界。 |
| 2026-08-25 | v1.0 | 冻结材质 01/02 角色更正、配置化颜色识别、`p0.rgbwsvt.1`、双协议双工艺和生产 Gate。 |
