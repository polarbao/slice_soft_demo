# DOC_DECISION_MATOPQ 材质不透明度识别与光油通道映射专项

> 文档状态：**ACTIVE / G-01 已授权解除（方案 A）/ MO-01..03 收口验证中
> / MO-04 PREPARED（P1 已回签，新增 K1-K3 前置核查）**
> 版本：v1.0 ｜ 日期：2026-08-31
> 定位：不占 Stage 编号的独立材质外观专项；任务状态唯一真源为
> `docs/codex_task/current/TASKS_MATOPQ_材质不透明度识别与光油通道映射专项任务清单.md`
> 触发：用户报告 `model/obj/multi-material/fenandtou.obj` 中下部透明材质区域在
> MeshLab 与本软件均未显示为透明；经排查为外观管线丢弃 MTL 不透明度所致。

---

## 1. 问题类型与所属层

```text
问题类型  外观保真缺陷（MO-01..03）+ 工艺通道语义扩展（MO-04）
涉及层    importers/mtl -> model DTO -> api/viewdata -> slicer_module 适配 -> 宿主 CPU 光栅
不涉及    p0.rgbwsv.2 协议、RGBWSV 通道序、uint8、black_is_print、切片几何采样
```

## 2. 资产事实（A 级，本次实测）

被测资产 `model/obj/multi-material/fenandtou.obj`（Rhino 导出，2.42 MB）：

| 材质段 | 面数 | 有向体积 | 焊接后边界边 | MTL `d` |
|---|---|---|---|---|
| `touming` | 8355（5222 三角 + 3133 四边） | 67.4001 mm³ | **0（闭合）** | **0.4200** |
| `sg (1)` | 5250 | 22.9144 mm³ | **0（闭合）** | 1.0000 |
| `Default` ×3 | 各 8 | 各 0.0004 mm³ | 24（散面） | 1.0000 |

关键几何结论：

```text
两个主体各自闭合；共享 741 个坐标逐位重合的界面面，
其中 738 个绕序相反 -> 体积互不重叠，是一次干净的二材质实体剖分。
因此按材质做体积通道归属在几何上是良定义的，无需 open_surface 兜底或 overlap 仲裁。
```

附带缺陷（与本专项主线无关，但会干扰验证）：

```text
3 个 Default 散面位于 (±110, -17.9/42.0, -2.1)，不存在于 Rhino 原始导出 fenandtou.objbak，
系导出后追加。它们把 bbox 由 12.38 x 23.97 x 4.65 mm 撑到 220 x 60 x 14.19 mm，
会破坏 autoOrient.maxHeightMm 判定。测试须使用已剔除该三片的 _clean 变体。
```

## 3. 当前代码现实（A 级，逐处已验证）

### 3.1 已经具备、无需新建的部分

```text
契约层  DOC_SCHEMA_14 v1.5 已冻结 baseColorFactor[4]、alphaMode、alphaCutoff、doubleSided
适配层  SceneViewDataAdapter.cpp:190  const bool blended = material.base_color.at(3U) < 1.0F;
        SceneViewDataAdapter.cpp:192  {"alphaMode", blended ? "blend" : "opaque"}
策略层  SceneRenderPolicyData.cpp:195-203 已读 alphaMode/alphaCutoff/doubleSided 与 4 分量 baseColorFactor
后端层  CpuRasterBackend.cpp:137-148 已把 4 分量 baseColor 与 alphaMode 拷入 MaterialResource
采样层  CpuRasterizer.cpp:124 Sample() 已返回 4 分量；BlendPixel() 已做 src-over 混合
工艺层  MaterialRole::Varnish 已直写 V 通道（slicer.cpp:3068 pixels.at(base+5U)=0）
        ResolveModelFillMaterial 已把 Varnish 角色映射到 ModelFillMaterial::Varnish（slicer.cpp:2805）
```

**结论：`ViewMaterial` 不需要新增 `alpha_mode` 字段。** `alphaMode` 由适配层从
`base_color[3]` 自动推导，契约字段早已冻结在案。因此 MO-01..03 属于
**补齐既有冻结契约的实现缺口**，不是契约变更，不触发 major/minor 版本协商，
不新增 SPI 导出符号，无需重跑 Stage 14 外部验收。

### 3.2 真实缺口（仅 4 处）

| 编号 | 位置 | 现状 | 后果 |
|---|---|---|---|
| GAP-1 | `model.cpp:1721` `load_mtl` | 只识别 `newmtl`/`Kd`/`map_Kd` | `d 0.4200` 在解析首行即丢弃 |
| GAP-2 | `model.h:44` `MaterialInfo` | 只有 `diffuse_rgb[3]` + 纹理字段 | 即使解析出也无处承载 |
| GAP-3 | `SceneViewAssetResolver.cpp:210` | 循环为 `channel < 3U` | `base_color[3]` 恒为 1.0，适配层永远判 opaque |
| GAP-4 | `CpuRasterizer.cpp:210-244` | 混合片元照样写深度；全链无 back-to-front 排序 | 即使 alpha 到位，透明面仍遮挡其后内容 |

### 3.3 附带缺陷（同批修复，非新增语义）

```text
GAP-5  newmtl / usemtl 均用 stream >> name 只取首 token，
       "newmtl sg (1)" 被截断为 "sg"。已在本次实测的生产报告中复现：
       material_role_mapping_report.json 内材质名为 "sg"，"(1)" 已丢失。
       两侧一致故当前碰巧仍能匹配，但 "sg (1)" 与 "sg (2)" 会塌成同一材质，
       且与 MeshLab / assimp 取整行的行为不一致。
```

## 4. 冻结边界（本专项不得触碰）

```text
p0.rgbwsv.2 / channelOrder R G B W S V / uint8 / black_is_print / printValue 0 / emptyValue 255 不变
Model > Support > Empty 优先级不变
Legacy 仍为默认切片路径；不启用 OpenVDB；不改 slicingMode 默认值
不新增 SPI 导出符号；不改 PM_SPI_VERSION
不放宽 config.cpp 内任何既有 fail-closed 互斥门（见 §6）
slicer_core 不得引入 QString/QList/QObject/QWidget
G2 行数门禁：model.cpp(1982)、slicer.cpp(5635)、config.cpp(1442) 只减不增
```

## 5. MO-01..03 目标状态（授权实施）

```text
MTL 的 d / Tr 被解析为单一 opacity 语义（Tr = 1 - d），二者矛盾时 fail-closed；
MaterialInfo 承载 opacity 与 has_opacity；
ViewData base_color 填满 4 分量，opacity < 1 的材质由既有适配逻辑自动判为 blend；
CPU 光栅先画不透明片元，再按视深 back-to-front 画透明片元且不写深度缓冲；
预览 alpha 设下限钳制，保证 d=0 的区域仍可见、可选中（不按字面渲染为完全不可见）。
```

## 6. MO-04 路线裁定：走 MATVOL，不走 materialRoleMapping

本次实测（当前二进制 `build-slicesoft/main/Release`，与源码同期）：

```text
A  materialRoleMapping{touming -> varnish} + modelFill  => 成功
   V 通道着墨 752,237 px，占模型像素 74.63%
   touming 体积占比 67.4001/(67.4001+22.9144) = 74.63%  <- 完全吻合
   sg 区 RGB 写入 (255,220,198)，正是其 Kd

B  materialRoleMapping + texture.unprintableWhitePolicy=white_underbase  => 失败
   slicer_cli error: texture.unprintableWhitePolicy=white_underbase
                     does not support materialRoleMapping.enabled=true
```

`white_underbase` 正是「按需补白墨」的实现机制，因此 **roleMapping 路线与
现行「全实体 RGB + 按需补白墨」工艺互斥**。同类互斥另有两条：
`materialPolicy.enabled`（config.cpp:1296）、`materialVolumePolicy.enabled`（config.cpp:1298）。

而 `config.cpp:985` 存在一条已获授权的窄放行，注释原文：

> `MV-06 窄放行：MATVOL 自带最终 RGB，不依赖纹理顶面投影路径。`
> `其余白区禁令（Global 管线、materialPolicy、旧 roleMapping、whiteValue 冲突、OpenVDB）保持不变。`

**裁定：MO-04 建在 `materialVolumePolicy`（MATVOL）路径上。** 理由三条：

```text
1  只有 MATVOL 有与 white_underbase 共存的既有授权，roleMapping 被点名排除（措辞为「旧 roleMapping」）；
2  roleMapping 在 relief 模式下是逐 XY 列取顶面材质（slicer.cpp:2528），
   本资产能成立仅因两实体在 XY 上左右并置；一旦出现「光油在彩色层下方、同列叠放」的设计即无法表达；
   MATVOL 是 closed_intervals 真三维区间求解，无此限制；
3  MATVOL 自带 open_surface / overlap / topology / missing_material=fail_closed 四套护栏，
   正好覆盖 §7 的判据前置条件。
```

**明确禁止**：不得以放宽 `config.cpp:1003` 的方式让 roleMapping 与 white_underbase 共存。
该处是门禁承诺，放宽须先出授权文档留痕。

## 7. MO-04 判据口径（设计已定，实施待输入）

用户已回签的工艺语义：

```text
Q1 4.6mm 整块清漆可固化                      -> 已确认可固化
Q2 V 通道墨量足够做体积填充，不足会有墨量提醒  -> 已确认足够
Q3 与 outer_varnish 冲突                     -> outer_varnish 在该层上继续增加（叠加，非互斥）
Q4 「全透明」是否等于光油                     -> 是，光油
```

据此，判据可自动推导，**不需要强制人工确认**（此点相对首轮建议已修正），
但必须满足 4 个条件：

```text
C1【核心】不得用 == 0，必须用 Profile 配置的容差 opacityMax（建议 0.001）做 d <= eps。
    这是识别逻辑的【全部依据】。实测区分点仅有两个 d 值：
      fenandtou.mtl  d 0.4200  -> 半透明，按 Q4 语义【不是】光油
      tm2/tm3.mtl    d 0.0000  -> 全透明，是光油
    硬编码 == 0 在浮点文本解析下不可靠，必须走容差。

C2【降级为兜底】d 与 Tr 归一（Tr = 1 - d）及其矛盾检测。
    ⚠ 事实更正（用户 2026-09-01 质疑后核查）：
      全仓 65 个 MTL 中含 d 的 47 个，含 Tr 的【仅 2 个，且均为本专项自建的测试变体】
      （d0-varnish-test/fenandtou_d0{,_clean}.mtl，Tr 是生成时人为写入的）。
      真实资产（Rhino 导出）中 Tr 出现次数为 0。
    因此 Tr 分支与矛盾检测在当前资产上【永不触发】。
    首版把它列为首要条件属【权重错配】——它解决的是当前不存在的问题。
    保留解析代码的理由仅为跨导出器健壮性（Blender / assimp 系会写 Tr），
    成本几行且无害，但不得再作为识别判据的主依据陈述。
C3  该材质必须拥有闭合实体，否则 fail-closed。光油是体积填充，开放曲面无法填充。
    沿用既有 MaterialVolumeOpenSurfaceConfig.mode = "reject" 口径。
C4  0 < d < 1 必须有显式政策并出诊断（「半透明材质 X 未映射工艺通道，按 RGB 处理」）。
    半透明按 Q4 语义不是光油；不得静默丢弃设计意图——那正是本次原始缺陷的根因。
```

## 8. Pending Confirmation

### 8.1 P1 弹性材料通道归属【已回签，2026-08-31】

```text
结论  弹性材料 = 缩裹材料 = transfer = T 通道，同一物理材料的三个叫法。
      通道字母 T 取自「弹性」拼音首字母；Transfer 英文首字母亦为 T，两侧自洽。
裁定  保留通道字母 T 不变；术语权威定义已写入
      DOC_DECISION_MATVOL_T_RGBWSVT协议与缩裹材料通道.md §3.0（v1.2）。
```

**对 MO-04 的实质影响（相对首版设计的修正）：**

```text
首版假设  「全实体 RGB + 按需补白墨 + 弹性材料」跑在 p0.rgbwsv.2 上。
          该假设【错误】。
实际      弹性材料即 T 通道，故该套工艺本身就跑在 p0.rgbwsvt.1（7 通道）上。
后果      MO-04 的光油（V）判据必须与 T 通道【共存于同一包协议】，
          而不是我首版设想的 6 通道协议。
          即目标协议为 p0.rgbwsvt.1，channelOrder = R G B W S V T，
          V 承载光油体积填充、T 承载弹性材料，二者并列不互斥。
```

**K1/K2 实测结论（2026-09-01，Release 二进制与源码同期）：**

```text
K1 已澄清（读码确定，无需实测）
   transfer 的 matchSource 被硬校验锁死为 material_diffuse_rgb
   （TransferChannelConfig.cpp:139）。故：
     弹性材料（T）  按 Kd 颜色识别   -> transfer_channel_policy，既有机制
     光油（V）      按不透明度识别   -> MO-04 新增判据，走 MATVOL
   两者是【两套独立机制】，不争用同一个 matchSource，可以共存。
   不需要扩展 transfer 的 matchSource，首版设想的改动面不成立。

K2 已实测：配置层【放行】三者共存
   证据一 仓库既有样例已把 transfer + white_underbase 组合在一起：
          samples/configs/matvol_t/process_profiles/obj_mtl_texture_rgb_white_ondemand_rgbwsvt.json
          （packageProtocol=p0.rgbwsvt.1、transfer.enabled=true、
            unprintableWhitePolicy=white_underbase）
   证据二 在其上追加 materialVolumePolicy.enabled=true 实测，
          【未出现任何 does not support 类互斥报错】，
          失败发生在几何准入层而非配置层。

K2 附带发现【重要，影响 MO-04 资产准入】
   报错  E_MATVOL_T_TOPOLOGY_INVALID: transfer material 'sg' was rejected:
         E_MATVOL_OPEN_SURFACE_REQUIRES_POLICY
   根因  MaterialVolumePlan.cpp:150 对 OpenSurface 分类【无条件抛出】，
         不读 openSurface.mode。即 openSurface.mode="surface_band" 在代码中【未实现】，
         与 MATVOL 任务卡「MV-04 DESIGNED / 触发资产待定」的记载一致。
   实测  maxBoundaryEdges 放宽到 100000 无效；openSurface.mode 改 surface_band 亦无效。
   结论  MO-04 走 MATVOL 时，资产必须【逐材质闭合】，否则 fail-closed，无旁路。

K2 未解点已定位（2026-09-01）

   排除过程（逐一实测排除，不是推断）：
     × 顶点未焊接      —— MaterialTopologyClassifier.h 明确会做全网格焊接
     × 焊接容差差异    —— 用 MATVOL 默认 1e-6mm 与本专项先前的 1e-4mm 分别重算，
                          touming/sg 两种容差下【均为闭合，边界边 0】
     × autoOrient 变换 —— 禁用后重测，报错完全相同
     × maxBoundaryEdges 不足 —— 放宽到 100000 无效
     × openSurface.mode=surface_band —— 配置无效，原因见下

   根因  TransferMaterialVolumePlan.cpp:32-41 中 transfer 路径【自行构造】
         MaterialVolumePolicyConfig，其中 open_surface.mode 被【硬编码为 "reject"】，
         只有 topology 一项从用户 policy 继承。
         故用户侧 materialVolumePolicy.openSurface 的任何设置对 transfer 路径【无效】。

   判定链  MaterialTopologyClassifier.cpp:190
           boundaryEdgeCount > 0 || materialInterfaceEdgeCount > 0  -> OpenSurface
           而 MaterialVolumePlan.cpp:150 对 OpenSurface 无条件抛出。
           注释原文已写明：「仅由材质交界边围成的子网格同样【不是】独立闭合体：
           垂直射线在它上面拿不到成对交点。归入 OpenSurface 以保持 fail-closed，
           两类边数分别保留，供 MV-04 区分『源模型真开放』与『材质交界导致开放』。」

   影响  这是【设计如此的 fail-closed】，不是缺陷。其直接后果是：
         凡与相邻材质【共享界面】的子网格都会被判 OpenSurface，
         而处理该情形的 MV-04（surface_band）在 MATVOL 任务卡中状态为
         「DESIGNED / 触发资产待定」即【未实现】，代码中无条件抛出，无旁路。

   结论  MO-04 无法按原计划在 fenandtou 这类【共享界面剖分】资产上走
         transfer + MATVOL 路径。须先解决下列之一，且【均不属 MATOPQ 专项范围】：
           选项 A  由 MATVOL-T 专项实现 MV-04（surface_band），并让 transfer 路径
                   停止硬编码 open_surface.mode、改为从用户配置继承；
           选项 B  改用【各材质独立封闭且不共面】的资产建模方式
                   （MATVOL 现有成功案例 03.obj 即属此类，故其未触发 MV-04）；
           选项 C  MO-04 的光油判据不走 MATVOL 体积路径，另择载体（须重做路线裁定）。
         三者均需用户裁决，MO-04 在此之前保持 PREPARED。
```

**其余前置核查：**

```text
K3  V 与 T 的同像素冲突【用户 2026-08-31 已裁定，分层裁决】：
    L1 邻域无票 -> 非冲突，保留原归属；L2 票数不等 -> 多数胜；
    L3 平局 -> 距离加权重投（正交 1.0 / 对角 0.7071）；
    L4 加权仍平 -> 固定兜底优先级，【不 fail】。
    更新方式必须双缓冲（读旧写新），原地更新会使结果依赖扫描方向，Golden 必漂。
    L4 兜底优先级 = 【V 优先】（用户 2026-09-01 裁定），K3 七项参数已齐，不再阻塞。
    完整口径见 DOC_POLICY_INDEX_冲突裁决与工艺逻辑策略总表.md §1.6。

    修正记录：首版建议「平局即 fail-closed」，经用户 2026-08-31 追问后确认
    【该建议不可进入生产】——3x3 下 0:0 票型在模型内部极常见，
    与真冲突同等 fail 会让每次切片都在模型内部大面积失败。
    fail-closed 已上移到配置层（未声明 L4 优先级则拒绝加载配置）。
```

### 8.2 P2 判据参数（仍未定）

```text
opacityMax 容差取值与 0<d<1 区间的落位角色，须写入工艺 Profile 而非代码常量。
```

## 8.3 资产语义更正：tm2-x 系列【无缩裹区域】（用户 2026-09-01 指正）

```text
事实   model/obj/multi-material/tm2-x 系列为【多图层叠加】资产，无缩裹区域。
       每个图层含两种素材：
         透明素材（d 0.0000）    -> 光油，走 MO-04 的不透明度判据落 V 通道
         不透明素材（d 1.0000）  -> RGB，可带 map_Kd 贴图
       不同图层的不透明素材【允许 Kd 相同】，这是正常形态而非缺陷。
正确工艺  transferChannelPolicy【不启用】；协议为 p0.rgbwsv.2（6 通道）。
```

**实施方此前的误解及其根源（留痕，避免复发）：**

```text
误解   曾假设该系列存在缩裹区域，并把 sg 的 Kd 配入
       transferChannelPolicy.materialDiffuseRgbValues。

根源一 把【工艺档位名称】当成了【资产事实】。
       用户描述工艺为「全实体RGB + 按需补白墨 + 缩裹材质」，
       实施方直接取用同名样例 obj_mtl_texture_rgb_white_ondemand_rgbwsvt.json，
       而该样例默认启用 transfer；未区分「软件的工艺档位」与「该资产含缩裹材质」。

根源二 自行制造证据。发现 T 通道原型配置的 materialDiffuseRgbValues 恰为
       [[255,220,198]] 与 sg 的 Kd 相同，据此认定两者「同族」。
       那只是肤色巧合，被误当作资产语义证据。

后果   此后全部 E_MATVOL_T_MATCH_AMBIGUOUS 均由误启用 transfer 造成，
       并非资产缺陷。用户为此连续改出 tm2-1/tm2-2/tm2-3 三版资产，
       该三轮改动本不必要。

结论   MATCH_AMBIGUOUS【无需修改】——不启用 transfer 即不会发生。
       若将来确有缩裹区域且多图层各有一份，才需要 MATVOL-T 专项
       把 ResolveTransferMaterial 改造为支持多材质匹配（当前 multipleMatches
       只允许 fail_closed，且要求恰好命中 1 个材质）。

教训   涉及资产语义（哪个素材是什么材料）时，必须由用户明示或由文件内证据支持，
       不得从工艺名称、样例配置或颜色巧合推断。
```

## 8.4 光油区白墨底（已回签）

```text
裁定   光油区【不需要白墨底】（用户 2026-09-01）。
实现   compose_layer 的光油 pass 在清 RGB、写 V 的同时清 W。
顺序   该 pass 仍位于白墨载体之后——补白需先按最终 RGB 判过一轮，
       光油区那部分 W 属误加，在此撤除。
```

## 9. 风险点

```text
R1  G2 行数门禁：GAP-1 位于 model.cpp(1982 行，只减不增)。
    实施须把 load_mtl 下沉到新文件，model.cpp 净行数不得增加。
    落地位置取 src/slicer_core/model/MtlMaterialParser.{h,cpp}，对齐既有 ObjFaceParser 先例
    （同为 model.cpp 私有解析助手，命名空间 slicer_core::model_detail）。
    这与项目 R0/R1/R2 的 wrap first / move later 原则一致，且顺带减少 legacy 债。
R2  GAP-4 改动 CpuRasterizer 的深度语义，可能影响既有不透明渲染结果。
    须以「opaque 路径逐像素不变」为验收口径，而非仅看新功能可用。
R3  预览 alpha 下限钳制属显示策略，不得反向写回 MaterialInfo.opacity，
    否则会污染 MO-04 的工艺判据来源。
R4  GAP-5 修复会改变材质名（"sg" -> "sg (1)"），
    进而改变 material_role_mapping / obj_mtl_material 报告内容与任何按名匹配的既有配置。
    须作为独立卡并单独评估 Golden 漂移，不与 MO-01 混提。
```

## 9.1 实施期追加的不变性证据（A 级，本次实测）

实施 MO-03 时对「透明 pass 是否会扰动既有不透明渲染」做了三项定点核查：

```text
1  全仓检索 tests 与 apps/slicer_ui_host_sim 下的 json/cpp，
   除新增代码本身外【无任何既有 fixture 或用例把材质 alphaMode 设为 "blend"】。
   故透明 pass 对既有回归而言是不可达分支，不变性由构造保证，而非仅靠测量。
2  SceneRenderPolicyData.cpp:265 与 TopViewRenderPolicyData.cpp:170 的
   alphaMode == "straight" 校验作用于 textures[] 而非 materials[]；
   材质 alphaMode 在 SceneRenderPolicyData.cpp:195 只做直通、无枚举校验，
   故 ViewData 推导出的 "blend" 不会被策略层拒绝。
3  drawCallCount 仍按 submesh 计数一次，与分趟无关；
   无 blend 材质时两趟合计的计数与改前相同。
```

`load_mtl` 下沉后的行为等价性逐项核对：

```text
newmtl   原 stream >> name  -> 现 istringstream{arguments} >> name，同取首 token，
         "sg (1)" 的截断行为被【刻意保留】（MO-06 单列，不在本批修）
Kd       原 stream >> r >> g >> b -> 现于 arguments 上同序解析
map_Kd   原 getline(stream, rest) + trim_copy -> 现 arguments 已是同一 getline 结果再 TrimCopy
空行/注释行  原无分支命中 -> 现 token 为空，ApplyMtlMaterialLine 回报未识别，同为无副作用
未知关键字   原无分支命中 -> 现同为未识别，无副作用
```

## 10. 验证口径

```text
构建   cmake --build build-slicesoft/main --config Debug（先确认退出码为 0）
回归   ctest --test-dir build-slicesoft/main -C Debug --output-on-failure（基线 213 项）
不变性 opaque 资产逐像素零漂移；本专项不改切片输出，故 TIFF 通道统计须与改前逐字节一致
新功能 fenandtou_d0_clean 在宿主渲染中 touming 区呈半透明且可选中
行数门禁 python scripts/ValidateSourceSizeGuard.py --base-ref <分叉点>
```

## 12. G-01 候选方案（Stage 14B 冻结合同拒绝 model.cpp 新增依赖）

### 12.1 事实

```text
tests/contracts/ValidateStage14BLayeringFeasibility.py:158-167 有一条【字面精确列表】断言：
  expectedModelIncludes = ["src/slicer_core/model.h",
                           "src/slicer_core/model/ObjFaceParser.h"]
  if modelIncludes != expectedModelIncludes: raise AssertionError(...)
model.cpp 的项目内 include 被冻结为恰好这两项，多一项即失败。
```

用该合同自身的 `AssignLayer()` 实测：新增的 `MtlMaterialParser.{h,cpp}` 与
`ObjFaceParser.h`、`model.cpp` 同为 `base` 层（`BASE_PREFIXES` 含
`src/slicer_core/model/`），**未产生任何 base -> engine 边**。
合同同名所护的「分层可行性」属性未被削弱；失败仅来自那条更严格的字面清单冻结。

### 12.2 候选

```text
方案 A（推荐）  把 expectedModelIncludes 扩为 3 项，纳入 MtlMaterialParser.h
  改动面   该测试脚本 1 处清单
  代价     需授权 + 留痕（放宽冻结面）
  论据     新头与既放行的 ObjFaceParser.h 同层、同目录、同命名空间、同角色；
           且本次 model.cpp 实际【缩减】41 行（1982 -> 1941），
           是该合同所期望的抽取方向，而非依赖累积

方案 B  撤销抽取，d/Tr 解析写回 model.cpp 内联
  改动面   不动任何合同
  代价     与 G2 行数门禁冲突：model.cpp 回到 1982 行后再加约 10 行即 1992 > 1982，
           必须另删同等行数。要么压缩无关代码（无谓 churn），
           要么把 model.cpp 顶在门禁上限。且保留 1982 行 legacy 债，
           与项目 wrap first / move later 原则相反

方案 C  把 MTL 解析塞进已放行的 ObjFaceParser 翻译单元
  改动面   不动任何合同、不动行数门禁
  代价     把 MTL 材质解析放进名为「ObjFaceParser」的文件，
           命名与内容不符，属技术性绕过而非设计选择。【不推荐】

方案 D  暂停 MO-01，本批只交付不依赖 model.cpp 的部分
  代价     MO-02/03 依赖 MO-01 的 opacity 字段，实际等于本批零交付
```

### 12.3 处置

```text
按项目规则 7 与「门禁放宽须先出授权文档」惯例，不得自行采用方案 A。
MO-01 置 BLOCKED，等用户对 A/B/C/D 裁决。
该合同属 Stage 14 冻结面，不属本专项所有，故不由本专项单方面修改。
```

## 11. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-31 | v1.0 | 首版。建立 MATOPQ 专项：固化资产事实与四处真实缺口；确认 ViewData 契约已冻结 alpha 字段故 MO-01..03 为实现补齐而非契约变更；裁定 MO-04 走 MATVOL 而非 roleMapping 并禁止放宽 config.cpp:1003；记录用户已回签的四项工艺语义与 C1-C4 判据条件；弹性材料通道归属列为 P1 阻塞输入 |
