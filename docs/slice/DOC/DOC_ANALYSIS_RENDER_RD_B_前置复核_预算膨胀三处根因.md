# DOC_ANALYSIS_RENDER RD-B 前置复核：预算膨胀的三处根因

> 文档状态：**ACTIVE / BLOCKS RD-B**
> 版本：v1.0 ｜ 日期：2026-08-09
> 定位：在裁决 RD-B（是否引入 `meshoptimizer`）**之前**，先核实预算超限的真实成因
> 关联：`TASKS_RENDER` R-B 组、`REPORT_RENDER_R_A_01`、`DOC_SCHEMA_14` §3/§4
> 证据等级：A=已核实代码事实，B=目标设计，P=判断

---

## 0. 结论先行

**R-A-01 的实测数据成立，但「需要引入 `meshoptimizer`」这个推论不成立 —— 至少现在不成立。**

破洞的直接原因不是模型太大，而是**三处实现缺陷让预算被人为放大了几十倍**。
三处都能在**不引入任何第三方依赖**的前提下修掉，其中第一处是**一行改动**。

```text
根因 1  宿主把 lod 硬编码成 "lod2"          → 阈值被钉死在 10,000 三角面    【一行】
根因 2  同模型多实例的 mesh 被重复计入 N 次  → 22 实例排版下预算膨胀 22 倍   【一个缓存】
根因 3  顶点未共享（每三角 3 个独立顶点）    → 约 3.9 倍冗余                【中等】
```

**在这三处修完并重新实测之前，不应授权第三方依赖。**

---

## 1. 根因 1 · 宿主硬编码 `lod2`（A · 一行）

`apps/slicer_ui_host_sim/render/SceneRenderPolicy.cpp:41-49`：

```cpp
{QStringLiteral("viewMode"),      QStringLiteral("three_d")},
{QStringLiteral("texturePolicy"), QStringLiteral("require_if_present")},
{QStringLiteral("lod"),           QStringLiteral("lod2")},      // ← 硬编码
{QStringLiteral("meshTransform"), QStringLiteral("local")},
{QStringLiteral("maxBytes"),      128 * 1024 * 1024},           // ← 128 MiB
```

而 `src/slicer_core/api/viewdata/SceneViewMeshBuilder.cpp:84-99`：

```cpp
case ViewLod::Lod0: return std::numeric_limits<std::size_t>::max();  // 永不抽稀
case ViewLod::Lod1: return 50000U;
case ViewLod::Lod2: return 10000U;                                   // ← 生效的是这个
```

### 后果

**宿主一边申请 128 MiB 预算，一边无条件要求最激进的 LOD。**
`lod2` 使抽稀阈值恒为 **10,000 三角面**，与 128 MiB 预算**完全无关**。

对照 `REPORT_RENDER_R_A_01` 的 36 个实测模型：

```text
按 13.8k「预算阈值」计    17 / 36 超限   （R-A-01 报告的口径）
按 10k「lod2 阈值」计     35 / 36 超限   ← 真实触发面
                          仅 caihong/5mm.obj（308 面）幸免
```

即：**11,680 / 12,736 / 13,386 这一批"报告里判为安全"的模型，实际上同样在被抽稀。**
R-A-01 用预算阈值推导，未注意到宿主已把 `lod` 写死，因此**低估了破洞范围**。

### 修法

```cpp
{QStringLiteral("lod"), QStringLiteral("auto")},
```

`DOC_SCHEMA_14` §4 已冻结 `auto` 的语义：**由模块按 `maxBytes` 预算自动选择**，
先试 lod0，超了再降。这正是本场景需要的行为，且合同早已写明「**谁决定：模块决定实际 LOD，
宿主只给预算**」。硬编码 `lod2` 等于绕过了这条已冻结规则。

## 2. 根因 2 · 同模型多实例的 mesh 被重复计入（A · 一个缓存）

`src/slicer_core/api/viewdata/SceneViewCandidateBuilder.cpp`：

```cpp
std::map<ModelId, ResolvedViewAppearance> budgetedAppearances;   // :112  外观【有缓存】
for (const PreparedViewInstance& preparedInstance : prepared)    // :114  逐实例
{
    auto budgeted = budgetedAppearances.find(...);               // :122  外观按 model_id 复用
    ...
    ApiResult<ViewMesh> mesh = BuildViewMesh(...);               // :194  网格【无缓存】
    instance.mesh = std::move(*mesh.Value());                    // :209  每个实例各存一份
}
```

**外观（含 2048×2048 纹理）按 `model_id` 做了缓存并提升到顶层 `appearances[]`；
网格没有 —— 每个实例都重新构建并各自持有一份完整 mesh。**

`SceneViewBudget.cpp:35-64` 的估算随之逐实例累加：

```cpp
for (const ViewInstance& instance : viewData.instances) {
    if (instance.mesh.has_value()) {
        bytes += mesh.positions.size() * 4 + mesh.normals.size() * 4
               + mesh.texcoord0.size() * 4 + mesh.indices.size() * 4;
    }
}
```

### 这违反了已冻结的合同

`DOC_SCHEMA_14` §3 原文：

> ① 同一模型多实例时，local 网格只需传【**一份**】，各实例只带各自矩阵；
>    world 会造成 N 倍数据膨胀（排版场景常见 10+ 实例）

**合同要求避免的 N 倍膨胀，实现里在 `local` 模式下照样发生了。**

### 安全性已核实（A）

`SceneViewMeshBuilder.cpp:188-192` —— 世界矩阵**只在 `MeshTransform::World` 分支使用**：

```cpp
if (meshTransform == MeshTransform::World) {
    triangle.a = TransformPoint(triangle.a, worldMatrix);
    ...
}
```

宿主请求的是 `meshTransform: "local"`（`SceneRenderPolicy.cpp:47`），
因此**同一模型所有实例的顶点逐字节相同**，
`mesh_identity = ComputeMeshIdentity(mesh)`（:257）也必然相同。**去重安全。**

### 修法

镜像 `budgetedAppearances` 的做法：按 `model_id` 缓存 `ViewMesh`，
并把网格提升到顶层 `meshes[]`（与 `appearances[]` 同构），实例只留 `meshIdentity` 引用。

**收益：11×2 = 22 实例的甲片排版场景，网格字节数降为 1/N。**
若 22 个实例来自 ~10 个不同模型，收益约 2.2 倍；若同模型重复排布，最高 22 倍。

> ⚠️ 这一项**改动的是响应结构**，需确认是否构成 DTO 受控修订
> （顶层新增 `meshes[]` 属于新增可选字段，旧宿主行为不变 → 判断为 **minor 兼容**，
> 但仍须按 14F-R1/R2/R3 的先例走受控修订流程，**不得直接改**）。

## 3. 根因 3 · 顶点未共享（A · 中等）

`SceneViewMeshBuilder.cpp:153-156`：

```cpp
mesh.positions.reserve(estimatedTriangles * 9U);   // 每三角 3 顶点 × 3 分量
mesh.normals.reserve(estimatedTriangles * 9U);
mesh.texcoord0.reserve(estimatedTriangles * 6U);
mesh.indices.reserve(estimatedTriangles * 3U);
```

每三角面独占 3 个顶点，**顶点完全不共享**：

```text
当前     position 36B + normal 36B + uv 24B + index 12B = 108 B / 三角面
共享后   顶点数 ≈ 三角数 / 2 → (32 B × 0.5) + 12 B    ≈  28 B / 三角面
                                                        ≈ 3.9 倍收益
```

**注意**：共享顶点会改变法线语义（平滑 vs 硬边）。
本项目 UV 贴图模型存在 UV 缝，缝上的顶点**必须**保持分裂。
因此收益低于理论值 3.9 倍，需按 position+normal+uv 三元组去重实测。

## 4. 三处合计的预估效果（P）

以最重的 `MF_ai_shen_shizhi_L_tx03.obj`（299,980 三角面）单实例计：

| 状态 | 每三角字节 | 单实例 mesh 体积 | 128 MiB 预算下 |
|---|---:|---:|---|
| 当前（lod2 抽稀到 10k）| 108 | 1.08 MB | 破洞 |
| 仅修根因 1（lod0 原样）| 108 | **32.4 MB** | ✅ 单实例可容纳 |
| + 根因 3（顶点共享）| ~28 | **8.4 MB** | ✅ 宽松 |
| + 根因 2（22 实例去重）| ~28 | 8.4 MB（不随实例数增长）| ✅ 排版场景成立 |

**结论（P）**：仅修根因 1 与 2，最重资产的单实例与排版场景**都能在预算内无损显示**。
`meshoptimizer` 的 QEM 简化在此之后**可能完全不需要**。

## 5. 对 RD-B 的建议

```text
❌ 现在不授权 meshoptimizer
✅ 先修根因 1（一行）→ 重新实测 → 再评估
```

三条理由（P）：

**① 顺序错误。** 在把自身实现的三处放大因素修掉之前引入第三方库来"压缩数据"，
是用依赖成本掩盖实现缺陷。若根因 1、2 修完后已不超预算，
则 `meshoptimizer` 变成一个**永远不会被触发的依赖**。

**② R-A-01 的判据本身要重算。** 报告用的 13.8k 是预算推导值，
而真实触发阈值是 lod2 的 10,000 —— 两个数字不同，
且**修掉根因 1 之后阈值会整体上移一个数量级**。在旧阈值上做的选型结论不能直接沿用。

**③ 引入依赖的成本不可逆。** 需改 `vcpkg.json` + CMake + 部署清单 + 移植清单
（`slicer_ui_host_portability_manifest.json` 已冻结 46 个文件）+ 打印侧回签。
这些成本只有在确认必要时才值得付。

> 📌 **但 `meshoptimizer` 的选型准备工作不作废。**
> `DOC_PREP_RENDER_R_B_*` 中的许可证（MIT）、vcpkg port、CMake 集成方案保持有效，
> 一旦根因 1–3 修完仍超预算，可直接据此授权，无需重做调研。

## 6. 对 H-D-02 的建议

**codex 暂停 H-D-02 的判断方向正确，但阻塞面过大。**

```text
codex 的理由：17/36 模型触发抽稀 → 3D 画布会显示破洞模型
实际情况：    35/36 触发（lod2 硬编码），比报告更严重
但：          根因 1 是【宿主侧一行改动】，就在 H-D-02 自己的作用域内
```

**建议把根因 1 并入 H-D-02，而不是把 H-D-02 整卡挂起等 R-B：**

```text
H-D-02 内先做   lod2 → auto（一行，宿主侧，无需 RD-B）
                并按 DOC_SCHEMA_14 §4 规则 6 处理 PM-SLICER-VIEWDATA-BUDGET：
                超预算时【显式报错】，不静默显示破碎网格
然后            正常完成 3D 画布与相机接线
R-B-01/02       降级为「预算仍不足时的质量优化」，不再是 H-D-02 的硬前置
```

理由（P）：`lod=auto` + 显式预算错误，已经满足「不把破碎 mesh 暴露给用户」这一底线要求，
而这正是 codex 暂停 H-D-02 的**唯一理由**。底线一旦由更便宜的手段满足，阻塞就应解除。

## 7. 派生动作

| 编号 | 动作 | 归属 |
|---|---|---|
| RB-P1 | 宿主 `lod2` → `auto` + 预算错误显式提示 | 并入 **H-D-02** |
| RB-P2 | 模块按 `model_id` 缓存 mesh + 顶层 `meshes[]`（走受控修订）| 新卡 **R-B-00**，优先于 R-B-01/02 |
| RB-P3 | 顶点共享（按 pos+normal+uv 去重）| 新卡 **R-B-05** |
| RB-P4 | 修完 P1–P3 后**重测** 36 个模型，重算触发面 | **R-A-02** |
| RD-B | 依据 R-A-02 结果再裁决是否引入 meshoptimizer | **推迟** |

## 8. 修订记录

### 8.1 R-A-02 实测回填（2026-08-10）

RB-P1/P2/P3 完成后，Release 对原 36 个 OBJ 重新测量：22 个资产单实例完整 lod0，
14 个按既有纹理合同显式拒绝，预算拒绝为 0。R-B-05 的真实平均值为 105.15 B/三角，
未达到理论 28 B/三角，原因是当前 ViewData 只有面法向，真实曲面大部分顶点不能安全共享。

把 22 个合同有效资产聚合到同一场景后，provider 实际返回 lod2：上传 mesh 16.19 MiB、
纹理 100.50 MiB。由此确认破坏性跳采样仍可能在真实多模型场景触发，RD-B 启动条件成立。
R-B-01 可先独立修正配额；R-B-02 的第三方依赖仍等待显式授权。详细证据见
`REPORT_RENDER_R_A_02_顶点共享后真实资产重测.md`。

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-10 | v1.1 | 回填 R-A-02：顶点共享后平均 105.15 B/三角；22 个有效资产聚合仍返回 lod2，RD-B 启动条件成立，但第三方依赖继续等待明确授权。 |
| 2026-08-09 | v1.0 | 首版。核实预算超限的三处根因（宿主硬编码 lod2 / mesh 未按 model 去重违反 DOC_SCHEMA_14 §3 / 顶点未共享），给出各自 A 级代码位置与收益估算；建议推迟 RD-B、解除 H-D-02 的硬阻塞，并派生 RB-P1..P4 |
