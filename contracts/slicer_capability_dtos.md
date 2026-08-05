# SliceSoft 能力 DTO 合同

> 合同版本：1.1
> SPI 版本：`PM_SPI_VERSION=1`
> 机器可读真源：`contracts/slicer_capability_dtos.json`

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
网格        float32x3 position/normal + uint16|uint32 index
LOD         auto/lod0/lod1/lod2/outline_only
推荐变换    local mesh + row-major worldMatrix[16]
缓存        viewdataIdentity 标识快照，meshIdentity 标识可复用网格
分块        scene.get_viewdata operation=read_blob，经既有 pm_result 取回
```

只改变 `worldMatrix` 不使 `meshIdentity` 或 blob 失效。首版实现可只返回 bbox 与 outline，
但必须显式设置 `truncated`/`truncationReason`；不得静默丢弃 mesh。不得新增第 16 项能力、
`pm_get_blob` 或第 12 个 ABI 导出符号。

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
