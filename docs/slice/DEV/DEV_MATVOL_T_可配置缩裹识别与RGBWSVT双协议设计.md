# DEV_MATVOL-T 可配置缩裹识别与 RGBWSVT 双协议设计

> 文档状态：**ACTIVE / EXPLICIT PRODUCTION OPT-IN IMPLEMENTED**  
> 版本：v1.2 | 日期：2026-08-26  
> 决策：`../DOC/DOC_DECISION_MATVOL_T_RGBWSVT协议与缩裹材料通道.md`

## 1. 配置目标

目标配置块：

```json
{
  "output": {
    "packageProtocol": "p0.rgbwsvt.1",
    "channelOrder": ["R", "G", "B", "W", "S", "V", "T"]
  },
  "transferChannelPolicy": {
    "enabled": true,
    "matchSource": "material_diffuse_rgb",
    "materialDiffuseRgbValues": [[255, 220, 198]],
    "missingRegion": "allow_empty",
    "multipleMatches": "fail_closed",
    "value": 0,
    "topology": {
      "selfIntersectionPolicy": "tolerate_closed_self_intersection",
      "maxSelfIntersectionPairs": 64
    }
  }
}
```

旧配置缺字段时保持 `p0.rgbwsv.2` 与六通道。未知协议、未知匹配源、空颜色表、重复颜色、
T 值等于空值、旧协议启用 T、新协议使用错误通道顺序全部 fail closed。

## 2. 核心 DTO

```cpp
struct TransferMaterialMatch {
    bool present;
    std::string materialName;
    std::array<std::uint8_t, 3> diffuseRgb;
};

struct RgbwsvtLayer {
    int widthPx;
    int heightPx;
    std::vector<std::uint8_t> channels; // 7 bytes per pixel
};
```

首版 resolver 要求恰好一个材质命中。零命中按 `allow_empty` 返回 `present=false`；多命中失败。

## 3. 几何复用

从 `AdaptedTriangleMesh` 过滤出命中材质的三角面，复用 `BuildMaterialVolumePlan`：

```text
full adapted mesh
  -> exact material-name filter（名字来自颜色 resolver，不来自配置）
  -> topology classifier
  -> compact interval plan
  -> MaterializeMaterialOwnershipLayer
  -> transferMask = owner != kNoMaterialOwner
```

这样只要求缩裹子网格满足 T 的几何 Gate，不要求甲片材质进入多材质 owner plan。甲片仍使用当前
生产 occupancy 与既有工艺合成。T mask 必须是 model mask 子集。

## 4. 单层合成

输入保持现有六通道层，输出按像素扩为七通道：

```text
transferMask=0: copy RGBWSV, append T=255
transferMask=1: RGBWSV=255, append T=configured value
```

合成器不读取配置文件、不做材质识别、不持有跨层缓冲。调用方复用单层 T mask 和七通道输出缓冲。

## 5. TIFF

LibTIFF Writer 允许 6/7 sample；Photometric 仍为 RGB：

```text
RGBWSV : SamplesPerPixel=6, ExtraSamples=3, ImageDescription=RGBWSV
RGBWSVT: SamplesPerPixel=7, ExtraSamples=4, ImageDescription=RGBWSVT
```

handwritten Writer 保持只支持旧六通道。新协议若解析到 handwritten lane 必须明确失败，不能回退或
静默丢 T。Reader/Preview/RIP 使用 manifest 协议选择 6/7，不从 SamplesPerPixel 猜业务协议。

## 6. 工艺迁移

旧工艺文件原样保留；每个支持 T 的工艺新增 `_rgbwsvt` 版本，至少覆盖：

```text
单材料白墨 W
单材料光油 V
彩色全 RGB
RGB + 按需补白
```

新版工艺只改变协议和缩裹路由；甲片工艺参数从对应旧文件复制并单独计算新 hash。

## 7. 测试

```text
resolver: 01/02 角色更正、颜色可配置、无匹配、多匹配、缺 Kd
geometry: closed T、open T、self-intersection budget、mask subset
composer: T 排他、无 T 六通道投影零差异、W/V/RGB 三工艺
TIFF: 6/7 sample tags、stripped/tiled、none/packbits、边缘 tile padding
protocol: old schema rejects T、新 schema requires T at index 6
reality: 03/08/09 + 后续修正资产
compatibility: 旧工艺 package/TIFF/profileHash 零漂移
```

## 8. 当前实现落点

T-01..T-09 已完成。`run_slicer` 可生成完整 RGBWSVT Package，但准入状态由调用边界决定：

```text
参考 Host 显式新版 Profile -> Worker slice.rgbwsvt/minor=1 -> singleton Scene
  -> transfer_scene_production_opt_in=true -> productionAcceptance=admitted

直接 slicer_cli 或不完整的 Scene 资格
  -> productionAcceptance=rgbwsvt_candidate_unvalidated
```

生产 Facade 是 opt-in 标志的唯一正式发射点；Runner 还要求 T 已启用、instance/input override 均存在、
无 callback 与 model-report override，避免普通直接调用伪装成生产 Scene。严格 Reader 只接受上述两个状态，
Worker 对新协议产物二次校验并只接受 `admitted`。旧协议路径不设置该标志。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.2 | 登记 T-01..T-09 完成及显式生产准入边界：Host/Worker Scene admitted、direct CLI candidate、Worker 二次校验。 |
| 2026-08-25 | v1.1 | 登记 T-01..T-04 低层原型实现状态和 T-05 前生产入口 fail-closed 边界。 |
| 2026-08-25 | v1.0 | 建立配置、resolver、T-only plan、七通道合成、TIFF 与工艺迁移设计。 |
