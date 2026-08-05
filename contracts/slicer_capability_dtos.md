# SliceSoft 能力 DTO 合同

> 合同版本：1.2
> SPI 版本：`PM_SPI_VERSION=1`
> 机器可读真源：`contracts/slicer_capability_dtos.json`
> 受控修订：`DOC_DECISION_14A_04_R1_双视图纹理ViewData合同修订.md`

## 1. 范围

本合同冻结打印宿主通过 11 个 `pm_*` C ABI 导出调用的 15 项能力。ABI 只承载
UTF-8 JSON、句柄和调用方缓冲，不跨边界传递 Qt、STL 或异常。能力结果统一包含
`ok` 与 `code`，错误码以 `contracts/slicer_error_codes.json` 为准。

能力清单固定为：

```text
model.import
model.get_metadata
model.release
scene.apply_operation
scene.get_snapshot
scene.get_viewdata
geometry.preflight
geometry.collision
geometry.repair
slice.rgbwsv
package.verify
package.get_summary
package.get_layer_descriptor
package.render_layer_preview
package.read_report
```

`scene.layout` 不属于能力包；packing 策略由宿主负责，碰撞和越界真值由
`geometry.collision` 与 `scene.apply_operation` 返回。

## 2. 承载边界

| 类型 | 能力 |
|---|---|
| DLL 进程内同步 | model 三项、scene 三项、`geometry.collision`、五项 package 查询 |
| 模式决定 | `geometry.preflight.fast` 在 DLL；`geometry.preflight.full` 在 Worker |
| Worker 异步 | `geometry.repair`、`slice.rgbwsv` |

`model.import` 暂按进程内合同冻结；若 14B-00 证明无法进入 `slicer_base`，允许只改变承载，
不得改变本卡冻结的请求与响应语义。

## 3. 字段规格

每项能力的请求、响应路径、JSON 类型、必填条件、常量和错误码均在机器可读真源中逐项列出。
数组元素使用 `[]` 表示，条件必填使用 `requiredFor` 表示。响应还必须满足公共结果信封：

```json
{
  "ok": true,
  "code": "PM-SLICER-OK-0000",
  "message": "optional diagnostic text"
}
```

失败时不得返回部分成功语义；未知字段可按 SPI minor 向前兼容，删改既有字段语义必须提升
SPI major。

## 4. Scene ViewData

`scene.get_viewdata` 完整采用 `DOC_SCHEMA_14_SceneViewData网格DTO规格`：

```text
坐标系      right_handed_z_up
单位        mm
字节序      little_endian
网格        float32x3 position/normal + float32x2 texcoord0 + uint16|uint32 index
LOD         auto/lod0/lod1/lod2/outline_only
推荐变换    local mesh + row-major worldMatrix[16]
缓存        viewdataIdentity 标识快照，meshIdentity 标识可复用网格
分块        scene.get_viewdata operation=read_blob，经既有 pm_result 取回
显示        top / three_d
外观        appearances[]；每组由 appearanceIdentity + materials + RGBA8/sRGB texture blobs 标识
俯视纹理    surfacePreview RGBA8/sRGB blob（必需，不以宿主自行投影替代合同响应）
```

只改变 `worldMatrix` 不使 `meshIdentity`、`appearanceIdentity` 或对应 blob 失效。
对声明纹理的模型，`top` 必须返回带纹理 `surfacePreview`，`three_d` 必须返回 UV、材质和纹理；
预算不足可降低 LOD/纹理分辨率，但不得静默退为无纹理灰模。模型本身没有纹理时返回
`textureStatus=not_provided` 并使用 `baseColorFactor`。不得新增第 16 项能力、`pm_get_blob`
或第 12 个 ABI 导出符号。

`surfacePreview.localBoundsMm` 使用模型局部 XY 边界；宿主通过实例 `worldMatrix` 放置预览四边形。
不得把世界边界塞入 preview blob identity，否则每次拖拽都会错误地使纹理预览缓存失效。

多模型响应使用 `appearances[]`，实例的 `surfacePreview.appearanceIdentity` 与网格 submesh 的
`materialId` 必须能解析到其中唯一一组外观；单数 `appearance` 无法表达多模型场景，禁止使用。
`outline_only` 在 top 模式仍必须保留 `surfacePreview`，在 three_d 模式不允许使用。

### 4.1 双视图请求

```json
{
  "capability": "scene.get_viewdata",
  "operation": "query",
  "viewMode": "top",
  "texturePolicy": "require_if_present",
  "content": ["bbox", "outline", "surface_preview", "appearance"]
}
```

同一模型在 `top` 与 `three_d` 必须解析到同一 `appearanceIdentity`。切换视图只切换相机和呈现策略，
不能重新导入模型、改变 scene revision 或丢失选中集。

## 5. 生产协议红线

```text
schema       p0.rgbwsv.2
channels     R G B W S V
bitDepth     8
polarity     black_is_print
printValue   0
emptyValue   255
```

`package.render_layer_preview` 必须从生产 TIFF 解码，不能读取调试 preview PNG。
`slice.rgbwsv` 只接受已经 Commit 的 `sceneHash`，并通过 Worker 生产包；不允许回退到进程内
切片或静默切换 Legacy/OpenVDB 引擎。

## 6. 版本与后续实现

本卡只冻结对外 DTO，不创建 facade、DLL 或 Worker。C++ 内部 DTO 与 facade 在 14B-01 实现，
必须保持字段语义一致。交互幂等、revision 回滚和三车道细则见
`contracts/slicer_three_lane_contract.*`；取消状态机由 14A-06 冻结。
