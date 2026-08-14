# SliceSoft 能力 DTO 合同

> 合同版本：1.11
> SPI 版本：`PM_SPI_VERSION=1`
> 机器可读真源：`contracts/slicer_capability_dtos.json`
> 受控修订：`DOC_DECISION_14A_04_R1_双视图纹理ViewData合同修订.md`、
> `DOC_DECISION_14A_04_R2_重能力输入身份补充.md`、
> `DOC_DECISION_14F_R1_HOSTFLOW场景生命周期合同受控修订.md`、
> `DOC_DECISION_14F_R2_HOSTFLOW隐式场景初始化上下文受控修订.md`、
> `DOC_DECISION_14F_R3_HOSTFLOW规则排版合同受控修订.md`、
> `DOC_DECISION_RENDER_R_B_00_ViewMesh复用DTO受控修订.md`、
> `DOC_DECISION_RENDER_R_B_03_ViewData降级理由受控修订.md`、
> `DOC_DECISION_RENDER_R_B_04_ViewData半精度传输合同修订.md`、
> `DOC_DECISION_14A_04_R3_不完整OBJ灰色降级与单材料准入.md`

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

不新增独立 `scene.layout` 能力；规则排版经既有 `scene.apply_operation` 的
`applyGridLayout` 操作提交。碰撞和越界真值继续由既有响应返回。

### 1.1 构建体积

`scene.get_snapshot` 与 `scene.apply_operation` 响应均携带 `buildVolume.widthMm`、
`buildVolume.heightMm` 以及可选 `buildVolume.zLimitMm`。默认设备 Profile 显式解析为
`230 x 100 x 60 mm`；旧 scene 未声明 `zLimitMm` 时不得自动补写该字段，确保 canonical JSON
与 scene hash 保持逐字节兼容。

当 `zLimitMm` 存在且实例世界 bbox 的 `max.z` 超限时，`scene.apply_operation.warnings`
返回非阻断告警。该判定与 top/three_d 视图无关，也不得复用 `autoOrient.maxHeightMm`；后者只
表示自动定向目标高度，不是设备物理限高。

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

### 3.0 模型外观完整性与降级准入

`model.import` 与 `model.get_metadata` 必须返回 `appearanceStatus`、
`singleMaterialOnly` 和 `appearanceDetail`。当 OBJ 实际使用 `usemtl`，但完全没有
`mtllib`/MTL 定义，或已解析 MTL 引用的漫反射贴图文件不存在时，模块允许只读视图以
半透明中性灰降级显示，并返回 `singleMaterialOnly=true`。宿主必须据此禁用所有 RGB
工艺，只允许单材料白墨或单材料光油。

该例外不覆盖已声明 MTL 中无法解析的材质名、存在但解码失败的贴图、无效 UV 或其他
外观合同错误；这些情况继续 fail-closed。降级状态只影响模型显示和宿主工艺准入，
不得改变生产 RGBWSV/TIFF 协议。

### 3.1 场景生命周期与实例操作

`scene.apply_operation.operations[].type` 支持：

```text
addInstance
removeInstance
applyGridLayout
translate
rotateZ
uniformScale
mirror
```

`addInstance` 条件必填 `modelId`。该 id 必须由同一个模块实例内成功的 `model.import` 返回，
且尚未执行 `model.release`。`assignInstanceId` 可选；缺省时由模块分配唯一实例 id。
`initialTransform` 可选，字段与 scene 的 canonical model transform 一致：
`translateXMm`、`translateYMm`、`rotateZDeg`、`uniformScale`、`mirrorX`、`mirrorY`；
缺省为 identity。`addInstance` 禁止同时提供 `instanceId`，避免实例 id 双重来源。

`removeInstance` 条件必填 `instanceId`，只移除场景实例，不隐式释放 `modelId`。既有四种变换仍按
v1.4 字段工作：`translate.deltaMm`、`rotateZ.degrees`、`uniformScale.factor` 和 `mirror.axis`。
多个 operation 按请求数组顺序原子执行，任一操作失败不得暴露部分提交。

`applyGridLayout` 通过 `layout` 条件必填对象提交 11×2 行优先规则排版：`policy=grid`、
`maxColumns=1..11`、`maxRows=1..2`、非负 `columnGapMm` / `rowGapMm`、
`spacingMode=edge_clearance`、`order=row_major`。它作用于场景稳定实例顺序，隐藏实例继续占位，
锁定实例保持原位，容量上限为 22。为避免同一批中变换中间态与排版基准不一致，
`applyGridLayout` 必须是该请求的唯一 operation；不得携带实例或模型身份字段。
成功响应沿用 `scene.apply_operation`，包含全部实例 canonical transform、碰撞与越界结果。

#### 3.1.1 隐式空场景创建

以下条件同时成立时，模块创建新空场景，并在成功响应中条件必填 `sceneHandle`：

```text
sceneHandle 缺省；
scene 缺省或严格为空对象 {}；
operations 至少包含一个 addInstance；
sceneContext 携带宿主权威的 resolvedProfileId 与 device_profile buildVolume；
currentSceneRevision == 0；
expectedSceneRevision == 0。
```

`sceneContext` 只允许出现在隐式创建路径。`resolvedProfileId` 必须为非空字符串；
`buildVolume.source` 必须为 `device_profile`，`isFixture` 必须为 `false`，宽高以及可选的
`zLimitMm` 必须为有限正数。原点和轴向由宿主显式给出。已有 `sceneHandle` 或非空 inline
scene 请求禁止携带 `sceneContext`，避免两个权威源并存。幂等 fingerprint 必须包含完整
`sceneContext`，同一 `operationId` 改变 Profile 或构建体积时 fail-closed。

该限制用于保持旧请求兼容：不含新枚举、无 handle、无 scene 的旧变换请求继续按 v1.4 失败；
非空 inline `scene` 请求继续使用原语义，响应不得仅因合同升级而新增 `sceneHandle`。后续状态请求
使用隐式创建响应中的 `sceneHandle` 和最新 revision。

场景 session 绑定 `pm_module_t`，由 `pm_destroy` 统一清理。`pm_release` 只释放 `pm_job_t*`，
不得用于关闭整数 `sceneHandle`。v1.6 不提供显式 per-scene close；若后续需要提前回收，必须另行
受控修订。

#### 3.1.2 权威场景快照与生产透传

`scene.get_snapshot` 和 `scene.apply_operation` 响应中的 `scene` 是与 `sceneHash`、
`sceneRevision` 对应的完整 canonical scene，不是只含 handle/instance 摘要的展示对象。宿主可以把
该对象作为 opaque JSON 原样传给 `slice.rgbwsv.scene`，但不得自行构造、删减或补写内部 scene
字段。canonical 数值统一消除有符号零，避免 `-0.0` 经不同 JSON 实现归一为 `0.0` 后改变 hash。

`addInstance` 生成的 `sourceTransformIdentity` 使用已注册模型的源路径身份，以满足生产切片器的
资源一致性校验；source digest 继续只作为内容身份，不能代替可解析的源路径。

> H-A-01 冻结 add/remove，HQ-07 冻结隐式场景上下文。新操作运行时、幂等和负例属于
> H-A-02；在 H-A-02 完成前，不得把 v1.6 合同存在误写成新操作已经可用。

### 3.2 Worker 重能力输入身份

`geometry.preflight.full` 除 committed `scene` 外，必须携带 `sceneHash`、
`expectedSceneRevision`、canonical effective `profile`、`profileHash` 和显式 `targetMode`。
Worker 使用该 Profile 重建与生产切片相同的模型加载几何；禁止使用默认配置，禁止从 Profile 名称
猜测 Legacy/Global。可选 `buildVolume` 出现时必须与 scene 完全一致。

full preflight 响应必须绑定 scene/revision/hash，返回 authoritative/complete/cancelled、模型与实例
计数、每实例 transform identity、碰撞和越界证据。几何 blocker 是成功执行后的业务结果；资源、
身份、完整性和取消失败不得伪装为 authoritative PASS。

`geometry.repair` 首版只接受并输出 OBJ。请求必须携带 effective Profile、Profile hash、资源范围、
`modelFormat=obj` 和 `repairOutputFormat=obj`。成功结果必须证明资产已写入、已重导入、strict 检查
完整且通过，并且 UV/材质/资源属性得到保留。STL/3MF repair 在首版显式返回不支持，不得丢属性
后成功。

## 4. Scene ViewData

`scene.get_viewdata` 完整采用 `DOC_SCHEMA_14_SceneViewData网格DTO规格`：

```text
坐标系      right_handed_z_up
单位        mm
字节序      little_endian
网格        float32 或 float16 position/normal/texcoord0 + uint32 index
LOD         auto/lod0/lod1/lod2/outline_only
推荐变换    local mesh + row-major worldMatrix[16]
缓存        viewdataIdentity 标识快照，顶层 meshes[] 按 meshIdentity 复用网格
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

多模型响应使用 `appearances[]`；`instances[].appearanceIdentity` 必须解析到其中唯一一组外观，
该实例的 `surfacePreview.appearanceIdentity` 必须与其一致，网格 submesh 的 `materialId` 在该组
`materials[]` 内解析。单数 `appearance` 无法表达多模型场景，禁止使用。
`outline_only` 在 top 模式仍必须保留 `surfacePreview`，在 three_d 模式不允许使用。

three_d 响应的规范网格载体为顶层 `meshes[]`。每个 `instances[].meshIdentity` 必须唯一解析到
`meshes[].meshIdentity`；`meshTransform=local` 时，同一模型内容和 LOD 的多个实例只序列化并存储
一份网格 blob。为兼容 v1.7 宿主，v1.8 暂时保留 `instances[].mesh` 描述符别名，但该别名复用
同一 `blobId`，不得再次存储二进制内容。新宿主必须优先读取顶层 `meshes[]`。

### 4.1 网格属性编码

`meshAttributeFormat` 是可选查询字段，支持 `float32` 与 `float16`，缺省严格保持 `float32`，
因此旧宿主无需修改即可继续读取原有格式。请求 `float16` 时，position、normal 与 texcoord0
分别返回 `float16x3`、`float16x3`、`float16x2`，均使用 IEEE-754 binary16 与小端字节序；
index 继续使用 `uint32`。半精度由 `meshopt_quantizeHalf` 产生，宿主必须按 descriptor 解码，
不得把 binary16 指针当作 float32 使用。

`maxBytes` 预算按实际 wire format 估算；`meshIdentity` 包含编码身份，避免 float32/float16
缓存互相污染。R-B-04 同时量化 UV，原因是仅量化 position/normal 的理论上限不足以满足既定
40% 传输缩减目标；对冻结真实甲片口径，三类属性全部半精度后每个无共享三角由 108 B 降至
60 B，并把 22 实例均分预算阈值抬升到至少 25k 三角/实例。

### 4.2 降级理由

`truncationReason` 使用分号连接多个独立原因。当前 Provider 只允许以下稳定值：

```text
mesh_simplified_lod1_for_max_bytes
mesh_simplified_lod2_for_max_bytes
texture_resolution_reduced_for_max_bytes
top_preview_resolution_reduced_for_max_bytes
```

只有实际三角数低于源网格时才能返回 `mesh_simplified_*`；仅尝试较低 LOD 但几何未变化时不得误报。
历史跳采样语义保留 `mesh_decimated_lod1_for_max_bytes` / `mesh_decimated_lod2_for_max_bytes`
作为诊断保留字，但 R-B-02 后的当前 Provider 禁止产生这些值。宿主可对 `mesh_decimated_*` 显示强告警，
对 `mesh_simplified_*` 显示受控质量降级提示。

### 4.3 双视图请求

```json
{
  "capability": "scene.get_viewdata",
  "operation": "query",
  "viewMode": "top",
  "texturePolicy": "require_if_present",
  "meshAttributeFormat": "float16",
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

`options.backend` 是兼容字段，不是引擎选择器。缺省值和唯一合法值均为小写 `worker`；
`inprocess`、`auto`、大小写变体及其他字符串返回 `PM-SLICER-PROFILE-0031`，非字符串返回
`PM-SLICER-INPUT-0002`。Worker 启动、合同协商或执行失败时不得回退到 DLL 进程内切片。

## 6. 版本与后续实现

本合同冻结对外 DTO；v1.6 的 `addInstance` / `removeInstance` 和隐式建场景已由 H-A-02 实现，
v1.7 的 `applyGridLayout` 由 H-A-04 实现，v1.8 由 R-B-00 增加顶层可复用 `meshes[]`，
v1.9 由 R-B-03 冻结安全简化与历史抽稀的降级理由，v1.10 由 R-B-04 增加向后兼容的
半精度网格属性请求与响应格式，v1.11 增加不完整 OBJ 的显式外观状态与单材料准入字段；
H-A-03 已验证权威 scene 快照可由纯 C/Qt 宿主不透明透传到生产切片。独立 `scene.layout`
能力仍被禁止。
交互幂等、revision 回滚和三车道细则见
`contracts/slicer_three_lane_contract.*`；取消状态机由 14A-06 冻结。
