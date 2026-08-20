# DEV_MATVOL 逐层多材质所有权与白区载体兼容设计

> 文档状态：**DESIGN BASELINE / IMPLEMENTATION NOT STARTED**
> 版本：v1.0 ｜ 日期：2026-08-20
> 决策：`../DOC/DOC_DECISION_MATVOL_多材质纵深体积RGB与按需补白根治.md`
> 任务：`../../codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`

## 1. 目标与非目标

目标是把“每列一个顶面材质”升级为“每层每像素一个确定材质 owner”，同时保持单层流式内存、
Stage 15 同层按需补白和现有 RGBWSV 合同。

非目标：自动修复自交/非流形；把 MeshLab 选择色写入 MTL；改变支撑/光油物理策略；默认启用
OpenVDB；一次支持所有 GeneralMesh/S3/S4；修改 RIP 或新增通道。

## 2. 目标数据流

```text
ModelReport + Triangle(material key/Kd/UV)
  -> MaterialTopologyClassifier
  -> MaterialIntersectionProvider (compact, immutable)
  -> MaterialLayerOwnershipMaterializer (caller-owned one layer)
  -> MaterialLayerRgbComposer
  -> ApplyUnprintableWhiteCarrier(final RGB)
  -> existing support/varnish/material closure
  -> owned layer sink / package writer
```

依赖方向保持 `geometry -> scene DTO`、`materials -> material DTO`、`pipeline -> geometry/materials/output`；
geometry 不写 RGB/TIFF/report，materials 不读取文件，UI 不接触交点内部类型。

## 3. 配置草案

新增字段必须默认关闭；字段名在 MATVOL-02 实现前可做一次命名复核，但语义不得弱化。

```json
{
  "materialVolumePolicy": {
    "enabled": false,
    "mode": "closed_intervals",
    "openSurface": {
      "mode": "reject",
      "thicknessMm": 0.0,
      "placement": "below_surface"
    },
    "overlap": {
      "mode": "explicit_priority",
      "rules": [
        {"matchMaterialName": "01", "priority": 200},
        {"matchMaterialName": "02", "priority": 100}
      ]
    },
    "missingMaterial": "fail_closed"
  }
}
```

首批只允许 `slicePipeline=legacy`、`slicingMode=relief_heightfield`、S0。未知枚举、负厚度、重复规则、
无匹配优先级、同级实际重叠、Global/S3/S4 组合全部配置期或 preflight fail closed。

## 4. 核心 DTO

建议新增 STL-only 内部合同：

```cpp
struct MaterialKey {
    std::string modelId;
    std::string materialName;
};

enum class MaterialTopologyKind {
    ClosedOrientable,
    OpenSurface,
    NonManifold,
    SelfIntersecting,
    Invalid
};

struct MaterialTopologyFact {
    MaterialKey key;
    MaterialTopologyKind kind;
    std::uint64_t faceCount;
    std::uint64_t boundaryEdgeCount;
    std::uint64_t nonManifoldEdgeCount;
};

struct MaterialZInterval {
    int firstLayerInclusive;
    int lastLayerInclusive;
    std::uint32_t materialIndex;
};

class MaterialVolumePlan; // move-only, validated, immutable after build

void MaterializeMaterialOwnershipLayer(
    const MaterialVolumePlan& plan,
    int layerIndex,
    std::span<const std::uint8_t> modelMask,
    std::span<std::uint32_t> ownerOut);
```

`ownerOut` 使用唯一无 owner sentinel；只在 `modelMask=1` 时允许 material owner。生产接入前评估是否可
降为 `uint16_t`，但不得以截断或 hash 碰撞换内存。Plan 禁止隐式深拷贝，逐层 materializer 不分配。

## 5. 闭合材质算法

1. 按材质子网格建立确定性三角面集合；排序只用于确定性，不决定优先级。
2. 对采样列生成有序 Z 交点，使用与 S0 相同 XY 采样中心和容差。
3. 合并共面/共享边重复命中；奇偶配对形成该材质的闭合区间。
4. 奇数交点、方向冲突或数值不稳定时 fail closed，并报告列/三角面/material key。
5. 多个材质区间重叠时应用显式 priority；同级重叠失败。
6. 转换为层区间时使用现有层中心/层厚规则，不能另造 Z 量化公式。

空洞必须保留为多个区间，禁止用 first/last 包络填满。

## 6. 开放表面候选

`openSurface.mode=reject` 是唯一默认。显式 `surface_band` 候选：

```text
requested thicknessMm > 0；
effectiveLayerCount = ceil(thicknessMm / layerThicknessMm)；
placement 首批只允许 below_surface；
从命中的开放表面向模型内部方向写有限层 owner；
不改变 model occupancy，不把壳层外区域加入模型；
壳层与闭合主体重叠时按显式 priority。
```

法线/内外方向不可信或一个列有冲突开放命中时诊断模式可报告，生产模式必须失败。对 `03.obj` 的
具体厚度和绿色覆盖浅桃色规则由 MQ-01/MQ-02 决定。

## 7. RGB 与按需补白

`MaterialLayerRgbComposer` 按 owner 解析：有效 Texture2D/UV -> MTL `Kd` -> 显式 fallback；缺失策略
由配置决定。写完当前模型像素 RGB 后立即调用既有 `ApplyUnprintableWhiteCarrier`：

```text
ink = 255 - min(R,G,B)
white_underbase && ink <= threshold -> W = configured value
```

该调用必须覆盖 MATVOL 分支，但继续复用同一谓词和统计字段；不得复制一份近似判据。Stage 15 的
旧组合禁令保留，只新增“MATVOL 已通过 capability Gate”的窄放行条件。`materialPolicy` 和旧
`materialRoleMapping` 仍不得与 MATVOL 首批同时启用。

## 8. 报告与稳定错误

新增 `reports/material_volume_report.json`，schema 草案 `slicesoft.material_volume_report.1`：

```text
effectivePolicy/profileHash/sourceHash
material table: key, Kd, topology, priority
openSurface requested/effective thickness
per-layer owner pixels by material
overlap resolved/blocked pixels
unowned model pixels
unprintableWhiteCarrierPixels
memory/timing/replay identity
warnings/errors
```

稳定错误至少包括：

```text
E_MATVOL_UNSUPPORTED_PIPELINE
E_MATVOL_MATERIAL_MISSING
E_MATVOL_OPEN_SURFACE_REQUIRES_POLICY
E_MATVOL_TOPOLOGY_INVALID
E_MATVOL_INTERSECTION_UNPAIRED
E_MATVOL_OVERLAP_UNRESOLVED
E_MATVOL_MODEL_PIXEL_UNOWNED
E_MATVOL_REPLAY_MISMATCH
E_MATVOL_BUDGET_EXCEEDED
```

## 9. 文件所有权与任务映射

| 模块 | 建议文件 | 卡 |
|---|---|---|
| topology | `src/slicer_core/materials/volume/MaterialTopologyClassifier.*` | MV-02 |
| intersections | `src/slicer_core/materials/volume/MaterialIntersectionProvider.*` | MV-03 |
| open shell | `src/slicer_core/materials/volume/OpenSurfaceBandMaterializer.*` | MV-04 |
| ownership | `src/slicer_core/materials/volume/MaterialLayerOwnership.*` | MV-05 |
| RGB compose | `src/slicer_core/materials/volume/MaterialLayerRgbComposer.*` | MV-05 |
| orchestration | `src/slicer_core/pipeline/MaterialVolumePipeline.*` | MV-06/MV-08 |
| report | `src/slicer_core/reports/MaterialVolumeReport.*` | MV-06 |
| config | `config.h/.cpp`、迁移/schema | MV-02/MV-06 |
| Host | `HostSliceSettings*`、Profile builder/panel | MV-07 |
| tests | `tests/matvol/*`、Stage15/Stage16 回归 | 各卡 |

旧 `slicer.cpp` 先调用新 helper wrapper，完成等价 Gate 后再移动职责；禁止一次性重写。

## 10. 内存与性能

```text
允许：O(XY) caller-owned owner/RGBWSV scratch + compact intersection intervals；
禁止：O(material * layer * XY) masks、逐层重复全量 plan 校验、每像素动态容器；
要求：checked size arithmetic、move-only plan、同步取消点、单层 buffer 地址复用；
路由：RasterMemoryBudget 在写目录/TIFF 前完成，预算不足直接失败。
```

性能阈值在 MV-01 记录同请求基线后冻结。未取得同姿态/同 Profile/同输出证据前，不承诺固定百分比。

## 11. 测试矩阵

| 维度 | 用例 |
|---|---|
| topology | closed/open/non-manifold/self-intersection/missing material |
| intervals | 单区间、多区间、空洞、共面、共享边、负 Z、边界层 |
| overlap | 不重叠、显式高低优先级、同级阻断、顺序确定性 |
| open shell | reject、0/非法厚度、below surface、离散层数、方向冲突 |
| color | Kd、贴图、fallback、缺失 fail closed |
| white | strict white、near white、绿色、浅桃色；差异只在 W |
| combined | support/base/varnish/closure/repair/preview/report |
| compatibility | 旧 RGB、旧按需补白、materialRoleMapping、Global 均零漂移或明确拒绝 |
| lifecycle | cancel、consumer fail、writer fail、无半包 |
| memory | 大 Grid、多个材料、no dense stack、预算拒绝 |
| reality | `finger_suoguo/03.obj`，逐层 owner/RGB/W 可解释 |

## 12. 生产 Gate

```text
G1 DTO/错误/配置 fail-closed
G2 synthetic closed interval oracle diff=0
G3 open surface candidate and thickness decision
G4 owner -> RGB -> white carrier same-layer diff=0
G5 support/varnish/closure combined diff=0
G6 old Profile TIFF/package/RIP strict zero drift
G7 bounded memory and cancellation/no partial publish
G8 Reality 03 and representative existing assets
G9 Host/Profile/UI and package preview clarity
G10 user authorization for production opt-in
```

任一 Gate 失败时只保留非生产诊断，不得扩大到默认路由。

## 13. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-20 | v1.0 | 冻结分层架构、DTO、算法、开放面候选、白区兼容、报告、内存和验证 Gate。 |
