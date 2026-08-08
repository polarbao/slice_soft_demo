# DOC_DECISION_HOSTFLOW_H-E-R1 参考宿主目标水位裁决（HQ-09 / HQ-10）

> 文档状态：**ACTIVE / DECISION RECORD**
> 版本：v1.0 ｜ 日期：2026-08-08
> 定位：记录用户对 HQ-09（目标水位）与 HQ-10（场景保存归属）的裁决及其执行分批
> 关联：`TASKS_HOSTFLOW` H-E 组、`REPORT_HOSTFLOW_H_C_02`、`CLD_04` §4.2
> 证据等级：A=已核实代码事实，B=目标设计，P=判断

---

## 1. 裁决结论

```text
HQ-09  参考宿主目标水位   → 【乙】等价于切片库封装前的切片软件，且【分批执行】
HQ-10  场景/项目保存加载   → 【甲】归 PrintApp，参考宿主不实现
```

裁决人：用户 ｜ 日期：2026-08-08

## 2. HQ-09 的背景与选项

**此前裁决**：用户曾定「HQ-03 优先覆盖核心流程」，H-B 的 8 张卡按该范围交付并已完成。

**本次发现（A）**：`docs/slice/REPORT/assets/hostflow_hc02_migration_plan.json`
中 8 项 `action = adapt_to_host_profile`，其 `replacement` 指向的宿主功能**当前并不存在**：

| 主干单元 | 计划替代物 | 宿主现状 |
|---|---|:--:|
| `widgets/SupportEditor.h` | host support Profile editor | ❌ |
| `widgets/MaterialProcessProfileEditor.h` | host material process Profile editor | ❌ |
| `widgets/MaterialRoleMappingEditor.h` | host material role Profile editor | ❌ |
| `widgets/MaterialPolicyEditor.h` | host material strategy editor | ❌ |
| `widgets/ProductionTextureSettingsPanel.h` | HostSliceSettingsPanel advanced Profile section | ❌ |
| `services/ProductionTextureSettingsContract.h` | host Profile schema and editor DTO | ❌ |
| `services/ProductionTextureSettingsModel.h` | HostSliceSettings model | ⚠️ 仅 DPI/层厚/输出目录/材料策略 |
| `services/SingleMaterialReliefResolver.h` | host Profile material strategy | ❌ |

核实方式：`apps/slicer_ui_host_sim/HostSliceSettings.h` 中
`support` / `texture` / 支撑 / 纹理 **零命中**。

另两项缺口：

```text
STL 导入      src/slicer_core/model.cpp:626 已支持 .stl（ASCII/binary）
              → 内核有能力，宿主无入口，属【纯 UI 缺口】
批量导入      主干有 controllers/SceneBatchImportController.h，宿主为单文件对话框
```

**选项**：

| 选项 | 范围 | 成本（P）|
|---|---|---|
| 甲 | 维持核心流程，H-D 完成即收口 | 约 3–5 人日 |
| **乙 ✅** | 提升到等价于封装前切片软件 | 额外约 10–16 人日 |

**采纳乙。** 依据：Stage 14 的目标是让打印软件参照参考宿主完成移植；
若参考宿主缺少支撑与材料工艺参数入口，打印侧移植时这部分**没有可参照的实现**，
只能自行设计，与「最少改动移植」冲突。

## 3. HQ-09 的执行分批

**分三批，按「用户可感知价值 / 单位成本」降序：**

### 批次 E1 · 立即价值（建议随 H-D 一并推进）

```text
H-E-01  STL 导入      内核已支持，纯 UI 缺口，成本极低
H-E-03  支撑参数编辑   lower_support 类 Profile 的实际在用参数
```

**选它们打头的理由（P）**：E1 两张卡直接决定「操作员能不能用这个宿主做出一次真实的活」。
`textured_nail_rgb_only_lower_support` 等 Profile 已在生产使用支撑，
而宿主连支撑参数入口都没有 —— 这是当前**最刺眼**的缺口。
STL 则是几乎零成本就能消除的「能力倒退」。

### 批次 E2 · 参数深度

```text
H-E-04  材料工艺 Profile 编辑（材料策略 / 角色映射 / 单材料浮雕）
H-E-05  生产纹理设置
```

E2 覆盖 8 项 `adapt_to_host_profile` 中剩余的主体。**E2 依赖 E1 的宿主 Profile 编辑框架** ——
H-E-03 会先建立「宿主 Profile 可编辑段」的结构，E2 在其上扩展，避免两套编辑范式。

### 批次 E3 · 广度与预检

```text
H-E-02  批量导入
H-E-06  纹理白区预检接入（Stage 15 15C 成果）
```

E3 是**便利性与安全网**，晚做不产生返工。H-E-06 依赖 E2 的纹理设置已就位。

### 批次门（B）

```text
E1 完成 → 复核一次实际可用性，再决定 E2 是否照原样执行
E2 完成 → 复核 8 项 adapt_to_host_profile 是否已全部有归宿
E3 完成 → 回填 REPORT_HOSTFLOW_H_C_03 的 known_trim 条目
```

> ⚠️ **每批完成后必须复核，不得三批一次性排完。**
> 理由：H-E-03 建立的 Profile 编辑框架形态会影响 E2 的实现方式；
> 若 E1 完成后发现框架需要调整，E2 尚未开工，返工成本为零。

## 4. HQ-10 裁决：场景/项目保存加载归 PrintApp

**现状（A）**：`H-B-08` 明确「不恢复 scene/job/model/cache 运行时身份」
→ 参考宿主重启后场景丢失，须重新导入。
主干有 `models/SceneDocument.h` 与 `widgets/ProjectToolsDock.h`。

**裁决：甲 —— 归 PrintApp，参考宿主不实现。**

依据：

```text
① CLD_04 §4.2 已把持久化与多任务明确划归 PrintApp，本裁决与既有分工一致
② 若参考宿主也实现工程格式，两侧会各存一份，日后必然产生格式权威之争
③ 该项不影响「一次切片作业能否完成」，不属于核心作业流程
```

**因此 `SceneDocument.h` / `ProjectToolsDock.h` 维持 H-C-01 的分类不变**
（`SceneDocument` 为 B 桶 `abi_scene`，由打印侧按 H-C-02 计划自行实现；
`ProjectToolsDock` 为 C 桶 `debug_only`，不移植）。

## 5. 不改变的边界

```text
✅ PM_SPI_VERSION = 1 不变        ✅ 11 个 pm_* 导出不变
✅ 15 项能力不变                  ✅ p0.rgbwsv.2 / 通道序 / 位深 / 极性不变
✅ 生产 TIFF 与 Package 不变      ✅ 主干 apps/slicer_debug_ui 不改
```

H-E 全组**不新增 ABI 面**：所有新增参数进入**宿主 Profile**，
经既有 `slice.rgbwsv` 的有效配置通道下发。
⛔ **不得改为让宿主读取内部 `slicer_scenarios.json` 或 `samples/configs/**`**
—— 那会推翻 HQ-08-A 已确立的「Profile 目录归宿主」。

## 6. 派生任务与状态

| 卡号 | 批次 | 状态 |
|---|:--:|---|
| H-E-01 STL 导入 | E1 | **AUTHORIZED / 待点名** |
| H-E-03 支撑参数编辑 | E1 | **AUTHORIZED / 待点名** |
| H-E-04 材料工艺 Profile 编辑 | E2 | AUTHORIZED / 待 E1 批次门 |
| H-E-05 生产纹理设置 | E2 | AUTHORIZED / 待 E1 批次门 |
| H-E-02 批量导入 | E3 | AUTHORIZED / 待 E2 批次门 |
| H-E-06 纹理白区预检接入 | E3 | AUTHORIZED / 待 E2 批次门 |

> 「AUTHORIZED」表示**范围已获批准**，不表示可自行开工。
> 按本项目纪律，仍须用户点名单张原子卡（例如「执行 H-E-01」）。

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-08 | v1.0 | 首版。记录 HQ-09=乙（等价水位，分三批）与 HQ-10=甲（场景保存归 PrintApp）的裁决、A 级证据、E1/E2/E3 分批依据与批次门、不新增 ABI 面的边界，并登记 6 张派生卡状态 |
