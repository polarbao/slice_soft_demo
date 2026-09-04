# TASKS_ADMISSION 组合准入规则单一真源收敛专项任务清单

> 文档状态：**PROPOSED / 未开工 —— 待用户裁定是否立项**
> 版本：v1.0 ｜ 日期：2026-09-04
> 定位：不占 Stage 编号的独立专项；本清单为该专项任务状态唯一真源
> 缘起：PRESET 专项 PC-06。用户 2026-09-03 问「常用工艺预设是否可统一合并」，
> 分析结论是**预设本身不是问题**，问题在「哪些组合合法」这份知识散落三处
> 上游：`TASKS_PRESET_工艺可配置面收敛与自测用例收口专项任务清单.md` §8（PC-06）

---

## 1. 问题陈述

「哪些配置组合合法」这一份知识，当前存在**三份互不同步的表达**：

```text
① src/slicer_core/config.cpp        29 条组合/互斥 throw —— 唯一权威
② apps/.../HostSliceSettings.cpp    宿主用中文重述，只覆盖 2 条
                                    （:311 按需补白组合门、:331 matvol + 角色映射）
③ apps/.../HostProcessPresetCatalog.cpp
                                    9 条基线预设 = 「已知可行组合」以数据形式再写一遍
```

②必然不全：核心 29 条，宿主只重述 2 条。**结构上必然存在「宿主校验通过、
切片期才被拒」的组合** —— 用户在 UI 上拼出一个组合，提交后才在切片侧吃到拒绝。

同时③也不是②的补集：预设是「已知好的组合」的白名单，它无法告诉用户
「为什么另一个组合不行」，也无法在用户手工改动面板后给出即时反馈。

⚠ 数量更正：PRESET 卡 §8 曾写「24 条」，那是用较窄的 grep 模式数出来的。
按错误文案含 `does not support` / `only supports` / `requires` / `must remain`
逐条抽取后为 **29 条**，清单见 §3。

---

## 2. 固定边界

```text
不改任何一条既有规则的语义 —— 本专项只改「这份知识存在哪里、由谁表达」。
不改 p0.rgbwsv.2 / p0.rgbwsvt.1、通道顺序、uint8 位深、black_is_print 极性。
不改任何既有工艺预设的 id、显示名、描述与字段。
不改 DefaultPresetId。
不放宽 SourceSizeGuard、14E-02、SHA256 工艺哈希中的任何一条。
⚠ config.cpp 已在 SourceSizeGuard 的 G2 白名单内（>1000 行只减不增）。
  本专项若继续往该文件加代码，必须在授权文档里说明；
  更好的做法是把谓词抽到独立文件，顺带减小 config.cpp。
⚠ 宿主目录不得出现切片库内部目标名（14E-02 按子串匹配，注释亦算），
  因此谓词若要被宿主复用，必须通过 contracts/ 或 SPI 边界暴露，
  不能让宿主直接 include 切片核心头文件。
```

---

## 3. 29 条组合准入规则清单（2026-09-04 实测）

| # | 位置 | 规则 |
|---|---|---|
| 1 | `config.cpp:732` | `global_surface_shell` mode requires matching texture and modelFill configuration |
| 2 | `config.cpp:757` | P0 only supports `autoOrient.strategy == minimize_height_by_right_angle_rotation` |
| 3 | `config.cpp:760` | P0 00B requires `background.value == 255` |
| 4 | `config.cpp:763` | P0 00B requires `output.bitDepth == 8` |
| 5 | `config.cpp:766` | P0 requires `output.planarConfig == contiguous` |
| 6 | `config.cpp:881` | 00C only supports `modelMaterial.applyMode == solid_volume` |
| 7 | `config.cpp:898` | `global_surface_shell` requires `modelFill.scope=complement_of_global_texture_shell` |
| 8 | `config.cpp:974` | materialClosure repair requires `modelFill.enabled=true` |
| 9 | `config.cpp:978` | materialClosure repair requires `support.enabled=true` |
| 10 | `config.cpp:1000` | `unprintableWhitePolicy=white_underbase` only supports the Legacy full-volume RGB texture path |
| 11 | `config.cpp:1006` | 同上 does not support the `global_surface_shell` pipeline |
| 12 | `config.cpp:1011` | 同上 does not support `materialPolicy.enabled=true` |
| 13 | `config.cpp:1016` | 同上 does not support `materialRoleMapping.enabled=true` |
| 14 | `config.cpp:1023` | 同上 only supports `experimental.openvdbPipeline engine=legacy with enabled=false` |
| 15 | `config.cpp:1043` | `texture.applyMode surface_shell_from_sdf` requires openvdbPipeline enabled with engine=openvdb |
| 16 | `config.cpp:1073` | 04 `texture.enabled` currently requires `relief_heightfield` |
| 17 | `config.cpp:1091` | `global_surface_shell` requires `modelFill.enabled=true` |
| 18 | `config.cpp:1231` | `materialRoleMapping role=support` requires `allowInputSupportMaterial=true` |
| 19 | `config.cpp:1270` | `materialVolumePolicy.openSurface.mode=surface_band` requires positive `thicknessMm` |
| 20 | `config.cpp:1285` | `overlap.mode=auto_by_material_name` requires an empty rules array |
| 21 | `config.cpp:1322` | `materialVolumePolicy` requires `slicingMode=relief_heightfield` |
| 22 | `config.cpp:1325` | `materialVolumePolicy` requires the Legacy slice pipeline |
| 23 | `config.cpp:1336` | `materialVolumePolicy` requires `geometrySampling.strategy=legacy_center_sample` |
| 24 | `config.cpp:1339` | `materialVolumePolicy` does not support `materialPolicy.enabled=true` |
| 25 | `config.cpp:1342` | `materialVolumePolicy` does not support `materialRoleMapping.enabled=true` |
| 26 | `config.cpp:1409` | preview transfer channel requires `p0.rgbwsvt.1` |
| 27 | `config.cpp:1437` | `writeProductionRgbwsv` requires `enabled=true` and `engine=openvdb` |
| 28 | `config.cpp:1473` | `writeProductionRgbwsv` must remain gated by production admission policy |
| 29 | `config.cpp:1479` | `writeProductionRgbwsv` requires `admissionMode=strict_closed`（不得在 diagnostic_only / warn_and_attempt / repair_then_strict 下运行） |

**观察：** 这 29 条里，`white_underbase`（#10–14）与 `materialVolumePolicy`（#19–25）
两簇各占 5 与 7 条，是互斥面最密的两处 —— 也正是宿主唯一重述的那两条所覆盖的区域。
换言之宿主重述的不是随机 2 条，而是最痛的两簇的入口；剩下 22 条从未在 UI 侧表达过。

---

## 4. 目标形态

```text
一个可查询的准入谓词，作为「哪些组合合法」的唯一表达：
  Admit(config) -> { ok | rejected(ruleId, 稳定错误码, 人读原因) }

由它派生：
  切片侧  config.cpp 的 29 条 throw 改为调用该谓词（语义逐条不变）
  宿主侧  面板改为查询谓词并【置灰】不可达选项，而不是各自用中文重述
  预设    退化为「该谓词的若干命名解」，而不是独立维护的白名单
```

**为什么置灰比事后拒绝重要：** 现状是用户可以在 UI 上自由拼出一个组合、
点提交、然后在切片期吃到一条中文拒绝。谓词化之后，不可达组合在选择时即不可选，
且能说明是被哪条规则挡住的。

---

## 5. 任务拆分（草案，未开工）

| 卡号 | 任务 | 前置 |
|---|---|---|
| A-01 | 29 条规则逐条建模：ruleId、稳定错误码、涉及字段集、人读原因 | 本卡获批 |
| A-02 | 谓词抽到独立文件（不再往 config.cpp 加行），`config.cpp` 改为调用 | A-01 |
| A-03 | 谓词的可达组合自测：对维度笛卡尔积跑谓词，固化可达集合规模 | A-02 |
| A-04 | 通过 contracts/ 或 SPI 边界把谓词暴露给宿主（不得让宿主 include 切片核心） | A-02 |
| A-05 | 宿主面板改为查询谓词并置灰；删除 `HostSliceSettings.cpp` 的 2 处中文重述 | A-04 |
| A-06 | 预设目录改为「谓词的命名解」，并加一条门禁：每条预设必须被谓词判为可达 | A-05 |

**A-03 的附带价值：** 一旦能对笛卡尔积跑谓词，就能第一次说清
「这套配置到底有多少个可达工艺」—— 这正是用户最初那一问的定量答案，
而现在没有任何人能回答它。

---

## 6. 开工前置与待裁定

```text
待裁定 ① 是否立项。本专项触及切片侧校验、SPI 边界与宿主 UI 三处，
         规模明显大于 PRESET 专项的任何一张卡，不宜塞进 PRESET。
待裁定 ② A-02 的落点：抽到独立文件（顺带减小 config.cpp，对 G2 有利），
         还是留在 config.cpp（需在授权文档里说明 G2 白名单继续增长）。
待裁定 ③ A-05 是否允许改变 UI 行为。置灰会让某些当前「可选但会被拒」的组合
         变成「不可选」，这是用户可见变化，需明确授权。
前置     PC-05（支撑 mode/placement 双表达）建议先于本专项收口 —— 否则
         A-01 建模时会遇到「同一维度有两套表达」的问题，等于把债建进谓词里。
```

---

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-04 | v1.0 | 首版。由 PRESET PC-06 拆出立卡。更正规则条数 24→29 并逐条列出位置与文案；记明三份表达的现状与宿主只重述 2 条的事实；给出谓词目标形态、A-01..A-06 任务拆分、三项待裁定与「PC-05 应先收口」的前置。未开工。 |
