# REPORT_MATVOL 多材质纵深体积 RGB 当前状态

> 文档状态：**PREPARATION COMPLETE / IMPLEMENTATION NOT STARTED**
> 版本：v1.0 ｜ 日期：2026-08-20
> 任务真源：`../../codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`

## 1. 当前结论

MATVOL 专项文档和任务拆分已建立，MV-00 完成、MV-01 可在用户明确授权后执行。生产切片代码、
Profile、协议和默认路由均未因本次文档任务改变。

## 2. 已确认事实

```text
03.mtl 的两个颜色为绿色 [63,190,126] 与浅桃色 [255,220,198]；
工作区表面显示与 MTL 一致；
结果页全通道组合会叠加绿色 S 伪彩色；
Legacy 每列只保存 top_triangle_index，不能表达 Z 向材质变化；
03.obj 材质 01 为开放表面，材质 02 为闭合子网格；
现有 role mapping/顶面投影不足以让浅桃色进入纵深层；
Stage 15 white_underbase 当前拒绝 roleMapping/materialPolicy 组合；
根治必须使用显式新 Profile、逐层 owner、最终 RGB 后白区载体和 bounded 内存。
```

## 3. 当前未完成

```text
未创建 MATVOL C++ DTO/provider；
未创建 synthetic fixtures/oracle；
未修改 Host Profile/UI；
未接 Stage 15 或 MEMFLOW 生产路径；
未运行 MATVOL 构建、CTest、Package、RIP 或 Release 性能 Gate；
MQ-01 壳层厚度与 MQ-02 overlap 优先级未回签。
```

## 4. 下一张卡

`MV-01`：固化资产事实、synthetic fixture、旧顶面投影 baseline 和独立逐层 owner oracle。该卡不改
生产语义；仍需用户点名后方可开工。

## 5. 时间口径

任务清单不以预计日期代替 Gate。参考工程投入：封闭资产生产级方案约 5–8 个集中工作日；需要原生
开放表面壳层约 8–12 个工作日；若包含未完成 MEMFLOW 生产依赖，整体约 10–15 个集中工作日。
实际完成时间以每卡构建/回归结果和 INPUT OPEN 关闭时间为准。

## 6. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-20 | v1.0 | 建立专项初始状态，区分已确认、未完成、下一卡和时间口径。 |
