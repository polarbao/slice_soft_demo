# TASKS-02 后续任务

> 目录：`docs/monowrap/` ｜ 日期：2026-09-20 ｜ 状态：**待开工**
> 索引：[README](README.md) ｜ 前序：[TASKS-01](TASKS-01-任务清单.md)
> 授权：用户 2026-09-20——「关于 UI 选项我给你足够的授权让你进行操作」；
> 「当前预设工艺以及工艺配置选项非常多，你可在完成基础任务后追加几条新的任务」。
>
> 本文三条任务的共同前提：**TASKS-01 的核心功能已收口**（整模缩裹 10/10 实跑通过）。
>
> ⚠ **UX-01 与 CFG-01 的范围超出「单材料缩裹」**，若后续体量变大，
> 应从 MONOWRAP 拆出独立专项，不要让本专项无限膨胀。

---

## UI-01 整模缩裹的 UI 预设（已授权）

### 为什么它不是「加个配置文件」

见 [ANALYSIS-01 §4.5](ANALYSIS-01-现状与改动面.md)：部署目录里的 `*_rgbwsvt.json`
**不是逐个可选的 UI 选项**，宿主只从中派生**唯一一条**共享 T 策略。

### 本轮新查明的约束（A 级）

「单材料缩裹」要求输出**只有 T 有内容**（R/G/B/W/V 全为 0）。
而宿主的材质策略是一个 C ABI 枚举，`apps/slicer_host_sim/HostRequestBuilder.h:9-17`：

```c
enum hostmaterialstrategy
{
    HOST_MATERIAL_RGB_SOLID = 0,
    HOST_MATERIAL_RGB_WHITE = 1,
    HOST_MATERIAL_RGB_VARNISH = 2,
    HOST_MATERIAL_RGB_WHITE_VARNISH = 3,
    HOST_MATERIAL_WHITE_SOLID = 4,
    HOST_MATERIAL_VARNISH_SOLID = 5
};
```

**六个取值没有一个是「只写 T」**。`HostMaterialStrategy::VarnishSolid`
（即用户说的「单材料光油」）会写 V，拿它做底会得到 V + T 而不是单 T。

**好消息是追加枚举值是向后兼容的**：既有调用方永远不会收到 `= 6`，
不属于破坏冻结 ABI。代价在于要把这条路径从宿主一路接到发射出的工艺配置
（`HostSliceSettings.cpp` 的映射 → `HostRequestBuilder.c` → Profile JSON），
使 `materialPolicy` 的 rgb/white/varnish 三项全关、`transferChannelPolicy` 走 `whole_model`。

### 任务

| 编号 | 任务 | 状态 |
|---|---|---|
| UI-01a | 读通映射链 | ✅ 完成：`HostMaterialProfile.c:225-262` 的 switch 决定 rgb/white/varnish 三个开关与 `materialChannel` |
| UI-01b | 追加 `HOST_MATERIAL_TRANSFER_SOLID = 6` 及映射 | ✅ 完成，**6 处登记**：C 枚举、C 侧映射、C++ 枚举、`ToHostMaterialStrategy`、`MaterialStrategyId`、`singleMaterialRelief` |
| UI-01c | 新增自带策略的预设 | ✅ 完成，id `single_material_transfer_wrap` |
| UI-01d | 验证产出只写 T | ✅ 完成，内容断言已加进 `HostTransferProfileTests.cpp` 并**经故障注入证伪** |
| UI-01e | 既有预设行为不变 | ✅ **完成**：738 产物逐字节一致，见 [TASKS-01](TASKS-01-任务清单.md) §3.9 |

> **过程中发现的第 5 处硬编码**：`HostTransferProfileBridge.cpp:16-30` 的
> `IsValidTransferSettings` 同样写死 `matchsource == material_diffuse_rgb` 与颜色列表非空。
> 加上核心配置校验、核心解析器、报告 schema、宿主工艺加载器，
> **同一个假设在本仓共 5 处独立硬编码**。这本身是一条值得记的架构债。
>
> **清点断言的处理方式**：原断言是「T 工艺数 == eligible 基线数」，
> 自带策略的预设按设计不满足它。没有简单把数字加一，而是**拆成两类**——
> 派生变体仍须等于 eligible 基线（保住对漏派生的检出），自带策略另计一条。

**不要做的**：不要为了让新工艺出现在 UI 而往 `configs/material_process/` 里放
`whole_model` 工艺——那会破坏「部署工艺策略必须一致」的不变量，
`matvol_t_host_profile` 会变红（本轮已实测撞过一次）。

---

## CFG-01 预设工艺与工艺配置的整合评估

用户观察：「当前预设工艺以及工艺配置选项非常多」。

### 已知的量（A 级，本轮顺带测得）

| 项 | 数量 |
|---|---|
| UI 预设工艺 | **13**（9 条基线 + 4 条派生的 T 变体） |
| 部署 T 工艺 JSON | **10**，且它们的 `transferChannelPolicy` **被要求完全一致** |
| `samples/configs/` 下的配置目录 | **21 个**（3mf / golden / material_closure / material_mapping / material_policy / material_process / matvol / matvol_t / obj_standard / openvdb / … ） |

**第 2 行本身就是一条整合线索**：10 份 JSON 携带同一份策略，
真正被使用的只有「第一个能加载成功的那份」——**其余 9 份在 T 策略这件事上是纯冗余**。
它们的差异在别处（`materialPolicy` / `materialProcessProfile`），但那部分宿主并不从这里读。

### 任务

| 编号 | 任务 | 状态 |
|---|---|---|
| CFG-01a~d | 调研与整合方案 | ✅ **完成**，见 [ANALYSIS-02](ANALYSIS-02-配置与工艺冗余度实测.md) |

**最刺眼的一个数**：10 份部署工艺共 955 个叶子字段，宿主只读 **11 个**，
且只读排序第一的那一份——**利用率 1.2%**。
五条整合候选已逐条给收益与风险，**待你裁定，本专项未执行任何删并**。

**纪律**：整合会改变用户可见的工艺列表，属产品决策。
**本专项只出方案与量化依据，不擅自删并**。

---

## UX-01 标签栏顺序与切片数据流的一致性

用户观察：「当前切片软件标签栏处理流程顺序与切片流程数据是否一致？是否可进行交互上的优化与改善？」

### 任务

| 编号 | 任务 | 状态 |
|---|---|---|
| UX-01a~d | 调研与改进方案 | ✅ **完成**，见 [ANALYSIS-03](ANALYSIS-03-标签栏与切片流程一致性实测.md) |

**结论**：标签栏是**两层** QTabWidget，核心 phase 与 UI 文案一一对应、无漂移。
判出 **4 条真实不一致**（最严重的是「导入落位设置在 tab 4，而导入动作在 tab 0」，
用户按标签顺序操作会发现勾选无效），另有 6 条看似不一致但实际合理、不应动。
五条改进建议已按代价排序，**待你裁定，本专项未执行任何 UI 改动**。

**判据**：标签栏顺序应当反映用户的**操作顺序**，而流水线顺序反映**数据依赖**。
两者不一定要一一对应——**先确认哪些不一致是设计使然、哪些是历史遗留**，
不要把「不一致」本身当成缺陷。

---

## 顺序建议

**UI-01 → CFG-01 → UX-01。**

UI-01 已授权且范围明确；CFG-01 的量化会为 UX-01 提供输入
（工艺少了，标签栏的信息密度问题也会跟着变）；UX-01 范围最大、最依赖前两项的结论。
