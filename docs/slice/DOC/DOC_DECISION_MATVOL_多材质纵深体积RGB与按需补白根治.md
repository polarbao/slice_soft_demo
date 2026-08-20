# DOC_DECISION_MATVOL 多材质纵深体积 RGB 与按需补白根治专项

> 文档状态：**PROPOSED / PREPARATION AUTHORIZED / PRODUCTION CHANGE NOT AUTHORIZED**
> 版本：v1.0 ｜ 日期：2026-08-20
> 任务真源：`docs/codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`
> 技术方案：`docs/slice/DEV/DEV_MATVOL_逐层多材质所有权与白区载体兼容设计.md`
> 实施准备：`docs/slice/DOC/DOC_PREP_MATVOL_实施准备与数据上下文.md`

---

## 1. 一句话裁决

现有 Legacy `relief_heightfield` 只保存每个 XY 列的最高三角面，并把该三角面的颜色沿整列实体传播，
不能表达同一列随 Z 变化的多材质所有权。MATVOL 新增显式、版本化、默认关闭的逐层材质所有权候选；
先支持封闭材质子网格，再以显式物理厚度候选支持开放表面。旧 Profile、旧输出协议和旧 Golden
必须逐字节保持不变，未经组合 Gate 不得替换生产默认。

## 2. 触发资产与已知事实

资产：`model/obj/reality/finger_suoguo/03.obj` 与同目录 `03.mtl`。

| 项 | 当前事实 |
|---|---|
| OBJ SHA-256 | `978f88f1ffadef8a41f9c3f29a818c9943f30a54d34ca2ceb14433e7cd1ecf62` |
| MTL SHA-256 | `842c485abe7a505cfb429f82ec983fee9dfd6dcc4cb7838cea2b8f872fcdc3a6` |
| 材质 `01` | `Kd=(0.2471,0.7451,0.4941)`，RGB `[63,190,126]`，绿色 |
| 材质 `02` | `Kd=(1.0000,0.8627,0.7765)`，RGB `[255,220,198]`，浅桃色 |
| 面数 | `01=14966`，`02=12126`，总计 `27092` |
| 拓扑初查 | `01` 有 `1382` 条开放边；`02` 为闭合子网格；全局仍有 `1382` 条开放边 |
| 当前切片 | 即使启用既有输入材料映射，临时 63 层诊断的 `43851` 个非空 RGB 像素全部为绿色 |

上述临时诊断必须由 MATVOL-01 固化为可重复 C++ fixture/oracle 后，才可作为回归真源。

### 2.1 MV-01 机器化复核结果与同目录变体资产（2026-08-20 追加）

§2 表中的 `03.obj` 事实已由 `matvol_facts_unit_tests` 机器化复核，逐项吻合：
`usemtl 01` = 14966 面 / 1382 开放边（**OPEN**）；`usemtl 02` = 12126 面 / 0 开放边（**CLOSED**）；
两者非流形边均为 0。两个 SHA-256 亦与记录逐位一致。

同目录另有 **`08.obj` / `09.obj`**（2026-08-20 新增，晚于本决策首版），其 MTL 把 `02` 由浅桃色
改为纯黄 `[255,255,0]`。逐材质拓扑核算结果：

```text
03.obj    usemtl 01  14966 面  开放边 1382    OPEN
          usemtl 02  12126 面  开放边 0       CLOSED   ← 唯一满足 MV-03 前提的资产
08/09     usemtl 01  14966 面  开放边 1382    OPEN
          usemtl 02  12098 面  开放边 16826   OPEN（顶点未焊接，26560 边 / 12098 面）
```

⇒ **`03.obj` 保留为主触发资产**；`08.obj` / `09.obj` **不得替代**它作为封闭材质基线，
仅作为"两个材质皆开放、默认必须双双 fail closed"的负例，已纳入 MV-01 用例。

**MQ-02 的几何依据（仅为证据，不构成回签）**：`03.obj` 两材质 Z 范围为
`01 = [303.2790, 309.5736]`、`02 = [303.7267, 309.5736]`，几乎完全重叠且共享同一顶面，
形态为绿色开放面包裹浅桃色闭合主体——这解释了顶面投影 100% 命中绿色。
MQ-02 仍需用户回签，不因本证据自动关闭。

## 3. 根因裁决

1. MTL 解析和工作区表面显示正确；MeshLab 中的黄色不是 `03.mtl` 声明颜色。
2. 结果页默认组合 RGB/W/S/V，S 通道使用绿色伪彩色，不能用该组合判断 RGB 材质。
3. `ReliefColumnInfo::top_triangle_index` 每列只有一个材质事实；`build_material_role_columns()`
   只能把最高三角面材质复制到整列。
4. 开放表面没有天然体积，不能通过材料名或面颜色无歧义地推断内部归属。

## 4. 产品语义边界

### 4.1 新旧 Profile

```text
旧：全实体 RGB + 按需补白墨
    保持现有顶面颜色沿列传播语义，hash/输出不变。

新：多材质纵深 RGB + 按需补白墨（候选）
    显式启用 materialVolumePolicy；按层解析材质所有权；默认不生产准入。
```

不得把新语义静默塞进既有 Profile，也不得因检测到多个 `usemtl` 自动切路。

### 4.2 封闭与开放材质

| 输入 | 默认行为 | 候选行为 |
|---|---|---|
| 封闭、可定向材质子网格 | parity/有序交点形成体积区间 | 可进入生产候选 |
| 开放材质表面 | `fail_closed` | 仅显式 `surface_band` 候选 |
| 自交、非流形、重复反向面 | 阻断 | 不得通过壳层策略掩盖 |
| 未绑定材质或 MTL 缺失 | 阻断或显式 fallback | 不得静默继承相邻材质 |

开放表面壳层使用物理厚度 `thicknessMm`，离散为有效层数并写入报告。当前 `03.obj` 的绿色
开放表面采用多少厚度仍为产品输入；未确认前只能做诊断候选。

### 4.3 重叠与优先级

多材质同时占据一个体素时必须由 Profile 提供显式优先级。禁止依赖 OBJ 面顺序、材料数组顺序、
hash 顺序或线程完成顺序。优先级相同且发生重叠时生产模式 fail closed。针对 `03.obj` 的推荐候选
是绿色表面壳层覆盖浅桃色闭合主体，但该条款需通过 MQ-02 回签。

## 5. 按需补白兼容裁决

MATVOL 不直接复用现有 `materialRoleMapping.enabled=true` 路径，也不直接删除 Stage 15 的组合禁令。
新路径按以下顺序接线：

```text
当前层 model occupancy
  -> 当前层 material ownership
  -> MTL Kd / texture 解析为最终 RGB
  -> ApplyUnprintableWhiteCarrier(finalRgb)
  -> 同层 W 写入
  -> support/varnish/closure/report
  -> RGBWSV TIFF
```

按需补白只观察最终 RGB；不修改 RGB，不新增 Z 层，不触碰 S/V，也不新增 ownership mask。
绿色和浅桃色通常不命中严格纯白阈值；真实纯白/近白区域继续按既有阈值和值写 W。

## 6. 永久红线

```text
不改 p0.rgbwsv.2、RGBWSV 顺序、uint8、black_is_print；
不改 SPI v1、Worker 文件合同和 RIP 接口；
不默认启用 OpenVDB，不新增强制第三方依赖；
不改变旧 Profile、S0/P0/Legacy 默认；
不把开放表面默认为实体；
不保存 materialCount * layerCount * pixelCount 的 dense ownership 栈；
不在未验证情况下把现有 Stage 15 配置禁令直接放开；
不将支持伪彩色误作为生产 RGB 证据。
```

## 7. 与 MEMFLOW 的依赖

MATVOL-01..05 可先建立纯合同、独立 oracle 和非生产候选；生产接线 MATVOL-06..09 必须消费
MEMFLOW 的 owned layer/单层物化能力。若 MF-03B4/MF-04 尚未完成，MATVOL 不得重新建立全层
材质 Mask 栈来绕过依赖。生产路由只能在作业开始前显式选定，不允许中途回退。

## 8. 准入与回滚

生产准入必须同时满足：旧 Profile 逐层 TIFF hash 零漂移；新 Profile 材质 owner/RGB/W 逐层 oracle
零差异；Package/RIP strict 通过；取消或错误不发布半包；峰值内存满足 RasterMemoryBudget；真实资产
报告无未裁决开放表面或重叠。

任何 Gate 失败时关闭 `materialVolumePolicy`，保留现有 Legacy 顶面投影路径。新增字段必须默认关闭，
旧构建遇到新 Profile 应 fail closed，不允许降级成旧全实体 RGB 后继续生产。

## 9. 待确认输入

| 编号 | 输入 | 当前状态 | 阻塞 |
|---|---|---|---|
| MQ-01 | 绿色开放表面壳层物理厚度 | INPUT OPEN | MATVOL-04 生产准入 |
| MQ-02 | 绿色壳层是否覆盖浅桃色主体 | 推荐是，未回签 | overlap policy |
| MQ-03 | S3/S4 是否进入首批生产范围 | 推荐否 | MATVOL-09 扩展 |
| MQ-04 | 正式设备性能/内存预算 | INPUT OPEN | 最终 SLA |

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-20 | v1.1 | 追加 §2.1：MV-01 已机器化复核 §2 全部 `03.obj` 事实并逐项吻合；登记同目录新增的 `08.obj`/`09.obj` 变体（材质 02 开放边 16826，顶点未焊接），裁定其不得替代 `03.obj` 作为封闭材质基线、仅作负例；补录 MQ-02 的 Z 范围几何证据但不代替用户回签。 |
| 2026-08-20 | v1.0 | 创建 MATVOL 专项，冻结非静默接入、开放表面、按需补白、MEMFLOW 与回滚边界。 |
