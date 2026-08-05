# SliceSoft 三车道交互合同

> 合同版本：1.0
> 机器可读真源：`contracts/slicer_three_lane_contract.json`

## 1. Transient 车道

拖拽、旋转、缩放进行中只修改宿主本地临时矩阵、近似 bbox 和已缓存 ViewData。
鼠标移动期间跨模块调用必须恒为 0，不改变模块场景、不递增 revision，也不能进入切片。

## 2. Commit 车道

鼠标释放、数值确认、Undo/Redo、自动定向和排版确认通过 `scene.apply_operation` 原子提交。
请求必须包含：

```text
operationId
currentSceneRevision
expectedSceneRevision
operations[]
```

正常请求中 `currentSceneRevision == expectedSceneRevision`，且两者必须等于模块权威 revision。
全部 operation 成功后 revision 恰好递增 1；任一失败则不应用任何 operation。

### 2.1 幂等

`operationId` 在“模块实例 + scene”范围内唯一。同一 ID 和同一规范化请求重试时返回首次结果，
不得再次递增 revision。相同 ID 携带不同 payload 时返回 `PM-SLICER-PROFILE-0031`，不改变场景。
幂等记录保留到 scene release 或 module destroy。

规范化请求不包含 `correlationId`，但包含场景身份、两项 revision 和 operations；对象键按字典序，
数字使用可往返的最短 JSON 表示。宿主改变操作内容时必须创建新的 `operationId`。

### 2.2 Stale 回滚

revision 不匹配返回 `PM-SLICER-LAYOUT-0022`，不得部分应用或静默覆盖。宿主必须按顺序：

1. 丢弃 transient 临时状态；
2. 调用 `scene.get_snapshot`；
3. 用权威 snapshot/revision 替换本地快照；
4. 重建本地 ViewData；
5. 若用户仍需操作，使用新的 `operationId` 重新提交。

禁止在没有回读快照时自动重试，也禁止用同一 `operationId` 发送改变后的 payload。

## 3. Production 车道

`slice.rgbwsv` 只接受 Commit 成功后返回的 `sceneHash`。Worker 独立重建完整 scene，并重新执行
full preflight；DLL 的 fast preflight 只能作为提示，不能作为生产准入真值。sceneHash 过期时返回
`PM-SLICER-LAYOUT-0022`，不得切片 transient/过期状态。

生产仍固定输出 `p0.rgbwsv.2`，Legacy 为默认引擎；未显式请求时不得改用 OpenVDB，任何引擎失败
均不得静默回退。

## 4. 实现边界

本合同冻结行为，不在 14A 阶段实现 UI、facade 或模块状态存储。14B/14C 的实现必须通过以下门禁：

```text
同 operationId 重试不重复应用
同 operationId 不同 payload 被拒绝
stale 不改变权威场景
回读 snapshot 后宿主与模块状态一致
mouse-move 跨 DLL 调用为 0
未 Commit 的 sceneHash 无法进入 Production
```
