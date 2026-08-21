# REPORT_MATVOL 多材质纵深体积 RGB 当前状态

> 文档状态：**MV-00..MV-03、MV-05..MV-06 COMPLETE / MV-07 PREPARED / 生产路由未改**
> 版本：v2.0 ｜ 日期：2026-08-21
> 任务真源：`../../codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`

## 1. 当前结论

MATVOL 的**非生产语义栈已完整落地**：从逐材质拓扑分类、封闭材质有序交点与 compact 层区间、
caller-owned 单层 owner 物化、owner 到 RGB 合成，直到最终 RGB 之后复用 Stage 15 按需补白，
以及体积报告与其正式 JSON Schema。全部代码**未接 `run_slicer`**，旧 Profile、`p0.rgbwsv.2`、
RGBWSV 语义、SPI 与默认路由均未改变，既有预设的 `profileHash` 亦未变动。

本分支（`product/packaged-slicer`）现已可从干净检出直接构建全部五个 matvol 测试目标。

## 1.1 已完成卡与机器证据

| 卡 | 交付 | 定向 CTest |
|---|---|---|
| MV-01 | 资产事实、MV-F01..F06 合成 fixture、Legacy 顶面投影 baseline、owner diff schema | 11/11 |
| MV-02 | `E_MATVOL_*` 九码三件套、逐材质拓扑五态分类、`materialVolumePolicy` 配置块与迁移登记 | 8/8 |
| MV-03 | move-only `MaterialVolumePlan`、CSR compact 区间、单层 owner 物化 | 10/10 |
| MV-05 | 逐材质 RGB 解析与单层合成，只写 RGB | 5/5 |
| MV-06 | 复用 Stage 15 补白谓词、体积报告、首个正式报告 JSON Schema | 5/5 |

全量回归：Release 构建零错误，CTest 212 项失败 8 项，与本专项介入前失败集**完全相同且无新增**。

## 1.2 三处由变异检验发现并修正的缺陷

```text
① 材质交界边误判（MV-02）
   AdaptSceneModelToTriangleMesh 做全网格顶点焊接，材质交界边在子网格里必然退化为边界边。
   首版只按 boundaryEdgeCount 判 OpenSurface，会把任何焊接的多材质模型都误判为开放面。
   现改为全网格边度数表区分真开边与交界边并分列计数；仅由交界边围成的子网格同样不算
   独立闭合体（射线拿不到成对交点），一并 fail-closed。

② 物化参数校验静默无操作（MV-03）
   首版 Materialize 标 noexcept 且尺寸不符时静默返回，会让调用方拿到未初始化 buffer 而无信号。
   现与既有 MaterializeLayerOccupancy 一致，显式抛 std::invalid_argument。

③ 窄放行抢占既有禁令消息（MV-06）
   首版额外加了正向窄放行检查，它抢在既有 materialPolicy / roleMapping 禁令之前触发，
   替换掉原有错误消息。既有禁令本就覆盖这两项，该检查冗余且损害可审计性，已移除。
```

## 2. 已确认事实

```text
03.mtl 两个颜色为绿色 [63,190,126] 与浅桃色 [255,220,198]，量化值已机器复核；
03.obj 材质 01 为开放表面（1382 开放边），材质 02 为闭合子网格（0 开放边），已机器复核；
同目录 08/09 变体的材质 02 开放边 16826 且顶点未焊接，不能替代 03 作为闭合体基线，仅作负例；
两材质 Z 范围几乎完全重叠并共享同一顶面，形态为绿色开放面包裹浅桃色闭合主体；
Legacy 每列只保存 top_triangle_index，结构性无法表达 Z 向材质变化，已有机器证据；
Legacy 投影链路全在 slicer.cpp 匿名命名空间内，测试 TU 不可达；
结果页全通道组合会叠加绿色 S 伪彩色，不能用该组合判断 RGB 材质。
```

## 3. 当前未完成

```text
MV-04  开放表面 surface_band 候选 —— 卡在 MQ-01 壳层物理厚度与 MQ-02 覆盖优先级两项产品输入；
MV-07  参考宿主 Profile/UI/预检与 RGB-only 结果表达 —— 前置已满足，PREPARED；
MV-08  MEMFLOW bounded/owned 生产接线 —— 依赖 MF-03B4/MF-04，且该专项提交在
       codex/memflow-bounded-streaming 分支及其独立工作树，本分支尚无对应源文件；
MV-09  回归矩阵 —— 等 MV-07 与 MV-08；
MV-10  生产 opt-in 准入 —— 等 MV-09 与设备输入。

尚未做：接 run_slicer、改任何生产默认、Package/RIP 端到端、Release 性能与内存 Gate。
```

## 4. 下一张卡

`MV-07`：参考宿主新增显式「多材质纵深 RGB + 按需补白」候选，展示拓扑与冲突诊断，
结果页提供 RGB-only 判断入口并明确 S 为伪彩色。仍需用户点名后开工。

## 5. 时间口径

任务清单不以预计日期代替 Gate。MV-01..MV-06 的语义栈已在本轮完成；剩余工作量集中在
宿主 UI（MV-07）、生产接线（MV-08，受分支与 MEMFLOW 依赖制约）与回归矩阵（MV-09）。
实际完成时间以每卡构建与回归结果、以及 MQ-01/MQ-02 关闭时间为准。

## 6. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-21 | v2.0 | 推进至 MV-00..MV-03、MV-05..MV-06 COMPLETE：记录五张卡的交付与定向 CTest 结果、全量回归失败集无新增、三处由变异检验发现并修正的缺陷（材质交界边误判、物化静默无操作、窄放行抢占禁令消息）、以及本分支已可从干净检出构建全部 matvol 目标。补充 08/09 变体与 Z 范围重叠的机器复核结论，并明确 MV-08 受分支与 MEMFLOW 依赖制约。 |
| 2026-08-20 | v1.0 | 建立专项初始状态，区分已确认、未完成、下一卡和时间口径。 |
