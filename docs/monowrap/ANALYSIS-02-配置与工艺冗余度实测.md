# ANALYSIS-02 配置与工艺冗余度实测（CFG-01）

> 目录：`docs/monowrap/` ｜ 日期：2026-09-20 ｜ 专项：**MONOWRAP**
> 索引：[README](README.md) ｜ 任务：[TASKS-02](TASKS-02-后续任务.md) 的 CFG-01
> 证据等级：本文全部为 **A 级**（当前代码/文件实测），逐条标注来源。
> 状态：**调研完成，整合方案待裁定。本文不含任何已执行的删并。**

---

## 0. 一处先行更正

我此前说「宿主从 `samples/configs/matvol_t/process_profiles/` 读工艺」——**不准确**。
`HostTransferProcessPresetLoader.cpp:16-24` 的默认目录是 **exe 同级的 `configs/material_process`**
（可被环境变量 `SLICESOFT_PROCESS_PROFILE_DIR` 覆盖），该目录由 CMake POST_BUILD
从 `samples/configs/matvol_t/process_profiles` 整目录拷贝而来，拷贝点有 5 处：
`apps/slicer_ui_host_sim/CMakeLists.txt:310/914/1029/1112` 与 `scripts/PrepareSliceSoftRuntime.ps1:548`。

**一处命名陷阱**：源码树里另有 `samples/configs/material_process/`（16 个 6 通道文件），
与部署目录同名但**不同源**。

---

## 1. 10 份部署 T 工艺的冗余度

### 1.1 `transferChannelPolicy` 逐字段完全相同

**10 份的该子树 distinct 变体数 = 1。** 共同取值：
`matchSource=material_diffuse_rgb`、`materialDiffuseRgbValues=[[255,220,198],[255,255,0],[255,219,198]]`、
`missingRegion=allow_empty`、`multipleMatches=fail_closed`、`value=0`、
`topology={tolerate_closed_self_intersection, 64, 8}`。

而 `LoadDeployedPolicy`（`HostTransferProcessPresetLoader.cpp:87-103`）按 `QDir::Name` 排序
取第一个能加载的——**实际生效的永远是 `nail_rgb_white_varnish_top1_rgbwsvt.json`**，
其余 9 份在宿主侧从不参与。

### 1.2 宿主只读 11 个 key

`Load()`（`:111-187`）解析的字段：`output.packageProtocol`、`output.channelOrder`、
`transferChannelPolicy` 的 6 个、`topology` 的 3 个。

| | 数量 |
|---|---|
| 10 份文件的叶子字段合计 | **955** |
| 宿主实际读取 | **11**（且只读 1 份） |
| 利用率 | **1.2%** |

单份利用率在 9.2% ~ 14.1% 之间。

### 1.3 真正被执行的只有 3 份

10 份里被真跑过切片的只有 `obj_mtl_texture_rgb_only`、`nail_white_underbase_only`、
`nail_varnish_only`（`tests/matvol_t/LegacyRgbwsvtCliCandidateTests.cpp:133-138`、
`scripts/run_matvol_t_t08_gate.ps1:44`）。

**另外 7 份的非 T 块无任何执行消费方**，只被三处浅校验触及：
逐份 load 后断言两个字段（`MatvolTransferResolverTests.cpp:188-220`）、
目录文件数恰为 10（`HostTransferProfileTests.cpp:192-194`）、
10 份 T 策略一致（`:160-184`）。

### 1.4 每份 T 工艺是其 6 通道孪生件的机械克隆

与 `samples/configs/material_process/` 下同名原件逐对 diff：
**每一对差异恒为 14 个叶子字段，其中 13 个是固定 delta**
（`transferChannelPolicy` 的 9 个 + `packageProtocol` + `channelOrder` + `packageDir` + `_comment`），
**唯一的语义差异是 `materialProcessProfile.name`**。

⚠ **一个无防护的漂移面**：原件被 `HostTransferProfileTests.cpp:67-113` 的 15 条 SHA256 钉死，
**T 副本没有**。改了原件会红，改了副本不会。

---

## 2. UI 预设的差异维度

数据源 `apps/slicer_ui_host_sim/HostProcessPresetCatalog.cpp:63-246`。

**只差一两个开关的组**（这是整合的主要空间）：

| 组 | 成员 | 唯一差异 |
|---|---|---|
| **G1** | `rgb_only` vs `rgb_white_ondemand` | **仅 1 个枚举**：`texture.whitepolicy` = FailClosed / WhiteUnderbase |
| **G2** | `rgb_white` / `rgb_varnish` / `rgb_white_varnish` | **仅 1 个枚举**：`materialstrategy` |
| **G3** | `relief_white` vs `relief_varnish` | **仅 1 个枚举**：`materialstrategy` |

G1+G2+G3 合计 7 条预设，差异面加起来只有 **3 个枚举维度**。

**另有一处可读性缺陷**：`volumetric_nail_rgb_white_ondemand` 用 `MakeTexturedPreset` 构造
（内部置 `texture.enabled = true`，`:18`），随后在 `:168` 立刻改回 `false`。
构造器语义与实际取值自相矛盾。

---

## 3. `samples/configs/` 的 21 个目录

**「目录数多」不是问题所在**：21 个里 18 个是各司其职的测试夹具，各有明确的 owner 脚本
（引用处逐个查过）。真正的冗余在目录**内部**。

| 分类 | 数量 | 说明 |
|---|---|---|
| 生产工艺 | 2 | `material_process`、`matvol_t/process_profiles` |
| 测试夹具 | 18 | 全部有 CMake/测试/脚本引用 |
| **历史残留** | 1 | `matvol/material_volume_policy_disabled.json`——**零引用**，仅 docs 提及 |

---

## 4. 可整合的候选（待裁定，本专项不擅自执行）

| # | 整合什么 | 收益 | 风险 |
|---|---|---|---|
| **1** | 把 T 策略提为单文件，10 份完整工艺不再进部署包 | 每个 runtime 目录少 9 个文件（6 个 runtime × 9 = **54 份冗余拷贝**）；「谁生效」从依赖排序变为显式；消除 1.4 的单侧漂移面 | 要改加载器、5 处拷贝点；两条硬断言需重写 |
| **2** | 删 7 份无执行消费方的 T 副本 | 少 7 文件 / 约 660 个未被消费的叶子字段 | 三处循环断言要改；**未实测**删后行为不变 |
| **3** | G2 三条合并为「一条工艺 + 实体填充材料三选一」 | 下拉少 2 条 | `RgbWhiteVarnish` 唯一入口消失；workspace 按 id 持久化可能失配（**未验证**） |
| **4** | G1 合并为「纯白像素策略」开关 | 基线少 1 条，连带 T 派生少 1 条 → **13→11** | 涉及 `DefaultPresetId()`，改默认 id 波及所有按 id 解析的调用方 |
| **5** | 删 `matvol/material_volume_policy_disabled.json` | 目录 21→20 | 零引用，风险最低 |

**建议优先级**：5（零风险）→ 1（收益最大）→ 2 → 4 → 3。

---

## 5. 不建议动的

| 项 | 理由 |
|---|---|
| 18 个测试夹具目录 | 全部有明确 owner；按用例族分目录是正确组织，合并只会让脚本的路径列表更难读 |
| `material_process/` 的 15 份原件 | 被 SHA256 逐份钉死，且 `slicer_scenarios.json` 按路径在跑其中 7 条 |
| `volumetric` / `multilayer` 两条候选工艺 | `HostProcessPresetCatalog.cpp:178-191` 明确列出四处「缺一不可」的差异，并记录了 MO-11 误设 `texture=false` 的教训。合并会重新引入该缺陷 |
| G3（白墨 vs 光油单材料浮雕） | 虽只差一个枚举，但是面向用户的独立心智模型，下游工艺差异大；省 1 条换用户多一次点击，收益不成比例 |
| `transfereligible` 机制本身 | 它正是为修掉「硬编码配对漏写」而引入的（见 `HostProcessPresetCatalog.h:20-27`）。派生数量多不是机制问题 |

---

## 6. 标注为「未验证」的推断

1. 候选 2「删到只剩 3 份后行为不变」——基于策略一致性推导，未实际运行宿主验证。
2. 候选 3「workspace 按 preset id 持久化会失配」——未确认 `HostWorkspaceStateTests.cpp` 是否钉了具体 id。
3. `volumetric` 预设 `texture.enabled` 先真后假是否有意为之——未验证。
