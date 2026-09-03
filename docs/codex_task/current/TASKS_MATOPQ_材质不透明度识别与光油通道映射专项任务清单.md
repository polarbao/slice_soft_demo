# TASKS_MATOPQ 材质不透明度识别与光油通道映射专项任务清单

> 文档状态：**ACTIVE / MO-00..11 全部 COMPLETE
> / MO-12 下层贴图取色缺口 BLOCKED 待授权 / K3 表决待重叠资产出现后落地**
> 版本：v1.3 ｜ 日期：2026-09-02
> 定位：不占 Stage 编号的独立材质外观专项；**本清单是任务状态的唯一真源**
> 分支：`codex/matopq-material-opacity`（分叉自 `product/packaged-slicer` @ ba391b4）
> 决策：`docs/slice/DOC/DOC_DECISION_MATOPQ_材质不透明度识别与光油通道映射专项.md`

## 1. 固定边界

```text
p0.rgbwsv.2、channelOrder R G B W S V、uint8、black_is_print、printValue 0、emptyValue 255 不变；
Model > Support > Empty 不变；Legacy 仍为默认；不启用 OpenVDB；
不新增 SPI 导出符号；不改 PM_SPI_VERSION；不改 DOC_SCHEMA_14 契约字段语义；
不放宽 config.cpp 内任何既有 fail-closed 互斥门；
slicer_core 不得引入 Qt 类型；
G2 行数门禁：model.cpp / slicer.cpp / config.cpp 只减不增；
MO-01..03 不改任何切片输出，验收口径为「切片产物逐字节不变」。
```

## 2. 状态表

| 卡号 | 任务 | 状态 | 依赖 | 完成日期 |
|---|---|---|---|---|
| MO-00 | 专项决策、任务清单、资产事实与缺口定位 | **COMPLETE** | 用户授权创建专项 | 2026-08-31 |
| MO-01 | MTL `d`/`Tr` 解析下沉与 `MaterialInfo.opacity` 承载 | **COMPLETE** | MO-00 | 2026-09-01 |
| MO-02 | ViewData `base_color` alpha 贯通 | **COMPLETE** | MO-01 | 2026-09-01 |
| MO-03 | CPU 光栅透明 pass：不写深度 + back-to-front + 预览 alpha 下限 | **COMPLETE** | MO-02 | 2026-09-01 |
| MO-04 | 不透明度 → 光油通道映射（MATVOL 路径） | **COMPLETE** | MO-01、方案 A | 2026-09-01 |
| MO-05 | 不变性与回归矩阵（opaque 零漂移 + 切片产物零漂移） | **COMPLETE** | MO-03 | 2026-09-01 |
| MO-06 | 材质名整行解析修复（`sg (1)` 截断） | **COMPLETE** | MO-01 | 2026-09-01 |
| MO-07 | 全仓冲突裁决与工艺逻辑策略盘点总表 | **COMPLETE** | 用户 2026-08-31 要求 | 2026-08-31 |
| MO-08 | 方案 A：退化面阈值可配（`degenerateAreaEpsilonMm2`） | **COMPLETE** | 用户 2026-09-01 授权 | 2026-09-01 |
| MO-09 | D-1：`slicer_cli --repair-asset` 资产修复子命令 | **COMPLETE** | 用户 2026-09-01 授权 | 2026-09-01 |
| MO-10 | V/T 同像素冲突 fail-closed 检测（K3 表决暂缓） | **COMPLETE** | MO-04 | 2026-09-01 |
| MO-11 | 宿主接线：ABI 四字段条件产出 + 自动模式预设 + Profile 哈希闭合 | **COMPLETE** | MO-04、MO-08 | 2026-09-02 |
| MO-12 | 下层材质贴图取色缺口（RGB 逐列顶面 vs MATVOL 纵深区间脱节） | **RESOLVED**（移交 MATOPQ-RGB） | MO-11 实测暴露 | 2026-09-02 |

## 3. MO-00 文档与事实（COMPLETE，2026-08-31）

**完成标准：** 决策文与任务清单互链；固化资产几何事实、四处缺口位置、契约现状与冻结边界；不改生产代码。

**实际结果：** 见决策文 §2/§3。关键结论两条：

```text
1  ViewData 契约 v1.5 已冻结 baseColorFactor[4] / alphaMode / alphaCutoff / doubleSided，
   且 SceneViewDataAdapter.cpp:190 已从 base_color[3] 自动推导 alphaMode。
   故 MO-01..03 是补齐既有冻结契约的实现缺口，不是契约变更。
2  真实缺口仅 4 处（GAP-1..4），不需要新建通道、DTO 字段或 SPI 符号。
```

## 4. MO-01 MTL 不透明度解析（COMPLETE，2026-09-01）

**目标：** 让 `d` / `Tr` 进入系统，并解决 G2 行数门禁冲突。

**实际实施：**

```text
新增 src/slicer_core/model/MtlMaterialParser.{h,cpp}
     （位置对齐既有 ObjFaceParser 先例：同为 model.cpp 私有解析助手，
       命名空间 slicer_core::model_detail；纯 STL，无 Qt，无文件 IO）
  - ApplyMtlMaterialLine()  统一处理 Kd / map_Kd / d / Tr
  - MtlMaterialLineResult.opacity_conflict  d 与 Tr 互不相容时回报，不让后者静默胜出
model.h    MaterialInfo 增加 opacity{1.0} 与 has_opacity{false}
model.cpp  trim_copy / kd_component_to_u8 / resolve_texture_path 与 load_mtl 行内实现一并下沉，
           只留委派；实测 1982 -> 1941 行，以【实际缩减】满足 G2 而非顶在上限
```

**行为等价性：** `newmtl` 仍取首 token（`sg (1)` 截断行为刻意保留，见 MO-06）；
`Kd`/`map_Kd` 解析顺序与结果不变；空行、注释行、未知关键字均仍为无副作用。

**边界：** 本卡只做「解析并承载」，不改变任何渲染或切片行为。
`opacity` 此时是纯数据字段，无消费者。

**验收：** 见 §8.1。

## 5. MO-02 ViewData alpha 贯通（COMPLETE，2026-09-01）

**目标：** 把 `MaterialInfo.opacity` 送进 `ViewMaterial.base_color[3]`。

**实施：**

```text
SceneViewAssetResolver.cpp  在既有 has_diffuse 分支后追加 alpha 写入：
  material.base_color[3] = source->has_opacity ? source->opacity : 1.0F
不新增 ViewMaterial 字段；alphaMode 仍由适配层自动推导。
```

**边界：** `has_opacity=false`（绝大多数既有资产）时 alpha 保持 1.0，
适配层判 opaque，下游行为与改前完全一致。

## 6. MO-03 CPU 光栅透明 pass（COMPLETE，2026-09-01）

**目标：** 让透明材质真正透出其后内容，且保证不透明路径逐像素不变。

**实施：**

```text
CpuRasterizer.cpp
  - 三角形按 alphaMode 分两批：opaque 批先画（行为完全不变），
    transparent 批后画且【不写深度缓冲】，仅做深度测试；
  - transparent 批按三角形视深 back-to-front 稳定排序；
  - 预览 alpha 下限钳制 kMinPreviewAlpha，保证 d=0 区域仍可见可选中。
```

**关键不变性：** 当场景内无 `alphaMode == "blend"` 材质时，transparent 批为空，
逐像素输出必须与改前完全一致。这是 MO-05 的核心验收项。

## 7. MO-04 不透明度 → 光油（COMPLETE，2026-09-01）

**路线：** 走 `materialVolumePolicy`（MATVOL），**不走** `materialRoleMapping`。裁定理由见决策文 §6。

**判据条件 C1-C4：** 见决策文 §7。摘要：`d`/`Tr` 视为同一事实并做矛盾检测；
用 Profile 容差而非 `== 0`；材质须拥有闭合实体否则 fail-closed；
`0 < d < 1` 必须有显式落位角色并出诊断。

**P1 已回签（2026-08-31）：** 弹性材料 = 缩裹 = transfer = **T 通道**，
通道字母 T 取自「弹性」拼音首字母，Transfer 英文首字母亦为 T，两侧自洽，**保留 T 不改**。
术语权威定义见 `DOC_DECISION_MATVOL_T_RGBWSVT协议与缩裹材料通道.md` §3.0（v1.2）。

**由此修正首版错误假设：** 该套工艺跑在 `p0.rgbwsvt.1`（7 通道）而非 `p0.rgbwsv.2`。
光油（V）与弹性（T）并列共存于同一包协议。新增 K1/K2/K3 前置核查，见决策文 §8.1。

**明确禁止：** 不得放宽 `config.cpp:1003` 让 roleMapping 与 white_underbase 共存。

## 8. MO-05 不变性与回归矩阵（COMPLETE，2026-09-01）

### 8.1 实测结果（本会话实际执行）

```text
构建   cmake --build build-slicesoft/main --config Debug
       改前基线 exit 0；改后 exit 0（已不经管道复核，": error" 计数为 0）
行数   python scripts/ValidateSourceSizeGuard.py --base-ref product/packaged-slicer
       PASS，无 G1/G2/G3 failure；45 条 warning 全为改动前既有的 G4/G5
       model.cpp 1982 -> 1941 行，G2「只减不增」以实际缩减满足
新单测 mtl_material_parser_unit_tests  Passed（8 个用例）
回归   ctest 全量 229 项：221 通过 / 8 失败
```

**CTest 实际项数为 229（含本次新增 1 项），改动前为 228。**
`AGENTS.md` 记载的「当前共 213 项」已过时，不能作为本专项的验收基准。

因果归属（改前 stash 后全量重建 exit 0，再对同一 8 项做 `ctest -R` 比对）：

| 测试 | 改后 | 改前 | 归属 |
|---|---|---|---|
| slicer_stage14c04_sync_capability_safety_test | Failed | Failed | 既有 |
| stage14f03_single_model_s1_gate | Failed | Failed | 既有 |
| stage14f05_local_closure_gate | Failed | Failed | 既有 |
| scene_layer_adapters_unit_tests | Failed | Failed（断言逐字一致） | 既有 |
| slicer_stage14e02_qt_host_boundary_test | Failed | Failed | 既有 |
| slicer_stage14e04d_dual_view_contract_test | Failed | Failed | 既有 |
| hostflow_hd02_real_asset_matrix | Failed | Failed | 既有 |
| **slicer_stage14b_layering_feasibility_test** | **Failed 3/3** | **Passed 5/5** | **本次引入** |

```text
7 项为分支继承的既有红灯，与本专项无关，不在本专项范围内修复
（Stage 14 在 AGENTS.md 中为 EXTERNAL ACCEPTANCE DEFERRED，部分项按设计等外部回签）。
1 项为本次真实引入，非 flaky：改前隔离运行 5/5 通过，改后隔离运行 3/3 失败，确定性差异。
根因为 Stage 14B 冻结合同（G-01），已按用户授权的方案 A 解除，见 §8.3。
```

#### 8.1.1 G-01 解除后的最终全量回归（2026-09-01）

```text
构建   FINAL_BUILD_EXIT=0，": error" 计数 0
回归   229 项：222 通过 / 7 失败    -> 失败数由 8 降至 7
       slicer_stage14b_layering_feasibility_test  Passed（本次引入项已修复）
       mtl_material_parser_unit_tests             Passed
剩余失败 7 项与改前基线【逐项一致】：
       slicer_stage14c04_sync_capability_safety_test
       stage14f03_single_model_s1_gate
       stage14f05_local_closure_gate
       scene_layer_adapters_unit_tests
       slicer_stage14e02_qt_host_boundary_test
       slicer_stage14e04d_dual_view_contract_test
       hostflow_hd02_real_asset_matrix
结论   满足 §8.2 增量口径：未新增任何失败项。
```

#### 8.1.2 切片产物逐字节不变性（2026-09-01，Release-to-Release）

```text
方法   同一配置、同一模型（fenandtou_d0_clean），改前/改后各用 Release 二进制跑一遍
94 层 TIFF 拼接哈希
  改前 3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
  改后 3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f   一致
报告   material_role_mapping / model_report / obj_mtl_material / slice_report 哈希全部一致
全目录 仅 3 处差异，逐条查明【全为探针自身路径差异】（configPath 两处、packageDir 一处），
       无任何语义漂移。
结论   MO-01..03 不参与切片计算这一设计前提得到实测印证。
```

#### 8.1.3 最终全量回归（2026-09-01，MO-04/06/08/09/10 全部落地后）

```text
Debug 构建    exit 0，": error" 计数 0
Release 构建  exit 0，": error" 计数 0
CTest         230 项：223 通过 / 7 失败
行数门禁      PASS（45 条 warning 全为改动前既有 G4/G5）

新单测
  mtl_material_parser_unit_tests        Passed（14 断言，含 MO-06 的 6 个）
  material_opacity_varnish_unit_tests   Passed（MO-04 的 7 个用例）
  slicer_stage14b_layering_feasibility  Passed（G-01 方案 A 解除后保持）

失败 7 项与改前基线【逐项相同】，未新增任何失败项：
  slicer_stage14c04_sync_capability_safety_test / stage14f03_single_model_s1_gate
  stage14f05_local_closure_gate / scene_layer_adapters_unit_tests
  slicer_stage14e02_qt_host_boundary_test / slicer_stage14e04d_dual_view_contract_test
  hostflow_hd02_real_asset_matrix
```

**默认路径零漂移（最关键项）：**

```text
经光油 pass、清 W、V/T 冲突检测三轮改动后重验
  基线   3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
  全量后 3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
结论   两个新配置默认关闭时 94 层 TIFF 逐字节一致，
       全部新逻辑正确门控在 opt-in 之后，未泄漏到默认路径。
```

**MO-06 未引起 Golden 漂移（原判风险未发生）：**

```text
既有 samples/configs/material_mapping/*.json 的规则为
matchNameContains: "white"/"varnish"/"clear" 等【子串】匹配，
而截断只影响空格之后的部分，子串匹配不受影响，故本批配置恰好免疫。
注意：这是【该批配置恰好不受影响】，不等于 MO-06 无风险。
将来若有配置依赖完整材质名的精确匹配，仍须复核。
```

**tm2-3 端到端实测（正确工艺：无 transfer + 按需补白墨 + 光油不要白墨底）：**

```text
124 层   R 910,405 / G 910,676 / B 910,676 / W 0 / S 9,593,222 / V 1,803,775
守恒     V + G = 2,714,451 === modelPixels，每像素恰好一个通道归属
光油     V 全部满墨(0)；W = 0 印证「光油区不要白墨底」
贴图     loadedTextures=2，sampled=657,033，R 非空值为连续色阶（贴图优先于 Kd 已验证）
阈值     标准档 1e-12 报 OpenSurface；精细档 1e-24 切片成功（同资产同配置仅改阈值）
```

### 8.2 不变性口径

```text
本专项 MO-01..03 不参与切片计算，故切片产物必须逐字节不变；
opaque 资产的宿主渲染必须逐像素不变；
CTest 口径修正为【增量口径】：与改前同一批次比对，不得新增失败项。
理由：分支继承 8 项既有红灯，原写「213 项全绿」在本分支上不可达，
留着会变成一条明知达不到的验收标准。
```

### 8.3 G-01 阻塞：Stage 14B 冻结合同拒绝 model.cpp 新增项目依赖

```text
测试   tests/contracts/ValidateStage14BLayeringFeasibility.py:158-167
报错   model.cpp acquired a project dependency outside the frozen base parser boundary:
       ['src/slicer_core/model.h', 'src/slicer_core/model/ObjFaceParser.h',
        'src/slicer_core/model/MtlMaterialParser.h']
性质   该断言是【字面精确列表】冻结，只允许上述前两项：
         expectedModelIncludes = ["src/slicer_core/model.h",
                                  "src/slicer_core/model/ObjFaceParser.h"]
```

用合同自身的 `AssignLayer()` 实测分类：

```text
base  src/slicer_core/model/MtlMaterialParser.h
base  src/slicer_core/model/MtlMaterialParser.cpp
base  src/slicer_core/model/ObjFaceParser.h
base  src/slicer_core/model.cpp
```

```text
即：新增头文件与既已放行的 ObjFaceParser.h 同层（base）、同目录、同命名空间、同角色，
未产生任何 base -> engine 边，合同同名所护的「分层可行性」属性未被削弱；
失败仅来自那条更严格的字面清单冻结。
```

**处置：不得自行放宽。** 该合同属 Stage 14 冻结面，按项目规则 7 与
「门禁放宽须先出授权文档」惯例，须先取得授权并留痕，故 MO-01 置为 BLOCKED 等裁决。
候选方案见决策文 §12。

## 9. MO-06 材质名整行解析（COMPLETE，2026-09-01；用户 2026-09-01 授权选项 1 材质名转义）

```text
现状  newmtl / usemtl 均用 stream >> name 只取首 token，"sg (1)" 截断为 "sg"。
      已在生产报告 material_role_mapping_report.json 复现。
影响  修复会改变材质名，进而改变按名匹配的既有配置与 Golden 报告内容。
      samples/configs/material_mapping/*.json 内的 matchNameContains 规则需复核。
裁定  不与 MO-01 混提，须单独评估 Golden 漂移后再开工。
```

## 9.5 MO-11 宿主接线（COMPLETE，2026-09-02）

**范围：** 把 MO-04（不透明度→光油）与 MO-08（退化面阈值）暴露到宿主侧，使工艺预设可携带这两项。

### 9.5.1 ABI 四字段（全部条件产出）

`apps/slicer_host_sim/HostRequestBuilder.h` 的 `hosteffectiveprofilesettings` 新增：

| 字段 | 默认 | 取默认时的行为 |
|---|---|---|
| `materialvolumeoverlapauto` | 0 | `overlap.mode` 仍产出 `explicit_priority` |
| `materialvolumeopacityvarnishenabled` | 0 | 不产出 `opacityVarnish` 块 |
| `materialvolumeopacityvarnishmax` | — | 仅上一项非 0 时生效 |
| `geometrydegenerateareaepsilonmm2` | 0.0 | 不产出 `degenerateAreaEpsilonMm2` 键 |

口径与 MV-07A 一致：**取默认值时生成的 Profile JSON 与修订前逐字节一致**，故既有预设 profileHash 不变、Stage 14 门禁不受影响。

### 9.5.2 数字格式约束（本卡的实现缺陷与修复）

Worker 侧 canonical 字符串的数字格式**必须**与 `src/slicer_core/json_value.cpp` 的 `dump_impl` 规范化输出逐字节一致：

```cpp
if (std::floor(number) == number) → setprecision(0) << fixed        // 整数不带小数点
else                              → defaultfloat << setprecision(15)
```

首版实现用 `%.6f`（产出 `0.001000` 尾零）与 `%.24g`，导致
`multilayer_transparent_varnish_lower_support` 的 Profile hash 不闭合：

```text
声明=sha256:9debc775eb7cfe8b8716d2b793277d2fed0f2ee8e48aaeee1e051008866b85b1
重算=sha256:1b2100d5ee0e2869072e27ad644a0b50f62495e392e817caa08506ce2191aa39
```

已全部改为 `%.15g`（对应 `defaultfloat + setprecision(15)`），并在
`HostVolumetricProfile.c` 与 `HostRequestBuilder.c` 两处加注释说明格式来源——
这是不读 `dump_impl` 无法得知的隐式契约，后续新增浮点字段同样会踩。

### 9.5.3 测试断言修正（未放宽任何检查）

`tests/hostflow/HostSliceSettingsTests.cpp` 的 `VerifyMaterialVolumeConditionalEmission`
原先对所有 MATVOL 预设硬编码单一形态（`explicit_priority` + 恰好 2 条规则 + `01`/`02` + `200`/`100`）。
已按形态分支：

- 公共字段统一校验（不变）
- 显式优先级分支：原有 `01`/`02` + `200`/`100` 校验**一字未改**
- 自动模式分支：**新增** 4 项校验（`auto_by_material_name`、`rules` 必须为空、`opacityMax ∈ (0,1)`、`semiTransparentRole == rgb`）
- 收口断言由 2 项增至 3 项（纳入 `sawVolumetricAuto`）

### 9.5.35 本卡的两项 UI 接线缺陷（2026-09-03 补记）

MO-11 当时只接了 ABI、Worker 侧 Profile 产出与工艺预设，**漏了 UI 宿主侧的两处**，
两者均在用户实机测试中暴露，详细记录见 `TASKS_MATOPQ_RGB` §9.55：

| 缺陷 | 修复 | 后果 |
|---|---|---|
| 多图层预设 `texture.enabled = false` | `f35ccaf` | 逐材质贴图采样整个不执行，各图层只剩 Kd 单色 |
| `HostMatvolSettingsPanel` 未接线四字段 | `7083c12` | 自动模式退化为手填模式致校验失败、按钮禁用；光油映射与退化面阈值一并丢失 |

第二项使 **UI 路径的多图层能力此前完全不可用**。本卡当时的验收（hostflow 四项 + 条件产出断言）
覆盖的是「ABI 与 Profile 产出」，未覆盖「UI 面板 → settings 结构」这一段，
故测试全绿而功能不可用。

**纪律补充：** 向 `hostmaterialvolumesettings` 之类的宿主设置结构新增字段时，
必须同时接线 ABI、Profile 产出、工艺预设**与 UI 面板存取**四处，缺一即为静默丢值。

### 9.5.4 实测结果（本会话实际执行并观察到的输出）

全量回归（Debug，`build-slicesoft/main`，2026-09-02 16:25）：

```text
BUILD=0   CTEST=8
97% tests passed, 7 tests failed out of 231
Total Test time (real) = 1812.66 sec
```

失败 7 项与继承基线逐项相同，本卡引入的 4 项已全部转绿：

```text
继承失败（未新增）
   18 - slicer_stage14c04_sync_capability_safety_test
   53 - stage14f03_single_model_s1_gate
   55 - stage14f05_local_closure_gate
  149 - scene_layer_adapters_unit_tests
  187 - slicer_stage14e02_qt_host_boundary_test
  226 - slicer_stage14e04d_dual_view_contract_test
  229 - hostflow_hd02_real_asset_matrix

本卡相关项全绿
  hostflow_hb05_slice_settings / he03_support_settings / he04_material_profile / he05_texture_profile
  slicer_stage14b_layering_feasibility_test（G-01 方案 A）
  source_size_guard_self_test（G1/G2/G3 行数门禁）
  matvol_production_wiring_tests / matvol_t_production_matrix_tests
```

资产端到端（Release，2026-09-02 16:33）：

| 资产 | 结果 | 墙钟 | 说明 |
|---|---|---|---|
| tm2-4n | exit 0 | 117s | 124 层 |
| tm2-5 | exit 0 | 114s | 124 层 |
| tm2-4 | exit 1 | 2s | `E_MATOPQ_LAYER_NAME_INVALID: material 'nail-1' does not end with the required -L<n> layer suffix` |

tm2-4n 与 tm2-5 六通道逐项一致，且与 2026-09-01 记录的 tm2-5 基线一字不差（零漂移）：

```text
层数=124  尺寸=310x567x6
R = 910,405   G = 910,676   B = 910,676
W = 0         S = 9,593,222  V = 1,803,775
V+G = 2,714,451 = modelPixels
```

优先级反转对照实验（`explicit_priority`，L2 组 220/210 > L1 组 120/110）：

```text
自动(L1优先)  R 910,405  G 910,676  V 1,803,775   V+G 2,714,451
反转(L2优先)  R 912,078  G 912,348  V 1,802,103   V+G 2,714,451
差异                +1,673     +1,672     -1,672          ±0
```

反转仅改变 1,672 像素（占 modelPixels 的 0.062%），证明 `closed_intervals`
在每个 XY 列上正确分离了 L1/L2 的 Z 区间——**并非 L1 整层压掉 L2**；
仅两层交界处落入重叠区由 priority 仲裁。反转有差异亦证明
`nail-L2`/`trans-L1` 均真实参与求解，未被无声丢弃。

## 10. 资产

```text
model/obj/multi-material/fenandtou.obj            原始资产（d 0.42）
model/obj/multi-material/fenandtou.objbak         Rhino 原始导出（无 3 个 Default 散面）
model/obj/multi-material/d0-varnish-test/
  fenandtou_d0.obj / .mtl                         d=0 变体，几何与源文件逐位相同
  fenandtou_d0_clean.obj / .mtl                   d=0 且已剔除 3 个 Default 散面（推荐用于工艺测试）
```

## 10.5 MO-12 下层材质贴图取色缺口（RESOLVED，2026-09-02）

> **本卡是 MO-11 实测暴露的结构性缺口，不是 MO-00..11 任一张卡的遗留。**
>
> **已由 MATOPQ-RGB 专项解决**（用户 2026-09-02 授权 M1+M2）。
> 分支 `MATOPQ-RGB`，任务清单 `TASKS_MATOPQ_RGB`，设计文 `DOC_DESIGN_MATOPQ_RGB`。
>
> 结果（tm2-5 的 `nail-L2` 段唯一色）：
>
> ```text
> 本卡记录时   1 个 = (250,250,250)   取到 trans-L1 的 Kd，跨材质错取
> M1 之后      1 个 = (167,243,255)   归属修正为 nail 自身 Kd，仍为单色
> M2 之后  22,882 个 均值 [213.3 124.7 112.6]   13_24_46.png 真实采样
> ```
>
> `nail-L1` 段三态逐项不变、`V+G` 三态守恒、默认路径 94 层哈希逐字节不变、
> 全量回归 231 项失败 7 项与继承基线相同。
>
> 以下为本卡当时的分析记录，保留备查。

### 10.5.1 现象

tm2-5 / tm2-4n 切片成功、层序识别正确、V+G 守恒成立，但**下层材质的贴图取不到**：

```text
按 Y 分段统计 RGB（tm2-5，chk_tm2-5）
  nail-L2 段 (原Y 31.7~41.5)   RGB像素 554,486   唯一值     1 个 → (250,250,250)
  nail-L1 段 (原Y 17.6~30.8)   RGB像素 356,190   唯一值 14,363 个 → 真实贴图
```

`nail-L1`（`13_24_46_1.png`）正确采样；`nail-L2`（`13_24_46.png`）整段为单一常量 250。

资产侧无缺陷：四材质 UV 齐全（`nail-L2` 6,274 面全部有 UV），两张 PNG 均存在，`map_Kd` 均已声明。

### 10.5.2 根因

`250` 并非贴图，而是 trans 材质的 Kd：`0.9804 × 255 = 250.002 → 250`。

几何叠放关系（tm2-5，原始坐标）：

```text
材质        面顶点数     Zmin     Zmax    Y 范围
nail-L1       17422    7.9312  11.9112   17.60 ~ 30.83
trans-L1      28198    7.3754  12.0260   24.02 ~ 41.57   ← 顶面
trans-L2      62146    7.6747  12.0526   17.70 ~ 33.69
nail-L2       21916    7.6916  12.0840   31.72 ~ 41.49   ← 被 trans-L1 遮住
```

`src/slicer_core/slicer.cpp` 的 `build_relief_texture_columns()` 逐 XY 列取色：

```cpp
const ReliefColumnInfo& column = columns.at(index);
const TriangleTextureInfo& texture_info =
    model_report.triangle_textures.at(column.top_triangle_index);   // 只有顶面
const RuntimeMaterialTexture* material = find_runtime_material(runtime, texture_info.material_name);
if (texture_info.has_uv && material != nullptr && material->loaded) { /* 采样 */ }
else { color.rgb = fallback_texture_rgb(config, material); }        // trans-L1 无 map_Kd → 走此支 → Kd 250
```

`material_rgb_for_role()` 的取色优先级同样以 `texture_columns->at(pixel_index)` 为最高，
而 `std::vector<TextureColumnColor>` **按 XY 列索引，一列仅一个颜色，无 Z 维度**。

因此两套机制脱节：

- **MATVOL 正确** —— 它已把 `nail-L2` 的体积识别为 RGB 材质区（那 554,486 像素即是），反转实验证明两层区间均参与求解
- **RGB 取色错误** —— 颜色仍由 relief 逐列顶面单一材质决定，下层材质的 `map_Kd` 永不可达

### 10.5.3 修复可行性（已核实的前置条件）

**UV 已贯通至 MATVOL 输入**，无需先做贯通改造：

```cpp
struct SurfaceTriangleAttributes {          // SceneModelTriangleMeshAdapter.h:17
    std::size_t source_triangle_index{0};
    bool has_uv{false};
    std::array<TexCoord, 3> uv{};
    std::string material_name;
};
struct AdaptedTriangleMesh {
    std::vector<SurfaceTriangleAttributes> triangle_attributes;   // MATVOL 构建输入
};
```

缺口在 plan 侧——`MaterialVolumePlan.h:28` 的区间不携带取色所需信息：

```cpp
struct MaterialLayerInterval {
    int firstLayerInclusive{0};
    int lastLayerInclusive{-1};
    std::uint32_t materialIndex{kNoMaterialOwner};
    // 缺：该材质在该列的顶面三角索引 + 重心坐标
};
```

内存边界（`MaterialVolumePlan.h` 头部注释，DEV_MATVOL §10）约束候选方案：

```text
允许  O(列数) 的 compact 区间 + 调用方持有的 O(XY) 单层 owner buffer
禁止  O(材质数 × 层数 × 像素数) 的稠密所有权栈
```

按 `MaterialLayerInterval` 扩两字段估算：310×567 列 × 最多 4 材质 ≈ 703,080 条，
每条 `int + 3×double` ≈ 28B → **约 20MB**，仍属 O(列数) 量级，与既有区间同阶，不触碰禁止项。

### 10.5.4 待用户决策项

1. 归属：新开 MATOPQ-RGB 专项，或并入 MATVOL-T
2. 是否接受 `MaterialLayerInterval` 扩字段（该结构受 MV-03 内存边界约束）
3. relief 路径行为变更的回归口径（既有单材质资产必须零漂移）

### 10.5.5 归因更正

用户 2026-09-01（消息 13）即报告「没有看到下层数据对应的贴图数据」，
当时实施方归因于 MATVOL 未启用与切片设置。**该归因错误。**
MATVOL 现已启用且工作正常，现象依旧——真因是 RGB 取色路径与 MATVOL 纵深求解为两套独立机制，与 MATVOL 配置无关。

## 11. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-02 | v1.3 | MO-11 宿主接线收口：ABI 四字段条件产出、`%.15g` 数字格式修复 Profile 哈希闭合、hostflow 断言按形态分支（未放宽检查）；全量回归 224/231（失败 7 项与基线逐项相同）；tm2-4n/tm2-5 端到端零漂移，tm2-4 按命名规范正确拒绝；优先级反转对照证明 L1/L2 区间分离。新增 MO-12 记录下层贴图取色缺口（BLOCKED，含根因、可行性核实与待决策项），并更正 2026-09-01 对该现象的归因。同步 §7/§9 过时的 PREPARED 标题。 |
| 2026-09-01 | v1.2 | MO-04/06/08/09/10 全部收口：全量回归 223/230（失败 7 项与基线逐项相同，未新增）；默认路径 94 层 TIFF 逐字节零漂移；tm2-3 端到端实测 V+G 守恒；记录 tm2-x 无缩裹区域的资产语义更正与实施方误解根源。 |
| 2026-09-01 | v1.1 | MO-01/02/03/05 收口：G-01 按用户授权方案 A 解除，全量回归 222/229（失败数 8→7，与基线逐项一致），切片产物 94 层 TIFF 逐字节零漂移；新增 MO-07 策略总表（COMPLETE）；P1 回签为 T 通道并修正首版协议假设；K1/K2 实测完成，K3 七项参数齐备（L4=V 优先）。 |
| 2026-08-31 | v1.0 | 首版。建立 MATOPQ 专项 7 张卡；MO-00 完成；MO-01/02/03/05 进入实施；MO-04 因弹性材料通道归属未回签保持 INPUT_OPEN；MO-06 因涉及 Golden 漂移单列待授权 |
