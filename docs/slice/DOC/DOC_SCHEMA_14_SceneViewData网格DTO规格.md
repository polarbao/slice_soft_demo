# DOC_SCHEMA_14 `scene.get_viewdata` 网格 DTO 规格

> 文档状态：✅ **ACTIVE / 契约草案**（供 14A-04 冻结采用）
> 版本：v1.1 ｜ 日期：2026-08-04 ｜ 基线收口：2026-08-05
> 定位：填补 `scene.get_viewdata` 网格数据的字段级规格空白（风险 UI-R4）
> 上游：`DEV_14` §5（承载分派）、`DOC_DECISION_14_UI` §6.4（缺口来源）
> 证据等级：A=已核实事实，B=目标设计，P=判断

---

## 0. 为什么必须现在定

**现状（A）**：`scene.get_viewdata` 的网格数据从未有过字段级规格。

| 出处 | 原文 |
|---|---|
| `CLAUDE_13` 能力表 | 「俯视轮廓、bbox、**可选三角缓冲**」 |
| `INT_16` 示例 | `"viewdataIdentity": "vd:43:inst-01:lod0"` —— 出现 `lod0`，语义未定义 |
| 现有实现 | `src/slicer_core/scene/SceneViewGeometry.*` 面向**俯视投影**构建 |

**14A-04 是契约冻结任务，是唯一一次低成本定义它的机会。** 3D 视角（14E-04c）需要网格数据；
若冻结时不定，届时将迫使**已交付给打印侧的 ABI 二次变更**。

> 即使首版实现只返回俯视轮廓，**DTO 也必须完整定义**，实现可以缺、契约不能缺。

## 1. 全局约定

```text
单位        毫米（mm），双精度或单精度浮点，字段名一律带 Mm 后缀
坐标系      右手系，+Z 向上（与切片一致）
字节序      little_endian（显式声明，不得让宿主假设）
浮点格式    IEEE 754
JSON 编码   UTF-8，无 BOM
```

## 2. 请求 DTO

```jsonc
{
  "capability": "scene.get_viewdata",
  "sceneId": "scene-001",
  "expectedSceneRevision": 43,          // 乐观并发：与当前不符则返回 SceneRevisionStale
  "instanceIds": ["inst-01", "inst-02"], // 空数组或缺省 = 全部实例
  "content": ["bbox", "outline", "mesh"],// 请求内容，可任意组合
  "lod": "auto",                         // auto | lod0 | lod1 | lod2 | outline_only
  "meshTransform": "local",              // local（推荐） | world
  "maxBytes": 33554432                   // 宿主给出的本次传输预算上限（32 MiB）
}
```

| 字段 | 必填 | 说明 |
|---|:--:|---|
| `content` | 是 | `bbox` / `outline` / `mesh` 三选多。不请求 `mesh` 时行为等价于现状 |
| `lod` | 是 | 见 §4 |
| `meshTransform` | 是 | 见 §3 |
| `maxBytes` | 是 | **必填**。模块据此决定实际 LOD 与是否分块；不得默认无限 |

## 3. 实例变换：`local` + `worldMatrix`（关键设计）

**默认且推荐 `meshTransform: "local"`。** 响应中网格为**模型局部坐标**，另附 4×4 世界矩阵。

```text
local  网格 = 模型局部坐标 + worldMatrix（行主序 16 float）
world  网格 = 已应用世界变换的坐标，worldMatrix 为单位阵
```

选 `local` 的三条理由（P）：

```text
① 同一模型多实例时，local 网格只需传【一份】，各实例只带各自矩阵；
   world 会造成 N 倍数据膨胀（排版场景常见 10+ 实例）
② 拖拽/旋转时【只有矩阵变，网格不变】—— 宿主可复用已缓存的顶点缓冲，
   仅更新一个矩阵即可重绘。这是 UI-M1「拖拽期跨 DLL 调用恒为 0」成立的前提
③ 宿主的 GPU 渲染管线本来就按 "mesh + model matrix" 组织，local 与之天然对齐
```

> ⚠️ **`worldMatrix` 变化不使网格缓冲失效** —— 见 §6 失效规则。这一条是 3D 交互性能的关键。

## 4. LOD 语义

| 值 | 含义 | 目标规模 |
|---|---|---|
| `lod0` | 原始网格，不简化 | 原样 |
| `lod1` | 中等简化 | 目标 ≈ 50k 三角面/实例 |
| `lod2` | 高度简化 | 目标 ≈ 10k 三角面/实例 |
| `outline_only` | 不含网格，仅俯视轮廓 + bbox | —— |
| `auto` | **由模块**按 `maxBytes` 预算与实例数自动选择 | 见下 |

**谁决定：模块决定实际 LOD，宿主只给预算。** 理由：模块知道网格实际规模，宿主不知道。

```text
auto 的选择规则（B）
  1. 估算 lod0 在当前实例集下的总字节数
  2. 若 ≤ maxBytes → 用 lod0
  3. 否则依次尝试 lod1、lod2
  4. 仍超出 → 返回 outline_only 并置 truncated=true，说明原因
```

**响应必须回报 `mesh.lod` 的实际值**，宿主不得假设拿到的就是所请求的。

## 5. 响应 DTO

```jsonc
{
  "viewdataIdentity": "vd:43:inst-01:lod1:9f3ac21b",
  "sceneRevision": 43,
  "units": "mm",
  "coordinateSystem": "right_handed_z_up",
  "byteOrder": "little_endian",
  "instances": [
    {
      "instanceId": "inst-01",
      "modelId": "model-a",
      "bboxLocalMm": { "min": [-12.0, -8.0, 0.0], "max": [12.0, 8.0, 6.4] },
      "worldMatrix": [ 1,0,0,0,  0,1,0,0,  0,0,1,0,  35.0,20.0,0.0,1 ],
      "outline": { "loops": [ /* 既有俯视轮廓结构，不变 */ ] },
      "mesh": {
        "meshIdentity": "mesh:model-a:lod1:9f3ac21b",
        "lod": "lod1",
        "vertexCount": 24680,
        "triangleCount": 49356,
        "meshTransform": "local",
        "buffers": {
          "position": { "format": "float32x3", "byteOffset": 0,      "byteLength": 296160 },
          "normal":   { "format": "float32x3", "byteOffset": 296160, "byteLength": 296160 },
          "index":    { "format": "uint32",    "byteOffset": 592320, "byteLength": 592272 }
        },
        "blobId": "blob-7f3a9c",
        "totalBytes": 1184592,
        "chunkBytes": 4194304,
        "chunkCount": 1
      }
    }
  ],
  "truncated": false,
  "truncationReason": null
}
```

### 5.1 字段说明

| 字段 | 说明 |
|---|---|
| `bboxLocalMm` | **局部**坐标 bbox。世界 bbox 由宿主用 `worldMatrix` 变换得到 —— 避免两处真源 |
| `worldMatrix` | 行主序 16 元素；`meshTransform=world` 时为单位阵 |
| `meshIdentity` | 只标识可复用网格内容，不含 scene revision 或实例世界变换 |
| `buffers.*.byteOffset/byteLength` | 均相对于该实例 blob 的起始，非全局 |
| `blobId` | 二进制缓冲的取回句柄，见 §7 |
| `truncated` | 未能按请求返回完整内容时为 `true`，**不得静默截断** |
| `truncationReason` | `truncated=true` 时必填，如 `"budget_exceeded_downgraded_to_outline_only"` |

### 5.2 允许的 `format` 取值

```text
position   float32x3
normal     float32x3
index      uint16 | uint32   （顶点数 ≤ 65535 时允许 uint16）
```

首版**不含** UV 与顶点色 —— 3D 预览用平光着色即可；如后续需要，按 §9 版本化扩展。

## 6. `viewdataIdentity` 构成与失效规则

```text
格式：vd:<sceneRevision>:<instanceId>:<lod>:<sourceContentHash8>
```

**失效条件（任一命中即失效，宿主须重新请求）**：

| 事件 | 是否失效 | 说明 |
|---|:--:|---|
| `sceneRevision` 变化 | ✅ 失效 | identity 中含 revision |
| 实例被删除 / 更换源模型 | ✅ 失效 | `instanceId` 或 `sourceContentHash` 变 |
| 请求不同 `lod` | ✅ 失效 | identity 中含 lod |
| 源资产内容变化（重新导入）| ✅ 失效 | `sourceContentHash` 变 |
| **仅 `worldMatrix` 变化**（移动/旋转/缩放实例）| ❌ **不失效** | **网格缓冲可复用，只更新矩阵** |

> 最后一行是整份规格里最重要的一条。它决定了 3D 拖拽能否做到零跨 DLL 调用（UI-M1 / UI-M7）。
>
> **实现注意**：`sceneRevision` 会随变换递增，因此 `viewdataIdentity` 会变，但**网格部分未变**。
> 宿主应按 `meshIdentity` 缓存网格，而非按整个 `viewdataIdentity` 缓存。
> `viewdataIdentity` 标识场景响应快照，`meshIdentity` 标识可跨实例变换复用的网格内容；两者不得混用。

## 7. 大缓冲传输策略（🔴 不新增导出符号）

**约束（A）**：SPI 的 11 个 `pm_*` 导出**已冻结**，不得新增 `pm_get_blob` 之类的符号。

**方案**：blob 取回作为 `scene.get_viewdata` 的子操作走既有
`pm_submit` / `pm_poll` / `pm_result` 通道，**不新增第 16 项能力，也不新增导出符号**。

```jsonc
// 请求
{
  "capability": "scene.get_viewdata",
  "operation": "read_blob",
  "blobId": "blob-7f3a9c",
  "chunkIndex": 0            // 0 .. chunkCount-1
}
// 结果：二进制块经 pm_result 的缓冲三态协议（out_required）返回
```

三条约束：

```text
① chunkBytes 默认 4 MiB，由模块决定并在响应中声明；宿主不得假设
② blob 生命周期绑定 meshIdentity 与模块 TTL/LRU；仅 worldMatrix 或 sceneRevision 变化
   不使同一 meshIdentity 的 blob 失效。源模型、LOD 或内容哈希变化时失效，
   再取返回 PM-SLICER-VIEWDATA-STALE
③ 模块须限制同时存活的 blob 数量与总内存，超限时按 LRU 回收最旧的；
   被回收的 blobId 同样返回 STALE，宿主重新请求即可
```

> **为什么不一次性走 `pm_result` 返回整个网格**：现有缓冲三态协议（先探长度、再分配、再取）
> 是为中小 JSON 结果设计的。几十 MB 网格一次性返回有两个实际问题 ——
> 探测与取回两次调用之间状态可能失效；宿主无法增量渲染。分块同时解决这两点。

## 8. 错误码

| 错误码 | 触发条件 |
|---|---|
| `PM-SLICER-VIEWDATA-STALE` | `expectedSceneRevision` 不符，或 blobId 已失效/被回收 |
| `PM-SLICER-VIEWDATA-BUDGET` | `maxBytes` 小到连 `outline_only` 都放不下 |
| `PM-SLICER-PROFILE-0031` | `lod` / `meshTransform` / `content` 取值非法（复用既有参数越界码）|

前两个为新增，需在 14A-01 的 `print_module_spi.h` 错误码表中登记。

## 9. 版本化与扩展

```text
本 DTO 随 PM_SPI_VERSION 一同版本化。
新增【可选】字段  → minor 递增，宿主须忽略未知字段
删改既有字段语义  → major 递增，需重新协商
```

预留但**首版不实现**的扩展点：UV 坐标、顶点色、逐面材质分组、切片层预览网格。
这些若将来需要，按 minor 追加，不破坏已交付 ABI —— 这正是本规格要提前占位的原因。

## 10. 对实现的最小要求（首版）

```text
必须实现  bbox + outline + viewdataIdentity + 完整错误码 + truncated 语义
可以缺席  mesh 实际数据（返回 content 不含 mesh，或 truncated=true 说明未实现）
不得偏离  §1 全局约定、§3 local 语义、§6 失效规则、§7 不新增导出符号
```

**契约完整 ≠ 实现完整。** 首版允许只返回俯视轮廓，但**字段与语义必须按本规格冻结**，
以免 3D 视角落地时被迫改动已交付 ABI。

## 11. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-04 | v1.0 | 首版。填补 `scene.get_viewdata` 网格 DTO 空白：定义全局约定、请求/响应 DTO、`local`+`worldMatrix` 实例变换语义及其三条理由、LOD 分级与 auto 选择规则、`viewdataIdentity` 构成与失效表（含"worldMatrix 变化不失效"这一关键授权）、**在不新增导出符号前提下**的 blob 分块传输方案、两个新增错误码、版本化规则与首版最小实现要求 |
| 2026-08-05 | v1.1 | Stage 14 开工基线收口：新增独立 `meshIdentity`，消除 scene revision 与网格缓存生命周期冲突；blob 读取改为 `scene.get_viewdata` 子操作，保持 15 项能力与 11 个导出符号不变 |
