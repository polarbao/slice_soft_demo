# TASKS-01 单材料缩裹专项任务清单

> 目录：`docs/plan-feature/单材料缩裹整模识别与整版多实例/` ｜ 日期：2026-09-20 ｜ 专项：**MONOWRAP**
> 状态：**TASKS-01 全部完成**（2026-09-20）；后续见 [TASKS-02](TASKS-02-后续任务.md) 与 [TASKS-03](TASKS-03-缩裹整版多实例.md)
> 索引：[README](README.md)
> 开发分支：`feature/claude-monowrap-whole-model-transfer`（已合并删除）、
> `feature/claude-monowrap-multiinstance`（分叉自 `feature/main`，MONOWRAP-10/11 在此收口）
> 方案阶段分支（已合并并删除）：`feature/claude-monowrap-single-material-transfer`、
> `feature/claude-monowrap-gate-answers`
> 上下文：[ANALYSIS-01](ANALYSIS-01-现状与改动面.md)
> 四问比较：`DECISION-01-开工门四问的裁定与依据.md`（2026-09-20 追加）
> 授权：用户 2026-09-20 提出本专项并要求
> 「可先创建相关任务清单、任务方案，补齐上下文文档数据，后续进行开发时可按照相关方案及任务进行处理」。
> ~~本次只交方案与上下文，未写任何生产代码。~~（该行为立项当日状态，其后已实现并验证。）

---

## 0. 一句话说清要做什么

`model/obj/alg_suoguo/` 下的缩裹资产是**纯几何 obj、零材质引用**，
而 T 通道（缩裹）只能「按漫反射 RGB 精确匹配材质」，无 `.mtl` 时必然解析为空。
**要补的是 T 通道缺失的「整模模式」——W 和 V 早就有 `all_model`，只有 T 没有。**

---

## 1. 开工门（开工前必须先答的四问）

这四问的答案会改变方案形状，**没答之前不要动代码**。
它们来自上下文文档 §4，逐条列在这里作为开工检查表。

**2026-09-20 已完成比较**，详见四问比较文档。结论摘要：

| 编号 | 问题 | 结论 | 待裁定 |
|---|---|---|---|
| **G-1** | 下游是否依赖 `materialName` / `diffuseRgb` | `diffuseRgb` **否**（只进报告）；`materialName` **是**，且 `MaterialVolumePlan.cpp:144` 对空材质名**直接抛错**——这是上一版盘点漏看的真正阻碍。建议**方案 B：T 通道内合成材质名**，不动 MATVOL 的 fail-closed | 建议即可 |
| **G-2** | schema 同版本放宽 vs 升 `.2` | 建议**同版本放宽**。两者实质差别仅是多一个枚举值，不值得一个新契约版本；双维护正是 F-53 那类漂移的温床 | **需裁定** |
| **G-3** | 整盘一次切 vs 逐件 | 实测 10 件分两排、XY 重叠 0 对、整盘 174×58.2×5.6mm——**已排好版**。建议**整盘一次切**；场景入口是否接通 T 通道需先实测 | **需确认用法** |
| **G-4** | 整模模式下 `value` 取什么 | **此问不成立**。`RgbwsvtProtocol.cpp:66` 把它锁死为 0（`black_is_print`），schema 的 `"const": 0` 是对的。把它列为开工门是盘点时的错误 | 否 |

> **G-1 与 G-2 有耦合**：若 G-1 取方案 B，合成名让 `materialName` 有确定取值，
> G-2 的改动面同步缩小到只剩 `matchSource` 的 `const` 与颜色列表的 `minItems` 两处。
> **建议一起定。**

---

## 2. 任务表

| 编号 | 任务 | 依赖 | 状态 |
|---|---|---|---|
| MONOWRAP-00 | 现状调研与改动面盘点，产出上下文文档 | — | ✅ **完成**（2026-09-20） |
| MONOWRAP-01 | 回答 G-1：读通链路并给方案 | 00 | ✅ **完成**（2026-09-20，建议方案 B） |
| MONOWRAP-01b | 实测 `--scene-config` 入口是否接通 T 通道 | 01 | ✅ **完成**：接通。`p0.rgbwsvt.1` 的生产路径 `RunTransferProductionEntry` **本身就读场景有效配置**，场景制是设计正路 |
| MONOWRAP-02 | 回答 G-2：schema 两方案的改动清单与风险 | 00 | ✅ **完成**（建议同版本放宽，已裁定采纳） |
| MONOWRAP-03 | 实测基线影响：既有工艺报告形状不变、738 产物不受影响 | 00 | ✅ **完成**：738 产物逐字节一致，见 §3.9 |
| MONOWRAP-04 | 配置层：放行 `whole_model`，该值下**要求**颜色列表为空 | 01,02 | ✅ **完成** |
| MONOWRAP-05 | 解析层：`whole_model` 跳过颜色匹配，返回合成名命中 | 04 | ✅ **完成**（**重新设计**，见 §3.5） |
| MONOWRAP-06 | 报告层：整模模式下的字段取值 | 02,05 | ✅ **完成**，实产报告已通过放宽后的 schema |
| MONOWRAP-07 | 契约：`const`→`enum`，`minItems` 改为按 `matchSource` 条件约束 | 02 | ✅ **完成**，五种形态证伪全部符合预期 |
| MONOWRAP-08 | 新建 CLI 工艺 `samples/configs/matvol_t/monowrap_whole_model_rgbwsvt.json` | 03,06 | ✅ **完成**（放在 `process_profiles/` **之外**，理由见 ANALYSIS-01 §4.5） |
| MONOWRAP-09 | 目标目录 10 个 obj 实跑 | 08 | ✅ **完成，10/10 通过**，每件 `transferPrintPixels` 精确等于 `modelPixels` |
| MONOWRAP-10 | 单测与契约门禁：整模模式的正例 + **反例** | 05,07 | ✅ **完成**，4 条用例经故障注入证伪，见 §3.8 |
| MONOWRAP-11 | 全量回归 + 字节级基线，失败集合与基线比对 | 09,10 | ✅ **完成**（2026-09-20），见 §3.9 |

**后续任务（UI 预设、配置整合、标签栏交互）见 [TASKS-02](TASKS-02-后续任务.md)。**

---

## 3. 依赖方向（一条容易搞反的地方）

**用户问题 ①（新建工艺文件）依赖问题 ②（整模识别）**，不是并列关系。

在 MONOWRAP-04/05/07 落地之前，即使把 `nail_transfer_only_rgbwsvt.json` 写出来，
跑这批模型也只会得到**空的 T 通道**——因为没有 `.mtl` 供颜色匹配。
**不要先交一个工艺文件当作问题 ① 已解决**，那是假交付。

工艺文件本身（MONOWRAP-08）是最后一步，且必须由 MONOWRAP-09 的实跑证明它产出非空 T 通道。

---

## 3.5 一次关键的方向更正：曾经只有 5/10 能切

初版实现让整模缩裹走了 `BuildMaterialVolumePlan`，于是 10 件资产里只有 5 件能切：

| 初版结果 | 件数 | 明细 |
|---|---|---|
| 切片成功 | 5 | 101、104、105、201、204 |
| 自交超限（容限 64 对） | 4 | 203(68)、205(136)、202(171)、102(172) |
| 非流形 | 1 | 103 |

当时我准备把它当成「资产质量问题 vs 容限偏紧」交用户裁定。
**用户指出「之前的单材料光油工艺能正常切这批模型」，这个观察推翻了那个方向。**

查下来 `src/slicer_core/materials/SliceMaterialTexture.cpp:426-428`：

```cpp
if (config.material_policy.varnish.enabled) {
    if (config.material_policy.varnish.mode == "all_model") {
        pixel.v = config.material_policy.varnish.value;
    }
```

**光油的整模模式是在已栅格化的模型像素上逐像素赋值，根本不经体积求解**，
所以不要求流形、不查自交。而「整模即缩裹材料」与它语义同构——
`transferMask` 就应该等于 `modelMask`，求任何体积区间都是多余的。

**改法**：整模模式不建 volume，`MaterializeTransferLayerMask` 直接照搬 `modelMask`。
这比初版**更小**：`MaterialVolumePlan` 与 `MaterialTopologyClassifier` 的改动全部撤回
（合成材质名当初只是为了骗过体积求解的空材质名检查），合成名降级为纯报告标签。

**结果 10/10 全部切通**：

```text
101 T=4554685  102 T=2287198  103 T=1383582  104 T=841902   105 T=700168
201 T=1538079  202 T=3317260  203 T=897986   204 T=640161   205 T=1107332
```

包括原先非流形的 103 与四件自交超限的。**拓扑裁定随之消失，不需要抬任何容限。**

**教训**：看到「A 能做而 B 不能」时，先比两者的实现路径，而不是先调 B 的参数。
**参数是症状，路径才是原因。**

---

## 3.8 MONOWRAP-10 的落地与证伪（2026-09-20）

整模路径此前**没有任何单测**——`matvol_transfer_resolver_unit_tests` 与
`matvol_transfer_volume_plan_tests` 两个早就存在的 ctest 目标里，
`whole` 的命中数都是 0。唯一护栏是 `HostTransferProfileTests.cpp:381`，
而它守的是**预设形状**（RGB/白墨/光油必须全关），不是算法。

### 新增 4 条用例

| 文件 | 用例 | 守什么 |
| --- | --- | --- |
| `TransferMaterialVolumePlanTests.cpp` | `whole_model_verbatim_mask` | 整模掩膜**逐像素等于**模型掩膜；且 `wholeModel` 已置位、`volume` 为空 |
| 同上 | `whole_model_covers_colour_miss` | 同网格同掩膜下，颜色匹配落空而整模全覆盖——证明该分支**确实改变结果** |
| 同上 | `whole_model_colours_fail_closed` | 整模配了颜色 = 配置矛盾，必须 `ConfigInvalid` |
| `MatvolTransferResolverTests.cpp` | `whole_model_ignores_material_table` | 空材质表与有材质表结果一致，即「整模不看表」 |

**两处刻意设计**：

1. 正例用的是**混合掩膜 `{1,0,1,1}`**。若用全 1 掩膜，「逐像素照搬」与
   「无脑全写 1」两种实现无法区分，测试会变成空转。
2. 每条正例都**先断言 `plan.material.wholeModel` 与 `!plan.volume.has_value()`**，
   即先钉死被测分支真的触发，再比对结果。

### 证伪实测

把 `MaterializeTransferLayerMask` 的整模分支改成无脑全写 1 后重编：

```
FAIL whole-model mask copies the model mask pixel for pixel
CASE FAILED whole_model_verbatim_mask
FAIL whole_model covers exactly what colour matching misses
CASE FAILED whole_model_covers_colour_miss
```

3 条里 2 条变红、文案精确指向真因；还原后两个目标 2/2 全绿，
源码 `git diff` 为空（确认还原彻底，没留注入痕迹）。

---

## 3.9 MONOWRAP-11 的两半证据（2026-09-20）

分支 `feature/claude-monowrap-multiinstance`，起点是并入 codex ripflow 之后的
`feature/main`（`89672f5e`）。按 AGENTS.md 8b **先全量重建**（9 分 52 秒，0 错误），
再跑全量档，最后跑字节级基线。

### 前一半：全量档失败集合

```text
99% tests passed, 4 tests failed out of 274
Total Test time (real) = 1201.41 sec
	 73 - stage14f03_single_model_s1_gate
	143 - stage16c06_bounded_support_shape_unit_tests
	188 - scene_layer_adapters_unit_tests
	232 - slicer_stage14e02_qt_host_boundary_test
```

**失败集合 == 基线 4 项，无未归因新失败**，三项已登记抖动项本次一项都没触发。

两条附带确认：

- **F-53 修复在全量档站得住**。`slicer_stage14d07_r2_engine_conformance_test`、
  `slicer_stage14e04b_capability_coverage_test`、`hostflow_ha03_qt_end_to_end`
  三项全绿——它们上次全量档还是红的，已从抖动项表移出，再红即属未归因。
- `stage14f05_local_closure_gate` 的 Skipped **是设计行为**：
  该门禁只验 Release 分发包，Debug 下以 `SKIP_RETURN_CODE 111` 退出。

### 后一半：字节级基线

```text
字节级比对：PASS（11 个用例 / 738 个产物逐字节一致）
```

这一半才是整模缩裹真正要过的关。MONOWRAP 动过
`TransferMaterialVolumePlan.cpp` 这种**产出路径上的文件**，
快集档绿只说明没有测试变红，**逐字节一致才说明既有 11 例产出一个字节都没变**。
MONOWRAP-03 与 [TASKS-02](TASKS-02-后续任务.md) 的 UI-01e 一直挂着 🟡 等的就是这个。

### 顺带修正：三档闸门的项数全部陈旧

规范与 `AGENTS.md` 记的是 core 155 / fast 262 / full 270，实测为
**157 / 266 / 274**，已一并订正。差额里 `ripflow_module_source_pin_test`
一项是 codex 本批加的，其余是本仓早先几个专项加完没回写。

> 这不是格式问题：本仓的回归判据是**失败集合比对**，
> 而集合比对的前提是知道总数对不对——**总数写错会让「少跑了一批」
> 看起来和「全跑了」一模一样**。

---

## 4. 验证要求

1. **正例**：目标目录 10 个 obj 用新工艺跑出 T 通道**非空**的包，逐层核对覆盖范围与模型一致。
2. **反例（必须有）**：新增的整模模式不得让**既有**按颜色匹配的工艺行为改变——
   须证明既有 rgbwsvt 工艺的报告与产物**逐字节不变**。
3. **门禁须先被证伪**：新增单测/契约门禁要先人为破坏被测分支、确认它变红。
   本仓有过「对拍测试整条空转」的先例，**全绿不等于验过**。
4. **失败集合比对**：按 `AGENTS.md` 8c 比集合不比数量。当前基线稳定失败为 4 项。
5. **闸门**：合入 `develop/packaged-slicer` 跑快集档；合入 `main` 跑全量档 + 字节级基线。

---

## 5. 不在本专项范围内

| 项 | 理由 |
|---|---|
| 给 `MaterialRole` 增加「缩裹」角色 | `TransferMaterialResolver.h` 明确写明材质名**永不**用作 transfer 角色配置，那是刻意的设计决定。推翻它须另立专项并出授权文档 |
| 改动 MATVOL_T 的未完成项 | 本专项改它的配置面与报告契约，但**不在 merge 里替它承接债务** |
| 缩裹算法本身 | 本专项只解决「整个模型被识别为缩裹材料」，不碰缩裹几何算法 |

---

## 6. 变更记录

- **2026-09-20**：立项。完成 MONOWRAP-00：实测目标目录为纯几何零材质、
  定位到「T 通道缺整模模式而 W/V 已有」这一核心认识、盘点改动面与契约风险。
  顺带更正了缘起里「按 mtl 素材命名识别」的表述——实际是按漫反射 RGB 匹配，
  按名字那条路径（`map_input_material_to_role`）的角色集里根本没有缩裹。
