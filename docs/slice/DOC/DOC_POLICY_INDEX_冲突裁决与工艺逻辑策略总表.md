# DOC_POLICY_INDEX 冲突裁决与工艺逻辑策略总表

> 文档状态：**ACTIVE / 首版盘点完成**
> 版本：v1.0 ｜ 日期：2026-08-31
> 定位：全仓**冲突裁决**与**工艺逻辑策略**的集中索引。本表是「有哪些策略、各自口径是什么」
> 的检索入口，**不取代**各专项决策文对自身策略的定义权。
> 触发：用户 2026-08-31 要求，在裁定 K3（V/T 同像素冲突）时一并盘点所有同类决策。

## 0. 阅读约定

```text
本表只收录【会改变输出结果】的裁决点，不收录纯格式校验（如「必须为正数」）。
每条给出：口径、实现位置、是否可配置、默认值、失败行为。
证据等级 A（当前代码）；与文档冲突时以代码为准并在此标注。
```

三种裁决风格，全仓通用：

```text
fail-closed   冲突即拒绝，不猜测。项目主导风格。
显式优先级     由 Profile 显式声明谁赢，无声明即拒绝。
邻域表决       统计邻域票数择多。仅 K3 一处，2026-08-31 新引入。
```

---

## 1. 像素所有权裁决（运行期，决定最终 TIFF 字节）

### 1.1 全局通道优先级【冻结】

```text
口径    Model > Support > Empty
位置    slicer.cpp（报告字段 modelPriority = "Model > Support"，slicer.cpp:5320）
可配置  否，冻结
依据    .agents/AGENTS.md §5 RGBWSV 协议红线
```

### 1.2 支撑类型优先级【冻结】

```text
口径    同一像素被多种支撑成因命中时，取优先级数值最大者
位置    slicer.cpp:1661 support_type_priority()，比较在 slicer.cpp:1734
可配置  否，硬编码
数值    InternalVoid 6 > UnsupportedIsland 5 > FullVerticalProjection 4
        > UpperProjection 3 > BottomProjection 2 > ProjectionBase 1 > None 0
注      SupportType 只进 metadata/report/debug，永不编码为 TIFF 通道值（协议红线）
```

### 1.3 材质与支撑冲突

```text
口径    model_material_over_support（模型材质压过支撑）
位置    config.h:100 MaterialPolicyConfig::conflict_policy
可配置  是，但校验只允许该唯一取值（config.cpp「materialPolicy.conflictPolicy must be
        model_material_over_support」），故实为冻结
```

### 1.4 外侧光油壳层冲突

```text
口径    varnish_shell_wins（光油壳层压过冲突方）
位置    config.h:305 OuterVarnishShellConfig::conflict_policy
可配置  是，默认 varnish_shell_wins
相关    support.upper.outside 必须为 outer_varnish_shell 或 model_envelope
```

### 1.5 MATVOL 材质体积重叠

```text
口径    explicit_priority —— 必须由 Profile 逐材质显式声明 priority，同级冲突 fail-closed
位置    config.h:180-188 MaterialVolumeOverlapConfig / MaterialVolumeOverlapRuleConfig
可配置  是；mode 校验只允许 explicit_priority
失败    matchMaterialName 重复 -> 拒绝；未声明 -> missingMaterial=fail_closed（唯一允许值）
```

### 1.6 V/T 同像素冲突【K3，2026-08-31 裁定，待实施】

```text
来源  用户 2026-08-31 裁定主策略（邻域多数表决）与分层兜底
状态  【设计已定，尚未实施】属 MATOPQ MO-04 范围
```

#### 1.6.1 分层裁决

```text
第 1 层  邻域无票（V+T == 0）
         -> 不是冲突。保留该像素原有归属，正常继续，不计入冲突诊断。

第 2 层  有票且票数不等
         -> 多数胜（V 多取 V，T 多取 T）。主策略。

第 3 层  有票且票数相等 -> 距离加权重投
         -> 正交邻居权重 1.0，对角邻居权重 1/sqrt(2) ≈ 0.7071
         -> 整数平局转为浮点比较，绝大多数平局在此打破，且完全确定性

第 4 层  加权后仍相等（需邻域完全对称，极罕见）
         -> 固定兜底优先级 = 【V 优先】（用户 2026-09-01 裁定）。不 fail。

诊断     第 3、4 层各自计数并落盘；第 4 层超阈值只【告警】，不阻断生产。
```

#### 1.6.2 为何不用纯 fail-closed（重要）

```text
首版曾建议「平局即 fail-closed」，该建议【错误且不可进入生产】，理由：
3x3 邻域下 0:0 票型（邻域内既无 V 也无 T）在模型内部与纯 RGB 区【极其常见】，
它根本不是平局而是「无信息」。若与真冲突同等对待并 fail，
每次切片都会在模型内部大面积触发失败，生产路径被自身护栏堵死。

修正口径：fail-closed 上移到【配置层】——未声明第 4 层兜底优先级时拒绝加载配置；
运行期逐像素不再 fail。既保住「不猜测」的哲学，又不阻断生产。
```

#### 1.6.3 必须固定的参数

| 参数 | 取值 | 状态 |
|---|---|---|
| 邻域半径 | 3×3（半径 1） | 用户 2026-08-31 采纳 |
| 维度 | 同层 2D | 用户采纳（跨层会把层厚差异卷入） |
| **更新方式** | **双缓冲（读旧写新）** | **硬约束：原地更新使结果依赖扫描方向 → Golden 必漂** |
| 迭代次数 | 单趟 | 用户采纳（多趟不保证收敛） |
| 邻域越界 | 只统计模型内像素 | 用户采纳 |
| 第 3 层权重 | 正交 1.0 / 对角 0.7071 | 本次新增 |
| 第 4 层兜底优先级 | **V 优先** | 用户 2026-09-01 裁定，参数已齐 |

```text
注   K3 是本仓库【唯一】的表决式裁决，其余裁决均为 fail-closed 或显式优先级。
     因此其参数约束写得比别处更严：任一参数变动都会改变 Golden，须重新基线。
```

#### 1.6.4 第 4 层取值的决策留痕

```text
裁定    V 优先（用户 2026-09-01）
实施    按裁定执行。

留痕    实施方曾建议 T 优先，理由为「弹性材料是结构性的、光油是表面功能层，
        结构缺失比表面缺失更难补救」。用户裁定取 V 优先，已按裁定实现。
        此处记录仅为日后追溯该参数的选取背景，不构成对裁定的保留意见。

影响面  该参数仅在 L4（邻域完全对称且加权后仍相等）生效，预期触发率极低；
        但它会进入 Golden，变更须重新基线。
        L4 触发次数须逐作业计数落盘，便于事后评估该取值的实际影响。
```

---

## 2. 配置层校验门（加载期 fail-closed）

```text
口径澄清   config.cpp 内 throw 的校验语句去重后共 130 条，但其中【混有纯格式校验】
           （如「必须为正数」「必须为 4 或 8」），并非全部都是冲突裁决。
           按 §0 约定，本表只详列【会挡住工艺组合】的互斥门，即 §2.1-2.4；
           130 应理解为「config.cpp 全部校验门总数」，而非「冲突裁决数」。
```

完整清单可由下列命令再生：

```bash
grep -oE '"[^"]*(does not support|requires|must (not )?[a-z])[^"]*"' src/slicer_core/config.cpp | sort -u
```

### 2.1 三条「不得放宽」的工艺互斥【重点】

| 门 | 位置 | 含义 |
|---|---|---|
| `white_underbase` × `materialRoleMapping` | config.cpp:1003 | 按需补白墨与材质角色映射互斥 |
| `white_underbase` × `materialPolicy` | config.cpp:998 | 按需补白墨与 MaterialPolicy 互斥 |
| `white_underbase` × `global_surface_shell` | config.cpp:994 | 按需补白墨只支持 Legacy 全实体 RGB 路径 |

```text
唯一已授权的窄放行：config.cpp:985 的 MV-06。
原注释：「MATVOL 自带最终 RGB，不依赖纹理顶面投影路径。
        其余白区禁令（Global 管线、materialPolicy、旧 roleMapping、
        whiteValue 冲突、OpenVDB）保持不变。」
即 MATVOL 可与 white_underbase 共存，roleMapping 被点名排除。
```

### 2.2 MATVOL 准入互斥

```text
materialVolumePolicy 要求  Legacy 管线 + relief_heightfield + legacy_center_sample
materialVolumePolicy 排斥  materialPolicy.enabled / materialRoleMapping.enabled
```

### 2.3 T 通道（弹性材料）协议绑定

```text
transferChannelPolicy.enabled=true 强制 packageProtocol = p0.rgbwsvt.1
且 channelOrder 必须恰为 R G B W S V T
matchSource 硬校验只允许 material_diffuse_rgb（TransferChannelConfig.cpp:139）
multipleMatches 只允许 fail_closed；missingRegion 允许 allow_empty | fail_closed
value 必须为 0（black_is_print）
位置  src/slicer_core/config/TransferChannelConfig.cpp:113-184
```

### 2.4 实验性 OpenVDB 准入

```text
admissionMode    strict_closed | warn_and_attempt | diagnostic_only | repair_then_strict
failurePolicy    fail_fast | diagnostic_only | non_production_only
writeProductionRgbwsv 要求 admissionMode=strict_closed，
                 且不得在 diagnostic_only / warn_and_attempt / repair_then_strict 下运行
红线             warn_and_attempt 产物永不算生产安全（AGENTS.md 生产安全规则 9）
```

---

## 3. 几何准入策略

```text
自交            selfIntersectionPolicy = reject（默认）| tolerate_closed_self_intersection
                确认自交必须 fail fast（AGENTS.md 生产安全规则 10）
非流形/重复面    必须阻断严格生产准入（生产安全规则 11）
开放表面        materialVolumePolicy.openSurface.mode = reject（默认）| surface_band
                surface_band 必须显式 thicknessMm，placement 只允许 below_surface
有界开边放宽     topology.maxBoundaryEdges，默认 0（须由工艺文件显式开启）
                位置 TransferChannelConfig.cpp:107，MQ-06 起 T 路径与 MATVOL 共用
```

### 3.1 退化面阈值 degenerateAreaEpsilonMm2（MATOPQ 新增，用户 2026-09-01 授权）

```text
配置   geometrySampling.degenerateAreaEpsilonMm2
单位   面积平方（mm^4）。判定式为 面积^2 <= 阈值 则丢弃该三角形。
默认   0（表示沿用适配器内建 1.0e-12，等价面积门 1e-6 mm^2）——既有工艺行为不变
校验   设值时必须有限且 <= 1e-6；否则 fail-closed
       （放宽过头会把真实薄面成片丢弃，比误杀极小面危害更大）
生效点 slicer.cpp 的 transfer 与 MATVOL 两处 AdaptSceneModelToTriangleMesh
```

**为什么需要它（A 级实测，2026-09-01）：**

```text
CAD/NURBS 导出（犀牛）的多材质资产含 nm^2 量级的【合法薄面】。
以 model/obj/multi-material/tm2-1 为例：
  573 个面被默认门判为退化，但【无一为零面积】
    最小面积 4.715e-09 mm^2   面积^2 = 2.223e-17
    最大面积 9.985e-07 mm^2
    中位数   1.979e-07 mm^2
  默认门 1e-12         -> 2.2e-17 < 1e-12  -> 误杀
  项目自适应公式 3.0e-13 -> 2.2e-17 < 3e-13 -> 仍误杀
    （MeshScaleTolerance: area_eps = position_eps^2 * 4，
      bbox 对角线 27.41mm 时 position_eps = 2.741e-07）
实测后果：默认门下 boundaryEdges=146；收紧到 1e-20 后 boundaryEdges=0（网格完全闭合）
结论：犀牛报告闭合与切片器判开放【并不矛盾】——是阈值误杀后才破洞。
      根因在软件侧，与设计端、资产命名、图层结构均无关。
```

**推荐值（UI 对外展示用）：**

| 档位 | 取值 | 面积门 | 适用 |
|---|---|---|---|
| 标准（默认） | `0`（=1e-12） | 1e-6 mm² | 既有资产；不改变任何现有行为 |
| **精细（推荐）** | **`1e-24`** | **1e-12 mm²** | **CAD/NURBS 导出的多材质资产（犀牛、SolidWorks 等）** |
| 自定义 | ≤ 1e-6 | — | 需实测该资产的最小面面积后再定 |

```text
精细档取 1e-24 的依据：
  须小于资产最小面积^2（2.2e-17）才能保留合法面 -> 1e-24 留了约 7 个数量级余量；
  须大于真退化面的数值噪声（三点共线时面积^2 落在 1e-30 量级）才能仍挡住它们。
  1e-24 同时满足两侧，且对 30mm 量级模型的 double 精度有充分裕度。

UI 建议
  不要让操作员直接填科学计数法，按上表给三档预设；
  选「精细」时提示：该档保留 CAD 导出的极小薄面，仅在标准档报
  「材质被判为开放表面」时使用；
  切换档位会改变几何准入结果与 Golden，须重新基线。
```

---

## 4. 材料闭合修复策略

```text
mode              diagnostic | repair_then_report
colorFillGap      只允许 model_fill
modelSupportGap   只允许 contextual
internalVoidGap   只允许 support
varnishSupportGap 只允许 support
前置              repair 要求 modelFill.enabled=true 且 support.enabled=true
位置              config.h:324-331 MaterialClosureRepairConfig
红线              manual_repair_required 永不计为生产 PASS
```

---

## 5. 纹理与白墨策略

```text
missingTexturePolicy    warn_and_fallback | fail_fast
nonSurfaceRgbPolicy     model_material | empty | fallback_rgb | material_policy
unprintableWhitePolicy  fail_closed | white_underbase
unprintableWhiteValue   不得等于 output emptyValue 255
white.mode              disabled | underbase | all_model | unprintable_white_underbase
white.coverage          all_model | model_surface | texture_unprintable_white
varnish.mode            disabled | all_model | top_n_layers
varnish.coverage        all_model | model_surface
```

---

## 6. 材质角色映射（roleMapping）

```text
mode          只允许 rules_then_default
匹配          matchNameContains 子串匹配，大小写不敏感（slicer.cpp:2400 lower_copy）
首命中即返回   规则顺序即优先级
角色集合      rgb | white | varnish | ignore | support_candidate | support
support 特例  role=support 需 allowInputSupportMaterial=true，否则降级为 support_candidate
落盘          Varnish -> V 通道直写 0（slicer.cpp:3068）
限制          relief 模式下为【逐 XY 列】取顶面材质（slicer.cpp:2528），
              无法表达同列纵深叠放；纵深场景须改用 MATVOL closed_intervals
```

---

## 7. 材质外观策略（MATOPQ 新增）

```text
不透明度归一   MTL d 与 Tr 归一为单一 opacity，Tr = 1 - d
               ⚠ 实测：全仓 65 个 MTL 中含 Tr 的仅 2 个且均为自建测试变体，
                 真实资产（Rhino）Tr 出现为 0。Tr 分支属跨导出器兜底，非主判据。
识别主判据     d <= opacityMax（Profile 容差），不得用 == 0
矛盾检测       同材质先后给出不相容的 d/Tr（如同时 d 0.0 与 Tr 0.0）时回报冲突，
               不让后出现者静默胜出。容差 1.0e-3
位置           src/slicer_core/model/MtlMaterialParser.cpp
预览 alpha     下限钳制 0.15，保证全透明区域仍可见可选中；
               属显示策略，【不得回写 MaterialInfo::opacity】，否则污染工艺判据来源
位置           SceneViewAssetResolver.cpp PreviewAlphaFromOpacity()
透明渲染       不透明批先画并写深度；透明批后画、按视深 back-to-front、不写深度
位置           CpuRasterizer.cpp
```

---

## 8. 已知缺口与待回签

| 编号 | 缺口 | 影响 |
|---|---|---|
| K3-a | V/T 邻域表决的**平局政策**未定 | 阻塞 K3 进入生产 |
| K3-b | 邻域半径 3×3 为暂定值，未回签 | 影响 Golden 基线 |
| K1 | `transfer.matchSource` 硬锁 `material_diffuse_rgb`；弹性按 Kd、光油按不透明度能否共存未验证 | 阻塞 MO-04 配置形状 |
| K2 | MATVOL + transfer + white_underbase 三者共存是否放行**未实测** | 阻塞 MO-04 |
| P2 | `opacityMax` 容差与 `0<d<1` 落位角色未定，须入 Profile 而非代码常量 | 阻塞 MO-04 |

---

## 9. 本表的维护约束

```text
新增任何【会改变输出结果】的裁决点时，必须同步登记到本表；
本表只做索引与口径摘要，策略的定义权仍属各专项决策文；
若本表与代码冲突，以代码为准并立即修正本表（证据等级 A 优先）。
```

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-31 | v1.0 | 首版。盘点全仓冲突裁决与工艺逻辑策略：像素所有权 6 类、配置互斥门 130 条（含 3 条不得放宽的工艺互斥与 1 条已授权窄放行）、几何准入、闭合修复、纹理白墨、角色映射、材质外观。登记 K3 邻域表决及其 6 项必须固定的参数，并标注平局政策未定为生产阻塞项。 |
