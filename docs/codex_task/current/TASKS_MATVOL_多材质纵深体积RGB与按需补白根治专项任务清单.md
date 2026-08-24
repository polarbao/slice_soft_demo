# TASKS_MATVOL 多材质纵深体积 RGB 与按需补白根治专项任务清单

> 文档状态：**ACTIVE / MV-00..MV-03、MV-05..MV-06 COMPLETE / MV-07A..07C COMPLETE
> / MV-04 INPUT OPEN（MQ-01 实测上限已给出，未回签）/ 生产默认 Profile 仍为 matvol 关闭**
> 版本：v1.7 ｜ 日期：2026-08-24
> 定位：不占 Stage 编号的独立材料体积专项；任务状态唯一真源
> 决策：`docs/slice/DOC/DOC_DECISION_MATVOL_多材质纵深体积RGB与按需补白根治.md`
> 准备：`docs/slice/DOC/DOC_PREP_MATVOL_实施准备与数据上下文.md`
> 方案：`docs/slice/DEV/DEV_MATVOL_逐层多材质所有权与白区载体兼容设计.md`
> 执行指令：`CODEX_PROMPT_MATVOL_多材质纵深体积RGB与按需补白根治执行指令.md`

## 1. 固定边界

```text
旧 Profile、p0.rgbwsv.2、RGBWSV、uint8、black_is_print 不变；
新能力显式 opt-in，先诊断后生产，不自动检测切路；
开放表面默认拒绝，surface_band 必须显式厚度；
材质重叠必须显式优先级，同级冲突 fail closed；
按需补白观察最终 RGB，只写同层 W；
不得建立 material * layer * pixel dense ownership 栈；
生产接线依赖 MEMFLOW owned/bounded 路径；
S3/S4、Global、OpenVDB 不进入首批生产范围。
```

## 2. 状态表

| 卡号 | 任务 | 状态 | 依赖 | 完成日期 |
|---|---|---|---|---|
| MV-00 | 专项决策、准备、DEV、任务清单、执行指令和初始报告 | COMPLETE | 用户授权创建专项 | 2026-08-20 |
| MV-01 | 资产事实、synthetic fixture、旧顶面投影 baseline 与独立 oracle | **COMPLETE** | MV-00 | 2026-08-20 |
| MV-02 | MaterialVolumePolicy、拓扑分类、稳定错误和配置合同 | **COMPLETE** | MV-01 Gate | 2026-08-20 |
| MV-03 | 封闭材质有序交点、compact interval plan 与单层物化 | **COMPLETE** | MV-02 | 2026-08-20 |
| MV-04 | 开放表面 surface_band 非生产候选与 `03.obj` 厚度裁决 | PENDING / INPUT OPEN | MV-03、MQ-01/MQ-02 | - |
| MV-05 | 单层材质 owner、显式重叠优先级和 RGB 合成 | **COMPLETE** | MV-03（MV-04 可选，未纳入） | 2026-08-21 |
| MV-06 | Stage 15 按需补白、closure、报告和组合 Gate | **COMPLETE** | MV-05 | 2026-08-21 |
| MV-07 | 参考宿主 Profile/UI/预检和 RGB-only 结果表达 | **COMPLETE（07A/07B/07C 全部落地）** | MV-06 | 2026-08-24 |
| MV-08 | MEMFLOW bounded/owned 生产候选接线与 Staged Package | PENDING | MV-06、MF-03B4/MF-04 | - |
| MV-09 | Reality/Golden/Package/RIP/取消/内存性能矩阵 | PENDING | MV-07、MV-08 | - |
| MV-10 | 生产 opt-in 准入、用户回签和专项收口 | PENDING / INPUT OPEN | MV-09、设备输入 | - |

## 3. MV-00 文档与上下文

**完成标准：** Decision/PREP/DEV/TASKS/Prompt/Report 互链；记录 `03.obj`/MTL hash、颜色、拓扑、
当前实现根因、兼容方案、任务依赖、停止条件和验证矩阵；不改生产代码。

**实际结果（2026-08-20）：** 文档包已创建。旧 Profile 保留、新 Profile 显式候选、开放面默认拒绝、
重叠显式优先级、最终 RGB 后复用 Stage 15 白区载体、bounded 单层所有权和 MEMFLOW 依赖已冻结。
MQ-01/MQ-02 继续 INPUT OPEN；未执行构建或生产验证。

## 4. MV-01 事实与 oracle

**目标：** 把本次人工/临时诊断变成自动化、可跨 AI 重复的事实基线。

**实施：**

```text
测试内构造 MV-F01..F06；
新增 test-local dense reference，只用于小网格 expected；
记录现有 top_triangle 投影对 03.obj 只能输出绿色的 baseline；
固化材质 Kd、face/usemtl、topology facts；
定义 owner diff schema：layer/x/y/expected/actual/material key；
不新增生产 API，不修改 Profile。
```

**验收：** fixture 可由 Release 测试独立生成；重复运行 hash 一致；Reality 资产缺失时明确 SKIP/blocked，
不得伪造 PASS；`git diff --check` 通过。

**实际结果（2026-08-20）：** 新增 `tests/matvol/MatvolFactsTests.cpp`（1173 行）与目标
`matvol_facts_unit_tests`，Release `/W4 /WX` 构建通过，`ctest -R "^matvol_"` **11/11 PASS**。

```text
交付
  MV-F01..F06 全部测试内构造，零外部资产、零 Golden 依赖
  独立 dense owner oracle（仅小网格 expected）与 Legacy 顶面投影 baseline 并列
  owner diff schema：layer / x / y / expected / actual / expectedMaterialKey / actualMaterialKey
  FNV-1a 64 摘要用于重复运行一致性

复刻并冻结的既有规则（源码逐条核对）
  XY 采样中心 (x+0.5, y+0.5)          slicer.cpp:1327-1332
  重心容差 -1e-9 / 退化阈值 1e-12      slicer.cpp:1179-1199
  顶面平局 zMm >= zMax（索引大者胜）    slicer.cpp:1343
  层中心 (i+0.5)*t、ceil/floor(z/t-0.5) slicer.cpp:1159, 1202-1208
  Kd 量化 round(clamp(v,0,1)*255)      model.cpp:1702-1705

机器证据
  MV-F01 Legacy 把顶面绿色铺满整列，层 0..1 本应浅桃色 → diff 非空并定位到具体 layer/x/y
  MV-F03 开放材质单列交点数为奇数 → unpaired，默认 fail closed；闭合材质不受影响
  MV-F04 分离实体保留两个区间，空腔层保持无 owner，未被 first/last 包络填平
  MV-F02 重叠仅由显式 priority 裁决，材质声明顺序反转结论不变；同级重叠记为 blocked
  Reality 03：usemtl 01 = 14966 面 / 1382 开放边 → OPEN；usemtl 02 = 12126 面 / 0 开放边 → CLOSED
  Reality 08/09：usemtl 02 开放边 16826 → 两材质皆开放，不能替代 03 作为闭合体基线

变异检验（证明断言非空转）
  14966 → 14967        FAIL reality_03_topology_facts
  平局规则 >= 改为 >    FAIL legacy_top_projection_cannot_express_depth

边界
  未新增任何生产 API、未改 Profile、未改协议、未接 run_slicer
  Legacy 投影链路全在 slicer.cpp 匿名命名空间（:46-4272）内，测试 TU 不可达，
  因此本卡在测试内复刻规则；类型提升留待 MV-02 在独立文件中按 wrap-first 处理
  连带修复 `.gitignore`：`tests/*` 原本会永久忽略 `tests/matvol/`，已补 negation
```

## 5. MV-02 配置与拓扑合同

**目标：** 建立 `MaterialVolumePolicy`、move-only plan 输入、拓扑分类和稳定错误，不接 `run_slicer`。

**验收：** unknown enum、Global/S3/S4、缺材质、open reject、non-manifold/self-intersection、重复规则、
无优先级全部 fail closed；旧配置缺字段时行为不变；新 Profile 被旧构建拒绝而非降级。

**实际结果（2026-08-20）：** 新增 `materials/volume/` 两组文件与 `matvol_topology_unit_tests`，
Release `/W4 /WX` 构建通过，定向 CTest **8/8 PASS**。

```text
交付
  MaterialVolumeError.*        E_MATVOL_* 九码三件套，what() 形如 `E_MATVOL_XXX: <message>`
  MaterialTopologyClassifier.* 逐材质子网格五态分类，复用 AnalyzeMeshTopology 与
                               AnalyzeCompleteMeshSelfIntersections，不重造边统计
  config.h / config.cpp        materialVolumePolicy 顶层块，默认关闭，解析与校验按仓库既有约定
  ConfigMigration.cpp          登记 CopyIfPresent，堵住 slicer.config.1 白名单重建的静默丢字段
  samples/configs/matvol/      默认关闭的解析回归 fixture

fail-closed 覆盖（逐条有用例）
  未知 mode / missingMaterial / openSurface.mode / placement / overlap.mode
  surface_band 缺正厚度、负厚度
  规则名为空、规则名重复
  slicingMode 非 relief_heightfield、S3/S4 采样、非 Legacy 管线
  materialPolicy.enabled 与 materialRoleMapping.enabled 组合
  关闭时即便字段非法也不阻断旧 Profile（旧配置行为不变）

🔴 设计修正（由变异检验发现）
  `AdaptSceneModelToTriangleMesh` 做全网格顶点焊接，材质交界边在子网格里会退化为边界边。
  首版只按 boundaryEdgeCount 判 OpenSurface，导致焊接模型的交界边被误计。
  已改为：全网格边度数表区分【真开边】与【材质交界边】，两者分列计数；
  且【仅由交界边围成的子网格同样不是独立闭合体】（射线拿不到成对交点），
  一并归入 OpenSurface 保持 fail-closed，由 MV-04 依据两个计数区分开放来源。

变异检验
  把交界边判定短路为恒真 → material_interface_edges_counted_separately 按预期 FAIL

边界
  未接 run_slicer；未改 p0.rgbwsv.2、RGBWSV、SPI 或既有 Profile 语义；
  新块未写入任何既有预设，因此不改动任何现有 profileHash
```

## 6. MV-03 封闭材质区间

**目标：** 对封闭可定向材质子网格生成 compact 多区间，并物化调用方持有的单层 owner buffer。

**验收：** MV-F01/F02/F04 全层 owner diff=0；空洞不被 first/last 填满；共享边/共面确定；奇数交点
阻断；热路径无分配、buffer 地址复用、取消无半结果；新 TU/test `/W4 /WX`。

**实际结果（2026-08-20）：** 新增 `MaterialVolumePlan.*` 与 `matvol_interval_unit_tests`，
Release `/W4 /WX` 构建通过，定向 CTest **10/10 PASS**。

```text
交付
  MaterialVolumeGrid   自带 XY 栅格与层参数（匿名命名空间里的 GridSpec 不可复用）
  MaterialVolumePlan   move-only、私有构造 + friend 工厂，外部无法构造半成品
  CSR compact 布局     columnIntervalOffsets_（列数+1）+ 扁平 intervals_
  Build/Materialize    逐列有序交点与奇偶配对；单层 owner 写入 caller buffer，热路径零分配无跨层扫描

复用而非重造
  拓扑判定复用 MV-02 的 ClassifyMaterialTopologies，仅 ClosedOrientable 进入区间求解
  XY 采样中心、重心容差 -1e-9、退化阈值 1e-12、层换算公式全部复刻既有 S0 规则

构建期 fail closed（物化期因此无需再判定）
  开放材质 OPEN_SURFACE_REQUIRES_POLICY；缺失显式优先级与同级实际重叠 OVERLAP_UNRESOLVED；
  奇数交点 INTERSECTION_UNPAIRED；未绑定材质 MATERIAL_MISSING；非法栅格 TOPOLOGY_INVALID；
  取消 BUDGET_EXCEEDED —— move-only 加末尾 return 保证不留半成品

验收逐项
  MV-F01 层 0..1 归材质 02、层 3..4 归材质 01、层 2 与 5..7 无 owner，与手算一致
  MV-F02 重叠层由 priority 200 胜出，材质声明顺序反转结论不变
  MV-F04 空洞层 2..3 保持无 owner，每列保留两段独立区间
  物化与独立稠密 oracle 逐层逐列 diff=0；modelMask 屏蔽像素恒为 kNoMaterialOwner
  4 轮 x 8 层重复物化堆分配计数为 0，buffer 地址不变
  compact 区间总数 32，为同规模稠密栈 256 的 1/8；重复构建产出逐字段相同的区间与偏移表
  层号越界与缓冲区尺寸不符显式抛 std::invalid_argument，与既有 MaterializeLayerOccupancy 一致，
  不采用静默无操作

共面/共享边确定性
  4x4 栅格中有 4 列的采样中心恰好落在顶/底面三角面的共享对角线上，两三角面同时命中同一 Z。
  合并共面重复命中后仍为 2 个交点、1 段区间；若不合并会得到 4 个交点并配出两段零厚区间。
  该路径由现有用例真实覆盖，不是构造性断言。

变异检验
  奇偶配对改成 first/last 包络 -> separated_bodies_keep_cavity_unowned 按预期 FAIL
  优先级比较方向反向           -> overlapping_bodies_resolve_by_priority 与 oracle 对照按预期 FAIL

边界
  未接 run_slicer；未改 p0.rgbwsv.2、RGBWSV、SPI 与既有 Profile 语义；不写 RGB/W/S/V
```

## 7. MV-04 开放表面候选

**目标：** 仅在显式厚度和 placement 下把开放表面物化为有限表面带；不改变 model occupancy。

**输入 Gate：** MQ-01 物理厚度、MQ-02 绿色覆盖浅桃色优先级。未确认时只允许 synthetic 诊断值，
不得写生产 Profile。

**验收：** reject 默认、厚度离散报告、方向冲突阻断、壳层不向模型外扩、`03.obj` 每层 owner 可解释。

## 8. MV-05 Owner 与 RGB

**目标：** 合并闭合区间/开放壳层，应用显式 priority，按 owner 解析 Kd/texture/fallback 写 RGB。

**验收：** 同级重叠阻断；顺序/线程无关；所有 model pixel 有且仅有一个有效 owner；绿色/浅桃色值
精确为 MTL 量化值；未绑定材质按策略阻断；不写 W/S/V。

**实际结果（2026-08-21）：** 新增 `MaterialLayerRgbComposer.*` 与 `matvol_rgb_compose_unit_tests`，
Release `/W4 /WX` 构建通过，定向 CTest **5/5 PASS**；matvol 四个目标合计 4/4 PASS。

```text
交付
  MaterialRgbTable        move-only，构建期解析并校验逐材质 RGB，附 mtl_kd / explicit_fallback 来源
  BuildMaterialRgbTable   MTL Kd 优先，其次显式 fallback；缺 Kd 且未授权 fallback 时 fail closed
  ComposeMaterialLayerRgb 单层 owner 到 RGB 交错写入，不分配、不跨层扫描

验收逐项
  纵深两材质 RGB 精确等于 MTL 量化值：材质 02 为 [255,220,198]，材质 01 为 [63,190,126]
  缺 Kd 默认 E_MATVOL_MATERIAL_MISSING；显式 fallback 才写入给定颜色且来源标记 explicit_fallback
  mask 标记为模型却无 owner 时报 E_MATVOL_MODEL_PIXEL_UNOWNED
  mask 为 0 的像素写入调用方给定的 unownedRgb，不报错且不隐式约定背景
  材质声明顺序反转与重复调用结果逐字节一致；重叠层由 priority 200 的绿色胜出
  缓冲区尺寸不符显式抛 std::invalid_argument

只写 RGB 的机器证据
  用 6N 字节缓冲模拟 RGBWSV 交错，仅把前 3N 字节交给合成器，其后 3N 字节填 0xAB 哨兵；
  合成后哨兵逐字节未变，同时 RGB 区域已正确写入，证明不触碰 W/S/V。

变异检验
  去掉未拥有的模型像素守卫后 model_pixel_without_owner_fails_closed 按预期 FAIL

边界
  未接 run_slicer；不写 W/S/V、不新增 Z 层、不改 model occupancy；
  纹理采样未纳入本卡，当前解析链为 MTL Kd 到显式 fallback，Texture2D/UV 留待后续卡
```

## 9. MV-06 按需补白与闭合

**目标：** 在 MATVOL 最终 RGB 后复用 `ApplyUnprintableWhiteCarrier`，接入 semantic stats、closure 和
`material_volume_report`；只窄放行 MATVOL+white_underbase。

**验收：** 纯白/近白差异仅在 W；绿色/浅桃色不误补；RGB/S/V 和 Z 层数不变；旧 Stage 15 Profile
逐层 TIFF hash 不变；materialPolicy/旧 roleMapping 非支持组合继续拒绝。

**实际结果（2026-08-21）：** 新增 `MaterialVolumeWhiteCarrier.*`、`reports/MaterialVolumeReport.*`、
`contracts/slicesoft.material_volume_report.1.schema.json` 与 `matvol_white_carrier_integration_tests`，
Release `/W4 /WX` 构建通过，定向 CTest **5/5 PASS**，`json_schema_contract_test` PASS。

```text
交付
  ApplyMaterialVolumeWhiteCarrierLayer  在最终 RGB 之后调用既有 ApplyUnprintableWhiteCarrier，
                                        只写 W，逐像素判据与 Stage 15 完全同源，不复制近似实现
  IsMaterialVolumeWhiteCarrierCombinationAllowed  组合窄放行谓词，供后续预检复用
  BuildMaterialVolumeReport             扁平领域对象风格，与 MaterialClosureReport 一致
  CountMaterialVolumeLayerOwners        逐层 owner 与未拥有模型像素统计
  slicesoft.material_volume_report.1    首个具备正式 JSON Schema 文件的报告；URL 风格 $id、
                                        additionalProperties: false、schema 用 const 钉死

config.cpp 的改动只有一处条件
  白区禁令块里给「只支持 Legacy 全实体 RGB 纹理路径」这一条加了 && !materialVolumePolicy.enabled。
  MATVOL 自带最终 RGB，不依赖纹理顶面投影；其余禁令（Global 管线、materialPolicy、
  旧 roleMapping、whiteValue 与 emptyValue 冲突、OpenVDB）全部原样保留，错误消息不变。

一处自我修正
  首版还加了一个正向的窄放行检查，结果它抢在既有 materialPolicy / roleMapping 禁令之前触发，
  把原有错误消息替换掉了。既有禁令本就覆盖这两项，该检查纯属冗余且损害可审计性，已移除。
  config.cpp 因此也不再依赖 materials/volume 头文件。

验收逐项
  纯白 [255,255,255] 与近白 [250,249,248] 命中补白；绿色与浅桃色不误补
  逐像素结论与既有 IsUnprintableWhiteTexel 谓词一致（用同一阈值对照断言）
  RGB 缓冲在补白前后逐字节相同；S/V 区域用 0xAB 哨兵证明未被触碰
  策略关闭时零写入零计数；非模型像素跳过；缓冲区尺寸不符显式抛 std::invalid_argument
  MATVOL + white_underbase 通过；旧 Stage 15 Profile 仍原样通过
  关闭 MATVOL 时缺纹理路径的旧禁令继续生效（这是「旧禁令不得放宽」的机器证据）
  MATVOL + materialPolicy / + 旧 roleMapping / whiteValue 冲突 三项仍以【原有错误消息】被拒
  报告顶层字段集与 schema required 清单逐项一致且无多余字段，防止 builder 与 fixture 漂移
  schema 契约测试：1 正例 + 1 surface_band 变体正例 + 4 反例（未知拓扑、负厚度、多余字段、越界通道）

变异检验
  把旧纹理路径禁令整体短路 -> combination_allowance_stays_narrow 按预期 FAIL

边界
  未接 run_slicer；不写 RGB/S/V、不新增 Z 层；未改 p0.rgbwsv.2、RGBWSV、SPI；
  新块未写入任何既有预设，现有 profileHash 不变；closure 报告接入留待生产接线卡
```

## 10. MV-07 Host 与预览

**目标：** 新增显式“多材质纵深 RGB + 按需补白”候选；展示拓扑、壳层厚度和冲突诊断；结果页
优先提供 RGB-only 判断入口，组合预览明确 S 为伪彩色。

**验收：** workspace/profile/scene revision 持久化一致；旧预设不变；无资产能力时禁用而非静默回退；
UI smoke 覆盖 Profile 生效和错误解释。

**实施准备（2026-08-21）：** 见 `docs/slice/DOC/DOC_PREP_MATVOL_MV_07_宿主接入实施准备.md`。

```text
本卡与 MV-01..06 性质不同：它必须放宽参考宿主两道既有生产校验门，因此按
CODEX_PROMPT_MATVOL §2 先出实施准备并等待确认，未确认前不改宿主既有门。

四条已实测约束
  ① 宿主禁止出现 slicer_core / slicer_base / slicer_engine 裸子串（含注释），
     不能复用 MaterialVolumePolicyConfig，必须自建镜像并由 Worker 解析回来
  ② 两套行数门禁：ValidateQtHostBoundary 的 500 行规则在 HEAD 上已失败（8 文件超限）；
     ValidateSourceSizeGuard 的 protectedPrefixes 含 apps/slicer_ui_host_sim/ 且不可白名单；
     HostSliceSettingsPanel.cpp 已 820 行，距 G2 永久冻结线仅剩 180 行
  ③ 必须显式放宽 HostSliceSettings.cpp:297-310 与 HostMaterialProfile.c:79-86/210-215
     两道白区门，且只增加 materialvolumeenabled 形态条件以保旧组合逐字节不变
  ④ slicingMode 耦合缺口：宿主由 textureenabled 推导 relief_heightfield，而 MATVOL
     本意是不走纹理路径；需在 useReliefHeightfield 增加 materialvolumeenabled 条件

原子拆分
  MV-07A  设置镜像、双模板 Profile 发射、slicingMode 修复、放宽两门、预设与哈希闭合   含生产语义变化
  MV-07B  独立子面板、能力不足禁用而非回退、UI smoke、持久化                          纯 UI 增量
  MV-07C  结果页 RGB-only 判读入口与 S 伪彩色标注                                      纯 UI 增量

验收口径的三处修正
  「禁用而非静默回退」与现状冲突：SetSingleMaterialRestriction 当前确实自动回落，
    新预设应照 HostProfilePanel::SetProfiles 的禁用加原因形状，不沿用回落写法
  「scene revision 持久化」无对应实现：revision 只在进程内，应理解为 workspace/profile
    经 HostWorkspaceState 持久化且新预设不破坏 ValidateSceneBinding
  「RGB-only 入口」已存在（预览模式 index 0），缺的是默认索引与伪彩色标注；
    全仓 apps/ 与 src/ 下「伪彩」零命中，S 通道伪彩色在 UI 上没有任何说明文字

待确认 MV07-Q1 放宽两道宿主门 / Q2 500 行规则处置 / Q3 预设是否需固定采样或层厚 /
        Q4 是否可改结果页默认预览索引
```

### 10.1 MV-07A/07B/07C 实施结论（2026-08-24 回填）

三张子卡全部 **COMPLETE**，MV07-Q1..Q4 全部已回签：Q1 放宽两道宿主白区门已授权并
单独出决策（`DOC_DECISION_MATVOL_MV_07_Q1_宿主白区门放宽授权.md`，提交 `5c37617`）；
Q2 500 行规则改为债务台账（`DOC_DECISION_MATVOL_MV07_Q2_宿主行数门禁债务台账.md`，
提交 `f27bdee`）；Q3 预设不固定采样与层厚；Q4 结果页默认预览索引获准改动。

| 子卡 | 结论 | 提交 | 关键落点 |
|---|---|---|---|
| MV-07A | COMPLETE | `2c9b449` | 宿主自建 `hostmaterialvolumesettings` 镜像（不引 slicer_core 裸子串）；新增第 7 个工艺预设 `volumetric_nail_rgb_white_ondemand_lower_support`（primary `01` / secondary `02`）；独立 `HostVolumetricProfile.c` 以规范化+紧凑双模板发射 `materialVolumePolicy`，未改既有模板；`useReliefHeightfield` 补 `materialvolumeenabled` 条件修复 slicingMode 耦合缺口；按 Q1 授权只增形态条件放宽两道白区门，旧组合逐字节不变；profileHash 闭合 |
| MV-07B | COMPLETE | `71da50e` | 独立子面板 `HostMatvolSettingsPanel`（不复用回落写法）；能力不足时 `SetCapabilityRestriction` 只禁用并给原因，不改写用户选择、不发信号；MATVOL 显式排除于静默回退分支；`HostWorkspaceMatvolState` 独立持久化段，workspace schema 6→7 |
| MV-07C | COMPLETE | `39de313`、`00d74b0` | 结果页默认预览索引 4→0（RGB-only）；补 5 处伪彩色说明与第 4 行摘要；`00d74b0` 更正伪彩色来源为 `MaterialPreviewComposer` 硬编码调色板，并非 `preview.pseudoColors` |

**MV-07 遗留边界**：生产默认 Profile 仍为 matvol 关闭，新预设为显式 opt-in；
MV-04 壳层厚度仍受 MQ-01 阻塞（实测几何上限 0.30 mm、推荐 0.228 mm，未回签，不得写入生产 Profile）。

## 11. MV-08 生产候选接线

**目标：** 消费 MEMFLOW caller-owned 单层 buffer 和 staged writer；作业开始前预算/能力选路。

**验收：** 不保留全层 owner/RGBWSV；取消/consumer/writer 失败清理 staging；不发布半包；不能满足预算
时明确 `E_MATVOL_BUDGET_EXCEEDED`；Retained 旧路径可回滚。

## 12. MV-09 回归矩阵

```text
Fixtures MV-F01..F06 + Reality 03 + Stage15 + existing OBJ/3MF mapping；
S0，support on/off，base/varnish/closure，none/PackBits；
old/new Profile，1/多实例，cold/warm，cancel/fault；
逐层 owner/RGBWSV hash、reports、manifest、preview、RIP strict；
wall/CPU/Peak Working Set，记录 build identity 和重复次数。
```

无相同请求 before/after 时不得给出性能提升比例。

## 13. MV-10 收口

**出口：** 用户确认开放壳层与优先级；所有 Gate 有仓库证据；候选只在显式 Profile 可用；正式设备
SLA/物理打印证据缺失时只写 engineering candidate，不写 production SLA PASS。

## 14. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-24 | v1.7 | MV-07 转 COMPLETE，回填 07A/07B/07C 三张子卡的落点与提交（`2c9b449`、`71da50e`、`39de313`+`00d74b0`），新增 §10.1。MV07-Q1..Q4 全部回签：Q1 白区门放宽已授权并单独出决策 `5c37617`；Q2 `ValidateQtHostBoundary` 500 行规则改为债务台账（8 个既有超限文件只许缩减不许增长），该门首次转绿，提交 `f27bdee`；Q3 预设不固定采样/层厚；Q4 结果页默认预览索引获准改为 RGB-only。MQ-01 补入实测几何上限 0.30 mm 与推荐值 0.228 mm，仍未回签，MV-04 保持 INPUT OPEN。 |
| 2026-08-21 | v1.6 | MV-07 完成实施准备并拆为 07A/07B/07C：新增 `DOC_PREP_MATVOL_MV_07_宿主接入实施准备.md`。固化四条已实测约束（宿主禁引 slicer_core 裸子串、两套行数门禁且宿主 UI 不可白名单、必须放宽的两道白区门、slicingMode 耦合缺口）与 profileHash 四条规范化规则及 H-F-04 根因。修正三处验收口径：禁用而非回退与现状 SetSingleMaterialRestriction 自动回落冲突、scene revision 实际不持久化、RGB-only 入口已存在但缺默认索引与伪彩色标注。登记 MV07-Q1..Q4；07A 含生产语义变化，未获确认前不修改宿主既有门。 |
| 2026-08-21 | v1.5 | MV-06 COMPLETE：在最终 RGB 之后复用既有 `ApplyUnprintableWhiteCarrier` 完成按需补白，只写 W 且逐像素判据与 Stage 15 同源；新增 `material_volume_report` 构建与首个正式报告 JSON Schema `slicesoft.material_volume_report.1`（1 正例 + 1 变体正例 + 4 反例）。`config.cpp` 的改动收敛为单一条件：只给「仅支持 Legacy 全实体 RGB 纹理路径」这一条加 `&& !materialVolumePolicy.enabled`，其余白区禁令与错误消息原样保留。首版误加的正向窄放行检查会抢占既有禁令消息，已移除。RGB 逐字节不变、S/V 以哨兵证明未触碰、旧禁令未放宽均有机器证据；变异检验短路旧禁令后按预期 FAIL。Release `/W4 /WX` 与定向 CTest 5/5、schema 契约测试 PASS，未接 `run_slicer`。MV-07 转 PREPARED。 |
| 2026-08-21 | v1.4 | MV-05 COMPLETE：新增 move-only `MaterialRgbTable` 与 `ComposeMaterialLayerRgb`，按 owner 解析 MTL Kd 并合成单层 RGB。绿色与浅桃色精确等于量化值；缺 Kd 默认 fail closed，仅显式策略允许 fallback 且记录来源；模型像素无 owner 报 `E_MATVOL_MODEL_PIXEL_UNOWNED`；以 0xAB 哨兵证明仅写 RGB 区域、不触碰 W/S/V；声明顺序与重复调用结果逐字节一致。变异检验去掉未拥有守卫后按预期 FAIL。Release `/W4 /WX` 与定向 CTest 5/5 PASS，未接 `run_slicer`。纹理采样未纳入本卡；MV-04 仍受 MQ-01/MQ-02 阻塞，MV-06 转 PREPARED。 |
| 2026-08-20 | v1.3 | MV-03 COMPLETE：新增 move-only `MaterialVolumePlan` 与 CSR compact 层区间布局，实现逐列有序交点、奇偶配对与 caller-owned 单层 owner 物化；开放材质、缺失优先级、同级重叠、奇数交点、未绑定材质、非法栅格与取消全部在构建期 fail closed，物化期保持纯净。MV-F01/F02/F04 逐层 owner 与独立稠密 oracle diff=0，空洞层保持无 owner，热路径零堆分配且 buffer 地址复用，compact 区间为同规模稠密栈的 1/8。两处变异检验（包络填充、优先级反向）均按预期 FAIL。物化的参数校验改为显式抛出以对齐既有 `MaterializeLayerOccupancy` 约定。Release `/W4 /WX` 与定向 CTest 10/10 PASS，未接 `run_slicer`。MV-04 仍受 MQ-01/MQ-02 输入 Gate 阻塞。 |
| 2026-08-20 | v1.2 | MV-02 COMPLETE：新增 `MaterialVolumeError` 稳定错误码三件套与 `MaterialTopologyClassifier` 逐材质子网格分类；`materialVolumePolicy` 顶层配置块按仓库约定接入 `config.h`/`config.cpp`，并在 `ConfigMigration` 登记以堵住 `slicer.config.1` 的静默丢字段。13 项 fail-closed 逐条有用例，Release `/W4 /WX` 与定向 CTest 8/8 PASS。变异检验发现并修正了顶点焊接导致的材质交界边误判，改为真开边与交界边分列计数且交界边围成的子网格不视为独立闭合体。未接 `run_slicer`，未改动任何既有预设与 profileHash。MV-03 转 PREPARED。 |
| 2026-08-20 | v1.1 | MV-01 COMPLETE：新增 `tests/matvol/MatvolFactsTests.cpp` 与 `matvol_facts_unit_tests`，Release `/W4 /WX` 构建与 11/11 CTest 通过。固化 MV-F01..F06 合成 fixture、独立 dense owner oracle、Legacy 顶面投影 baseline、owner diff schema 与重复运行摘要；复刻并冻结 XY 采样中心、重心容差、`>=` 平局规则、层换算与 Kd 量化五条既有规则；机器化 Reality 03/08/09 逐材质拓扑事实。两处变异检验证明断言有效。补 `.gitignore` negation 使 `tests/matvol/` 可入库。未新增生产 API、未改 Profile/协议、未接 `run_slicer`。MV-02 转 PREPARED。 |
| 2026-08-20 | v1.0 | 创建 MV-00..10 原子卡、依赖、完成标准、INPUT OPEN 和收口 Gate。 |
