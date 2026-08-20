# TASKS_MATVOL 多材质纵深体积 RGB 与按需补白根治专项任务清单

> 文档状态：**ACTIVE / MV-00..MV-02 COMPLETE / MV-03 PREPARED / 生产语义未改**
> 版本：v1.2 ｜ 日期：2026-08-20
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
| MV-03 | 封闭材质有序交点、compact interval plan 与单层物化 | **PREPARED** | MV-02（已满足） | - |
| MV-04 | 开放表面 surface_band 非生产候选与 `03.obj` 厚度裁决 | PENDING / INPUT OPEN | MV-03、MQ-01/MQ-02 | - |
| MV-05 | 单层材质 owner、显式重叠优先级和 RGB 合成 | PENDING | MV-03；MV-04 可选 | - |
| MV-06 | Stage 15 按需补白、closure、报告和组合 Gate | PENDING | MV-05 | - |
| MV-07 | 参考宿主 Profile/UI/预检和 RGB-only 结果表达 | PENDING | MV-06 | - |
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

## 7. MV-04 开放表面候选

**目标：** 仅在显式厚度和 placement 下把开放表面物化为有限表面带；不改变 model occupancy。

**输入 Gate：** MQ-01 物理厚度、MQ-02 绿色覆盖浅桃色优先级。未确认时只允许 synthetic 诊断值，
不得写生产 Profile。

**验收：** reject 默认、厚度离散报告、方向冲突阻断、壳层不向模型外扩、`03.obj` 每层 owner 可解释。

## 8. MV-05 Owner 与 RGB

**目标：** 合并闭合区间/开放壳层，应用显式 priority，按 owner 解析 Kd/texture/fallback 写 RGB。

**验收：** 同级重叠阻断；顺序/线程无关；所有 model pixel 有且仅有一个有效 owner；绿色/浅桃色值
精确为 MTL 量化值；未绑定材质按策略阻断；不写 W/S/V。

## 9. MV-06 按需补白与闭合

**目标：** 在 MATVOL 最终 RGB 后复用 `ApplyUnprintableWhiteCarrier`，接入 semantic stats、closure 和
`material_volume_report`；只窄放行 MATVOL+white_underbase。

**验收：** 纯白/近白差异仅在 W；绿色/浅桃色不误补；RGB/S/V 和 Z 层数不变；旧 Stage 15 Profile
逐层 TIFF hash 不变；materialPolicy/旧 roleMapping 非支持组合继续拒绝。

## 10. MV-07 Host 与预览

**目标：** 新增显式“多材质纵深 RGB + 按需补白”候选；展示拓扑、壳层厚度和冲突诊断；结果页
优先提供 RGB-only 判断入口，组合预览明确 S 为伪彩色。

**验收：** workspace/profile/scene revision 持久化一致；旧预设不变；无资产能力时禁用而非静默回退；
UI smoke 覆盖 Profile 生效和错误解释。

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
| 2026-08-20 | v1.2 | MV-02 COMPLETE：新增 `MaterialVolumeError` 稳定错误码三件套与 `MaterialTopologyClassifier` 逐材质子网格分类；`materialVolumePolicy` 顶层配置块按仓库约定接入 `config.h`/`config.cpp`，并在 `ConfigMigration` 登记以堵住 `slicer.config.1` 的静默丢字段。13 项 fail-closed 逐条有用例，Release `/W4 /WX` 与定向 CTest 8/8 PASS。变异检验发现并修正了顶点焊接导致的材质交界边误判，改为真开边与交界边分列计数且交界边围成的子网格不视为独立闭合体。未接 `run_slicer`，未改动任何既有预设与 profileHash。MV-03 转 PREPARED。 |
| 2026-08-20 | v1.1 | MV-01 COMPLETE：新增 `tests/matvol/MatvolFactsTests.cpp` 与 `matvol_facts_unit_tests`，Release `/W4 /WX` 构建与 11/11 CTest 通过。固化 MV-F01..F06 合成 fixture、独立 dense owner oracle、Legacy 顶面投影 baseline、owner diff schema 与重复运行摘要；复刻并冻结 XY 采样中心、重心容差、`>=` 平局规则、层换算与 Kd 量化五条既有规则；机器化 Reality 03/08/09 逐材质拓扑事实。两处变异检验证明断言有效。补 `.gitignore` negation 使 `tests/matvol/` 可入库。未新增生产 API、未改 Profile/协议、未接 `run_slicer`。MV-02 转 PREPARED。 |
| 2026-08-20 | v1.0 | 创建 MV-00..10 原子卡、依赖、完成标准、INPUT OPEN 和收口 Gate。 |
