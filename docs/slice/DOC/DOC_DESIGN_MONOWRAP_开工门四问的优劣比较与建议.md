# DOC_DESIGN MONOWRAP 开工门四问：优劣比较与建议

> 文档状态：**待裁定**（G-2、G-3 需用户拍板；G-1 我给建议；G-4 已不成立）
> 版本：v1.0 ｜ 日期：2026-09-20 ｜ 专项：**MONOWRAP**
> 上游：`DOC_ANALYSIS_MONOWRAP_单材料缩裹现状与改动面.md`
> 任务卡：`docs/codex_task/current/TASKS_MONOWRAP_单材料缩裹专项任务清单.md`
> 本文回答任务卡 §1 的开工门 G-1..G-4，**每条结论都附实测或代码位置**。

---

## G-4 先说：这一问不成立，答案被协议锁死

**结论：`transferChannelPolicy.value` 必须是 0，没有可选项，不需要裁定。**

`src/slicer_core/output/rgbwsvt/RgbwsvtProtocol.cpp:66`：

```cpp
if (transferValue != CurrentRgbwsvtProtocol().printValue)
{
    throw TransferChannelError(
        TransferChannelErrorCode::ConfigInvalid,
        "transfer print value must be 0 for p0.rgbwsvt.1 black_is_print");
}
```

`RgbwsvtProtocol.h:25` 里 `printValue{0U}`。`p0.rgbwsvt.1` 用的是
**black_is_print**（0 表示打印）约定，预览合成器那侧也一致——
`MaterialPreviewComposer.cpp:99` 判 `hasTransfer = transferValue < 255U`。

所以 schema 里的 `"value": { "const": 0 }` 是**正确**的，整模模式下同样取 0。
**把它列为开工门是我盘点时的错误**，在此更正。

---

## G-1：下游确实依赖 `materialName`，而且有一道我上一版漏看的硬拦截

### 先回答问题本身

| 字段 | 下游是否依赖 | 位置 |
|---|---|---|
| `diffuseRgb` | **否** | 只进报告元数据（`RgbwsvtLegacyPackageMetadata.cpp`），不参与几何 |
| `materialName` | **是，两处** | `TransferMaterialVolumePlan.cpp:41`（overlap 优先级规则）与 **`:46`（`materialNameFilter`，几何筛选器）** |

`materialNameFilter` 的语义是好消息——`MaterialVolumePlan.cpp:139-140`：

```cpp
if (!request.materialNameFilter.empty()
    && fact.materialName != request.materialNameFilter)
{
    continue;
}
```

**空 filter = 不筛选 = 全部三角形。**「整模」这个语义现有代码已经实现好了。

### 但紧接着有一道拦截，上一版盘点漏了它

`MaterialVolumePlan.cpp:144-149`：

```cpp
if (fact.materialName.empty())
{
    throw MaterialVolumeError(
        MaterialVolumeErrorCode::MaterialMissing,
        "mesh contains triangles without a bound material");
}
```

`fact.materialName` 直接取自 `mesh.triangle_attributes[i].material_name`
（`MaterialTopologyClassifier.cpp:102,119`）。目标模型**零材质绑定**⇒
全部三角形归成一组、组名为空 ⇒ **在这里就抛了**，根本走不到体积求解。

**所以「空 filter 等于整模」救不了我们**：filter 是空的没错，
但网格里的三角形本身没有材质名，被上面这道检查拒掉。

### 两个方案

#### 方案 A：放宽 `MaterialVolumePlan` 的空名检查

让它接受无材质绑定的网格，把空名当成一个合法的「匿名材质」组。

| | |
|---|---|
| **优** | 改动集中在一处；语义上「无材质的网格就是单一材质」也说得通 |
| **劣** | **波及面大**：这条路径是 MATVOL 与 T 通道**共用**的。放宽它等于同时改变了所有材质体积求解对「无材质网格」的 fail-closed 行为 |
| **劣** | 该检查是**有意的 fail-closed**——现在无材质资产会被明确拒绝而不是静默产出错误结果。放宽它要有独立理由，不能只因为 MONOWRAP 需要 |
| **劣** | 触发 MATVOL 专项的既有契约与测试，**等于在本专项里替 MATVOL 做决定** |
| **风险** | 高。可能影响字节级基线里走 MATVOL 的用例 |

#### 方案 B：在 T 通道内部合成一个材质名（**建议**）

整模模式下，进入体积求解前给所有三角形贴一个合成材质名
（如 `__monowrap_all__`），`materialNameFilter` 设为同名。

| | |
|---|---|
| **优** | **完全不碰 `MaterialVolumePlan` 的 fail-closed 行为**，MATVOL 侧零影响 |
| **优** | 改动封闭在 `TransferMaterialVolumePlan.cpp` 一个函数内，可读性好、易回退 |
| **优** | 合成名对下游是透明的——`overlap.rules` 与 `materialNameFilter` 拿到的都是同一个非空名，两处依赖自然满足 |
| **优** | 报告里的 `materialName` 有确定取值（合成名），不必让它变成可空字段，**顺带减轻 G-2 的 schema 压力** |
| **劣** | 需要拷贝一份 `triangle_attributes`（或在适配阶段注入），有一次额外内存开销。实测单件 6k~14k 面、整盘约 10 万面级，开销可忽略 |
| **劣** | 合成名会出现在报告里，需要在文档中说明它不是真实材质 |
| **风险** | 低。不改变任何既有路径的行为 |

### 建议：方案 B

理由是**边界**：无材质网格该不该被体积求解接受，是 MATVOL 的问题；
MONOWRAP 需要的只是「把整个模型当成一个材质」。用合成名把这两件事分开，
本专项就不必替 MATVOL 决定它的 fail-closed 策略。

这与本仓既有纪律一致——**别的专项的债不要在这里替它承接**。
若将来 MATVOL 自己认为该放宽空名检查，那是它的授权范围，届时方案 B 可以顺势简化。

---

## G-2：报告 schema 走「同版本放宽」还是「升版本」

`contracts/slicesoft.transfer_channel_report.1.schema.json` 有四处阻碍：

```json
"required": [..., "matchSource", "configuredMaterialDiffuseRgbValues",
             ..., "materialName", "matchedDiffuseRgb", ...],
"matchSource": { "const": "material_diffuse_rgb" },
"configuredMaterialDiffuseRgbValues": { "type": "array", "minItems": 1, ... }
```

**注意：若 G-1 取方案 B，第三、四项自动化解**——
合成名让 `materialName` 有值，`matchedDiffuseRgb` 可取合成默认值。
**剩下真正要动的只有 `matchSource` 的 `const` 与颜色列表的 `minItems`。**

### 方案一：同版本放宽（**建议**）

`const` → `enum: ["material_diffuse_rgb", "whole_model"]`，
`minItems: 1` 改为按 `matchSource` 条件约束（JSON Schema `if/then`）。

| | |
|---|---|
| **优** | 只维护一份 schema；消费方（`ValidateJsonSchemas.py`、`LegacyRgbwsvtCliCandidateTests.cpp`）不必区分版本 |
| **优** | **既有工艺的报告形状逐字节不变**——它们仍写 `material_diffuse_rgb` 与非空颜色列表，条件分支走的是原来那条 |
| **优** | 不新增 `packageProtocol`，`p0.rgbwsvt.1` 保持不变，**不触碰包协议这一层** |
| **劣** | schema 变复杂，多出一个 `if/then` 分支 |
| **劣** | 放宽 `const` 后，schema 对旧形状的约束力略降（原来是「只能是这个值」，现在是「二选一」） |
| **风险** | 中低。须实测既有 738 产物基线不变 |

### 方案二：升到 `slicesoft.transfer_channel_report.2`

| | |
|---|---|
| **优** | 风险隔离彻底，`.1` 一个字符都不动，旧形状约束力完全保留 |
| **优** | 语义清晰：新模式就是新契约 |
| **劣** | **两份 schema 长期双维护**，后续任何字段改动要改两处——本仓已经吃过「两个常量各写各的」的亏（F-53） |
| **劣** | 消费方要按 `schema` 字段分支，`ValidateJsonSchemas.py` 与测试都要加路径 |
| **劣** | 版本号语义会含糊：`.2` 与 `.1` 的差别仅是多一个枚举值，**不足以称为一个新契约版本** |
| **风险** | 中。改动面更大，且引入长期维护成本 |

### 建议：方案一

**决定性理由是差异的量级**：两者的唯一实质区别是 `matchSource` 多一个取值、
以及颜色列表在新取值下可为空。**这个量级不值得一个新契约版本**——
升版本的代价是永久的双维护，而收益只是隔离一个枚举值的风险。

而且 F-53 刚刚给过教训：**同一个概念散落在多处、各自演化，最终会漂移**。
两份 schema 就是在制造下一个 F-53。

**前提**：必须先实测「既有工艺的报告与产物逐字节不变」（任务卡 MONOWRAP-03），
这一条不过就重新评估。

---

## G-3：整盘一次切还是逐件切

### 实测数据说得很清楚

| 文件 | X 区间 | Y 区间 | Z 区间 |
|---|---|---|---|
| 101_ND002-down05 | [78.9, 91.0] | [1.5, 16.9] | [19.1, 23.6] |
| 102_ND002-down09 | [146.7, 155.4] | [3.9, 18.6] | [19.3, 23.0] |
| 103_ND002-down08 | [131.2, 141.3] | [2.7, 17.4] | [19.2, 23.3] |
| 104_ND002-up02 | [32.1, 40.9] | [43.3, 56.7] | [20.1, 23.9] |
| 105_ND002-down10 | [160.4, 167.4] | [6.2, 16.4] | [19.5, 22.8] |
| 201_ND002-up06 | [97.0, 109.2] | [43.5, 59.7] | [20.1, 24.7] |
| 202_ND002-down12 | [184.0, 193.3] | [3.1, 16.9] | [19.3, 23.1] |
| 203_ND002-up03 | [46.2, 56.4] | [43.9, 58.1] | [20.1, 24.3] |
| 204_ND002-up12 | [184.7, 194.0] | [43.8, 56.9] | [20.2, 24.1] |
| 205_ND002-up01 | [20.1, 27.2] | [44.0, 54.2] | [20.2, 23.6] |

**整盘包围盒 174.0 × 58.2 × 5.6 mm，XY 平面两两重叠 0 对，明显分两排**
（Y≈1.5~18.6 与 Y≈43~60）。**这是一盘已经排好版的指甲片。**

### 方案一：整盘一次切（场景模式，**建议**）

| | |
|---|---|
| **优** | **保住排版**。坐标是资产自带的，逐件切会把这个信息丢掉 |
| **优** | 产出**一个**可直接打印的包；逐件会得到 10 个没法当一个作业打的包 |
| **优** | 现有代码已支持：`SceneModelTriangleMeshAdapter.cpp:75` 把 `scene.triangles` **拍平成单一三角形列表**，T 通道会话（`slicer.cpp:339-349`）消费的正是这个合并后的网格 |
| **优** | Z 区间各件不同（19.1~24.7），整盘切能保证层号与 Z 的对应一致 |
| **劣** | 单次内存与耗时更高（约 10 万面级，仍属小规模） |
| **风险** | 低 |

### 方案二：逐件切 10 次

| | |
|---|---|
| **优** | 单次规模小；某一件失败不影响其余 |
| **劣** | **丢排版**，需要在别处重建，等于把资产自带的信息扔掉再造一遍 |
| **劣** | 10 个包的合并是额外工程，本专项没有这项 |
| **劣** | 每件各自 auto-orient 会引入不一致的定向 |
| **风险** | 中。合并逻辑是新增面 |

### 建议：方案一（整盘一次切）

但有**一项待验证**：场景路径是否已完整接通 T 通道。
`slicer.cpp:339` 的会话建立在主切片路径上，消费的是拍平后的场景网格，
**看起来是通的**；但 `--scene-config` 这个入口是否走到同一段代码，
我尚未实跑验证。**这一条列为 MONOWRAP-01 的第一项实测**，不作为已知结论。

---

## 汇总

| 门 | 结论 | 是否需要你裁定 |
|---|---|---|
| **G-1** | 建议**方案 B**：T 通道内部合成材质名，不动 MATVOL 的 fail-closed | 建议即可，我可直接执行 |
| **G-2** | 建议**方案一**：同版本放宽（`const`→`enum` + 条件 `minItems`） | **需要你拍板** |
| **G-3** | 建议**整盘一次切**；场景入口通不通需先实测 | **需要你确认实际用法** |
| **G-4** | **不成立**，协议锁死为 0 | 否 |

G-1 若取方案 B，G-2 的改动面会同步缩小到只剩两处——**两者是有耦合的，建议一起定**。
