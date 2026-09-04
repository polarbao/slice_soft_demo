# DOC_DECISION 自测用例去别名与 T 通道派生收口授权范围

> 文档状态：**ACTIVE / AUTHORIZED（§1 已过 18:00 全量回归；§9 已过 Debug 定向回归）**
> 版本：v1.1 ｜ 日期：2026-09-03
> 授权：用户 2026-09-03 明确「如果你需要授权，则可给你足够的权限进行后续任务处理」，
> 并要求「先不进行需要 build + ctest 的相关操作，可先处理你能处理的相关选项」；
> 同日追加「PC-03、PC-04 根据你的建议执行」，该次追加解除了 build + ctest 的暂缓，见 §9
> 上游：本次对自测用例与常用工艺预设的两项分析（同日会话）
> 相关先例：`DOC_DECISION_MATVOL_MV_07_Q1_宿主白区门放宽授权.md`（授权文档形状）

---

## 0. 一处先行更正

分析阶段曾提出「部署目录里有 6 个 `*_rgbwsvt.json` 已发布但 UI 上无从选择」。
**该表述不成立，此处更正。**

逐文件比对 `samples/configs/matvol_t/process_profiles/` 下 10 个文件的
`transferChannelPolicy` 块，结果是**十份逐字节相同**：

```text
{"enabled":true,"matchSource":"material_diffuse_rgb",
 "materialDiffuseRgbValues":[[255,220,198],[255,255,0],[255,219,198]],
 "missingRegion":"allow_empty","multipleMatches":"fail_closed",
 "topology":{"maxBoundaryEdges":8,"maxSelfIntersectionPairs":64,
             "selfIntersectionPolicy":"tolerate_closed_self_intersection"},
 "value":0}
```

而宿主侧 `HostTransferProcessPresetLoader::Load` **只读这一个块**，文件里其余
约 170 行（旧工艺的 output / materialPolicy / materialProcessProfile / support …）
宿主一概不读。因此：

```text
✔ 这 10 个文件表达的是【同一条】T 策略的 10 份副本，不是 10 条不同工艺
✔ 「派生用哪个文件」对结果没有任何影响
✘ 那 6 个未被引用的文件并不承载任何 UI 缺失的语义 —— 不存在「6 条工艺不可达」
```

真正的问题因此不是「漏了 6 条工艺」，而是**派生方式**：把文件名硬编码进调用方，
是一种虚假的精确，且要求「新增基线工艺时记得补一次调用」。MO-11 时期按需补白
就是这么漏掉的（见 `HostProcessPresetCatalog.cpp` 内原注释）。本授权处理的是这个。

---

## 1. 授权做什么

### 1.1 摘除 9 个纯别名 CTest 条目

判定口径：**同一可执行 + 同一参数 + 无任何区分性 test property**。
按此口径扫描 `CMakeLists.txt` 与 `apps/slicer_ui_host_sim/CMakeLists.txt`，命中 4 组。

| 实际执行的二进制 | 保留 | 摘除 |
|---|---|---|
| `hostflow_hb05_slice_settings_tests` | `hostflow_hb05_slice_settings` | `hostflow_he03_support_settings`、`hostflow_he04_material_profile`、`hostflow_he05_texture_profile` |
| `hostflow_hb08_workspace_state_tests` | `hostflow_hb08_workspace_state` | `hostflow_he03_support_persistence`、`hostflow_he04_material_persistence`、`hostflow_he05_texture_persistence` |
| `stage14d06_public_worker_routing_tests` | 本名 | `stage14d05_r4b_public_worker_artifact_tests`、`stage14d04b_public_worker_cancellation_tests` |
| `hostflow_hb01_model_import_tests` | `hostflow_hb01_model_import` | `hostflow_he02_batch_import` |

**「不含自己的断言」不是推断，是观测结果。** 2026-09-03 11:48 的
`build-slicesoft/main/Testing/Temporary/LastTest.log` 里，
`hostflow_he05_texture_persistence` 执行的命令是
`.../hostflow_hb08_workspace_state_tests.exe`（无参），输出为
`HOSTFLOW_HE05_PERSISTENCE_PASS schema=8 runtimeHandles=persisted:false`；
而 `tests/hostflow/HostWorkspaceStateTests.cpp` 全文只在第 411 行打这一个 token。
四条 CTest 行的输出因此完全相同。

**任务卡证据重映射（本文件即为该映射的留痕）：**

```text
HE-02 批量导入        → hostflow_hb01_model_import
HE-03 支撑设置        → hostflow_hb05_slice_settings
HE-03 支撑持久化      → hostflow_hb08_workspace_state
HE-04 材料工艺/持久化 → hostflow_hb05_slice_settings / hostflow_hb08_workspace_state
HE-05 纹理工艺/持久化 → hostflow_hb05_slice_settings / hostflow_hb08_workspace_state
14D-04B 公共取消      → stage14d06_public_worker_routing_tests
14D-05-R4B 公共产物   → stage14d06_public_worker_routing_tests
```

摘除处均留有注释说明并回指本文件，未删除任何测试源码、任何断言、任何二进制目标。

**未摘除的一组（口径命中但不是重复）：**
`tiff_writer_alignment_conformance_unit_tests` 与
`tiff_writer_handwritten_alignment_known_failure_unit_tests` 命令逐字相同，但位于
`if(SLICESOFT_TIFF_BACKEND STREQUAL "libtiff")` 的 if / else 两支，互斥且后者带
`WILL_FAIL TRUE`。它是 handwritten 后端的已知缺陷桩，应随该后端退场，属 TIFF 专项，
**不在本授权内**。

### 1.2 T 通道派生改为策略单次加载 + 基线工艺自带资格位

```text
新增  hostprocesspreset::transfereligible（默认 false）
      —— 「要不要派生 T」成为基线工艺定义现场必须回答的问题
新增  HostTransferProcessPresetLoader::DeployedProfileFileNames()
新增  HostTransferProcessPresetLoader::LoadDeployedPolicy()
      —— 按文件名顺序取第一个可严格加载的 *_rgbwsvt.json；
         HasAnyDeployedProfile 改为它的薄封装，行为不变
改写  AppendTransferPreset：不再自己读文件，改接已加载的策略；
      入参改为按值传递（调用方传的是 presets 元素，而函数末尾向 presets 追加，
      按值先拷可使追加导致的重分配与入参无关）
删除  4 处「基线预设 + 工艺文件名」硬编码配对
```

派生资格标注为 `true` 的 4 条与改动前**逐条相同**：
`textured_nail_rgb_only_lower_support`、
`textured_nail_rgb_white_ondemand_lower_support`、
`single_material_relief_white`、
`single_material_relief_varnish`。

### 1.3 补上这次重构所依赖前提的门禁

`tests/matvol_t/HostTransferProfileTests.cpp` 新增两条断言：

```text
VerifyDeployedTransferPolicyCopiesAgree
    部署目录里全部可严格加载的 *_rgbwsvt.json 的 transferChannelPolicy 必须一致。
    §0 的「十份副本相同」是 §1.2「按文件名取第一个」的成立前提；一旦某份副本漂移，
    UI 上的 T 工艺就会随目录排序而变且不报错 —— 属静默故障，故必须有门禁。

transferPresetCount == eligibleBasePresetCount
    T 工艺条数必须等于标记 transfereligible 的基线工艺条数。
    原有的 transferPresetCount == 4 钉住条数不变，但漏派生只表现为数字对不上，
    看不出漏在哪一环；这一条把漏派生直接指名。
```

---

## 2. 授权【不】包括什么

```text
⛔ 不删除任何测试源文件、断言或二进制目标 —— §1.1 只摘 CTest 注册行
⛔ 不动 tiff handwritten 已知缺陷桩（属 TIFF 专项）
⛔ 不动 samples/configs/material_process/ 下任何工艺文件
   —— HostTransferProfileTests::VerifyLegacyProcessProfileHashes 钉住其中
      15 个文件的 SHA256，且 samples/scenarios/slicer_scenarios.json 按路径引用
      top1 / top3。参数化与 overlay 化必须在能跑回归时另行处理
⛔ 不为 rgbWhite / rgbVarnish 派生 T 变体
   —— rolemappingenabled=true 与 T 通道的组合未经 MATVOL-T 评审
⛔ 不为 volumetricRgb / multiLayerVarnish 派生 T 变体 —— MV-08 生产接线未完成
⛔ 不合并 W/V 对称的两对预设（需 UI 增加填充通道选择器，属工艺面改动）
⛔ 不收敛 volumetricRgb 与 multiLayerVarnish（需 MATVOL 裁定不走 -L 命名资产的去向）
⛔ 不改 materialPolicy / materialProcessProfile 的任何字段或校验
⛔ 不改任何既有工艺预设的 id、材质策略、纹理、支撑、materialvolume 字段
⛔ 不改 DefaultPresetId
⛔ 不放宽 SourceSizeGuard 与 14E-02 的任何阈值或白名单
```

---

## 3. 哪些工艺会受影响

### 3.1 8 条基线工艺：字段全部不变

`transfereligible` 是**新增字段**，仅参与预设目录内部的派生决策，不进入
`hostslicesettings`，不进入宿主发射的 Profile，因此不影响任何 profileHash。
派生出的 T 变体自身该位恒为 false。

| # | 预设 id | 字段 | 派生 T |
|---|---|---|---|
| 1 | `textured_nail_rgb_only_lower_support` | 不变 | 是（同改前） |
| 2 | `textured_nail_rgb_white_lower_support` | 不变 | 否（同改前） |
| 3 | `textured_nail_rgb_white_ondemand_lower_support`（默认） | 不变 | 是（同改前） |
| 4 | `textured_nail_rgb_varnish_lower_support` | 不变 | 否（同改前） |
| 5 | `single_material_relief_white` | 不变 | 是（同改前） |
| 6 | `single_material_relief_varnish` | 不变 | 是（同改前） |
| 7 | `volumetric_nail_rgb_white_ondemand_lower_support`（候选） | 不变 | 否（同改前） |
| 8 | `multilayer_transparent_varnish_lower_support`（候选） | 不变 | 否（同改前） |

### 3.2 4 条 T 变体：id、显示名、描述、T 策略均不变，**下拉顺序变化**

id 与显示名由 `AppendTransferPreset` 按同一规则拼出，未改。
T 策略取自部署目录，而 §0 已证十份副本相同，故取值与改前一致。

唯一的用户可见变化是**下拉列表里这 4 项的相对顺序**：

```text
改前（手写调用顺序）  rgbOnly → whiteOnly → varnishOnly → onDemandWhite
改后（基线工艺顺序）  rgbOnly → onDemandWhite → whiteOnly → varnishOnly
```

评估为可接受，依据是全仓无一处依赖预设顺序：`HostSliceSettingsPanel` 一律用
`findData(id)` / `Resolve(id)` 定位，`HostSliceSettingsTests` 与
`HostWorkspaceStateTests` 亦全部按 id 断言（如 `onDemandIndex = findData(...)`、
`single_material_relief_white_rgbwsvt`），持久化存的也是 id 而非索引。
改后顺序与基线工艺一致，可读性反而更好。

---

## 4. 哪些模型会受影响

```text
没有任何模型受影响。

§1.1 只摘 CTest 注册行，不触碰切片语义。
§1.2 派生出的 4 条 T 工艺其 packageProtocol、channelOrder、transferChannelPolicy
     与改前逐字段相同，因此对任一模型的切片产出、通道内容与 profileHash 均不变。
§1.3 只新增断言。
```

---

## 5. 后续会有什么变化

### 5.1 立即可见的变化

```text
① CTest 注册条目 243 → 234（generate 后实测，非估算；恰为 −9）
② 工艺下拉的条数、id、显示名、描述均不变，仅 4 条 T 变体的相对顺序按 §3.2 调整
③ 新增基线工艺时，必须在其定义处显式回答 transfereligible，否则不派生 T；
   漏派生会被 §1.3 的第二条断言指名，而不再是 UI 上的静默缺失
④ 部署目录里 T 工艺副本一旦漂移，matvol_t_host_profile 会指名报出是哪两个文件
```

### 5.2 尚不会发生的变化

```text
⛔ 常用工艺预设的结构性合并未做 —— W/V 对称两对、两条候选工艺的收敛都在 §2 之外
⛔ 工艺配置文件的参数化与 overlay 化未做（top1/2/3 只差一个整数、
   *_regression 只差模型路径、*_rgbwsvt 是纯叠加层），受 §2 的 SHA256 冻结所阻
⛔ materialPolicy 与 materialProcessProfile 的交叉校验未做 —— 二者在 26 个工艺文件里
   平行重述同一意图，config.cpp 只逐块校验合法性、从不交叉比对，
   两者可静默不一致（产出按 policy 走，验收断言按 profile 判）。应单独立项
⛔ 8 个不在 CTest 内、只被 REPORT 文档当手工步骤引用的验证脚本仍留在中间态
⛔ 2026-09-03 10:57 的 LastTestsFailed.log 所列 7 项失败未处理
```

### 5.3 后续卡建议

```text
优先  查清 7 项失败与 4 项从未记录成功耗时的条目
      （scene_layer_adapters_unit_tests、stage14f03 / f05 门禁、
        hostflow_hd02_real_asset_matrix 累计 7 次运行 cost 恒为 0）
其次  materialPolicy / materialProcessProfile 交叉校验
其次  工艺配置 overlay 化（须与 VerifyLegacyProcessProfileHashes 的哈希基线同步更新）
待裁  W/V 对称预设合并；volumetricRgb 与 multiLayerVarnish 收敛（MATVOL 授权）
```

---

## 6. 回滚方式

```text
本授权涉及的全部改动可用 git revert 对应提交整体回退：
  9 条 CTest 注册行原样回来（其注释一并消失）；
  transfereligible 字段与两个 loader 方法消失，AppendTransferPreset 回到读文件的形态；
  两条新增断言消失。
既有 8 条基线工艺与 4 条 T 变体的字段在回滚前后均不变，故回滚不产生二次影响。
```

---

## 7. 门禁与验收

### 7.1 已执行

```text
✔ SourceSizeGuard --self-test：PASS
✔ SourceSizeGuard 全仓扫描：本次触碰的 3 个源文件均未命中任何规则
  （HostProcessPresetCatalog.cpp 238 / .h 57、
    HostTransferProcessPresetLoader.cpp 187 / .h 48、
    tests/matvol_t/HostTransferProfileTests.cpp 421，
    G1 ≤500、G3 ≤200 均满足）
✔ 14E-02 禁止子串（slicer_core / slicer_base / slicer_engine）：4 个宿主文件均无命中
✔ CMake generate 已在本次改动之上完整跑过一遍（由本会话之外的进程于 17:49 触发），
  未因悬空 set_tests_properties 名字报错，注册条目实测 234，9 条别名全部消失
```

### 7.2 未执行 —— 按用户 2026-09-03 指示暂不进行

```text
✘ cmake --build（Debug / Release /W4 /WX）—— §1.2 与 §1.3 的 C++ 改动【尚未编译】
✘ ctest 全量回归 —— 234 项的失败集尚未与改前比对
✘ 宿主 UI Smoke —— §3.2 的下拉顺序变化尚未在真实 UI 上确认
```

### 7.3 恢复回归后必须逐条确认

```text
必过  matvol_t_host_profile：含 §1.3 两条新增断言，且 transferPresetCount 仍为 4
必过  hostflow_hb05_slice_settings 与 hostflow_hb08_workspace_state 仍 PASS
      （HE-02/03/04/05 的证据已按 §1.1 重映射到它们）
必过  stage14d06_public_worker_routing_tests 仍 PASS
必过  VerifyPresetProfileHashClosure：8 条基线工艺逐条哈希闭合
必过  8 条基线工艺的 profileHash 与改前逐条相同（§3.1 的结构性保证需实测确认）
必过  Release /W4 /WX 零告警
必过  全量 CTest 失败集相对 2026-09-03 10:57 的 7 项无新增
已知  14E-02 在 HEAD 上已因 apps/slicer_ui_host_sim/HostMainWindow.cpp（502 行，
      未入债务台账）失败。本次未触碰该文件、未新增超限文件，故不改变其失败集
```

---

## 9. 追加授权（2026-09-03 同日，用户「PC-03、PC-04 根据你的建议执行」）

### 9.1 PC-03 新增第 9 条基线工艺 —— 纯增量

```text
新增  textured_nail_rgb_white_varnish_lower_support
      HostMaterialStrategy::RgbWhiteVarnish + TopSurfaceBand + rolemappingenabled=true
      字段按切片侧既有工艺 obj_mtl_texture_rgb_white_varnish.json 取
理由  RgbWhiteVarnish 是六个材质策略里唯一没有工艺预设入口的一个，
      而切片侧有 6 个 *rgb_white_varnish* 工艺文件被 SHA256 钉住、
      slicer_scenarios.json 也在跑它 —— UI 侧缺的是入口，不是能力
不改  既有 8 条基线工艺的任何字段；transfereligible=false 故 T 工艺仍为 4 条
实测  Debug 构建零编译器诊断；定向 4/4 PASS
```

### 9.2 PC-04 —— 本次唯一的**基线变更**，须重点留痕

```text
① 输入机制变更（非放宽）
   VerifyRealAssetMatrix 由 QDirIterator 递归扫 model/obj
   改为读仓库内清单 tests/hostflow/fixtures/render_ra02_asset_manifest.txt（31 条）。
   这是把不可复现的输入变成可复现的输入，不削弱任何断言。

② 冻结三元组重固化：22 / 0 / 14  →  29 / 0 / 2      ← 【基线变更】
   为何不能「修回去」：旧值系 2026-08-11（03b08bb）对 36 个资产测得，
   其中 5 个从未入库，任何人都无法从仓库重建，故只能重固化。
   ⚠ 该次重固化同时固化了一处【语义变化】：同一批 31 个资产里有 7 个
     从 asset-rejected 变为可渲染，即资产准入自 8 月 11 日起明显放宽
     （mesh repair / importer 侧的改进）。本授权承认并固化这一现状，
     但【不认定】该放宽本身是否正确 —— 若 RENDER 专项认为某些资产本应被拒，
     应另开卡追查，而不是回退本次重固化。

③ 聚合步骤取前 22 个可渲染资产
   22 = HostModelImportWorkflow::ImportModels 的场景实例上限，
   亦即 AGENTS.md 记载的「13B 尚未回签的 22-instance production budget」。
   本授权【明确不抬高】该预算，改为让用例用满预算而非用满资产。
   下游无任何冻结数字，故不触及其他期望。

④ 修一处实参求值顺序缺陷（失败时打空原因）
   纯可诊断性修复，不改断言语义。同类写法全仓另有 34 处，入 PC-12。
```

### 9.3 PC-04 明确**不包括**

```text
⛔ 不改 22 实例产品预算
⛔ 不改 TIMEOUT 900（余量仅 1.43 倍的风险已记入任务卡 §6.7，另行裁定）
⛔ 不缩减清单规模（属覆盖面决策，须 RENDER 专项裁定）
⛔ 不追查「7 个资产由拒转渲」这一准入放宽是否正确
```

### 9.4 回滚方式

```text
git revert 对应提交即可：清单文件消失、输入回到扫盘、三元组回到 22/0/14、
聚合步骤回到用满 renderedPaths、两处求值顺序回到原写法。
回滚后该用例会回到「每次回归都红且耗 558 秒」的原状。
```

---

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-03 | v1.1 | 追加 §9：同日用户「PC-03、PC-04 根据你的建议执行」的追加授权。PC-03 为纯增量（补第 9 条基线工艺 RgbWhiteVarnish）；PC-04 含本次唯一的基线变更 —— 冻结三元组 22/0/14→29/0/2，并明确该重固化同时固化了「7 个资产由拒转渲」的准入放宽现状但不认定其正确性；另记明不抬高 22 实例预算、不改 TIMEOUT、不缩减清单规模、不追查准入放宽，以及回滚方式。 |
| 2026-09-03 | v1.0 | 首版。§0 更正「6 条 T 工艺不可达」的错误表述并给出十份副本逐字节相同的证据；固化 §1.1 摘除 9 个纯别名 CTest 条目（含任务卡证据重映射与 LastTest.log 观测依据）与 §1.2 T 派生改策略单次加载 + 基线工艺自带资格位，§1.3 补上该重构所依赖前提的两条门禁；列明 9 项不在授权内的禁止事项、下拉顺序是唯一用户可见变化及其无依赖论证、回滚方式；明确 C++ 改动尚未编译、回归未跑，并给出恢复回归后必须逐条确认的清单。 |
