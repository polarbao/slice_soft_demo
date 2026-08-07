# DOC_DECISION_14F-R1 HOSTFLOW 场景生命周期合同受控修订

> 文档状态：**ACCEPTED / USER AUTHORIZED / CONTRACT REVISION ACTIVE**
> 版本：v1.0
> 日期：2026-08-07
> 授权项：`HQ-01`
> 当前实施卡：`H-A-01`

## 1. Context

Stage 14F 已冻结 `PM_SPI_VERSION=1`、11 个 `pm_*` 导出和 15 项能力。现有
`model.import` 只返回 `modelId`，而 `scene.apply_operation` 仅接受四种已存在实例的变换，
宿主无法在不知道内部 scene schema 的前提下把模型加入场景或删除实例。

2026-08-07，用户明确授权 `HQ-01`，允许以受控 minor 修订方式执行 `H-A-01`。本决策是
`DOC_DECISION_14F_外部验证延期与接口冻结.md` 要求的新一轮 R1 修订记录，不回写或弱化原冻结证据。

准备审查同时发现原 HOSTFLOW 草案中“用 `pm_release` 关闭场景”的表述不成立：
`pm_release` 的参数是 `pm_job_t*`，只释放作业句柄，不能接收 JSON 中的整数 `sceneHandle`。

## 2. Decision

1. `contracts/slicer_capability_dtos.*` 从 v1.4 升级为 v1.5。
2. 不增加能力或导出，只扩展 `scene.apply_operation.operations[].type`：
   `addInstance`、`removeInstance` 加既有四种变换。
3. `addInstance` 使用已由 `model.import` 注册且尚未释放的 `modelId`；可选
   `assignInstanceId` 与 `initialTransform`。未指定实例 id 时由模块分配唯一 id。
4. `removeInstance` 只删除场景实例，不隐式执行 `model.release`。
5. 仅当请求未携带 `sceneHandle`、`scene` 缺省或为 `{}`，且操作数组包含
   `addInstance` 时，隐式创建空场景；请求 revision 必须均为 0，响应必须返回 `sceneHandle`。
6. 不含新枚举的旧请求保持既有语义。特别是“无 handle、无 scene、仅旧变换”的请求仍按旧逻辑失败，
   不得被解释为隐式建场景；非空 inline `scene` 的既有行为也保持不变。
7. 场景会话的生命周期绑定到 `pm_module_t`，由 `pm_destroy` 统一清理。H-A-01 不新增显式
   per-scene close；若后续实测证明需要提前回收，必须另立受控修订，不能误用 `pm_release`。
8. H-A-01 只冻结合同和自动门禁，不实现 Facade/Adapter 行为；运行时实现属于 H-A-02。

## 3. Contract Details

### 3.1 `addInstance`

```json
{
  "type": "addInstance",
  "modelId": "model-42",
  "assignInstanceId": "instance-42",
  "initialTransform": {
    "translateXMm": 0.0,
    "translateYMm": 0.0,
    "rotateZDeg": 0.0,
    "uniformScale": 1.0,
    "mirrorX": false,
    "mirrorY": false
  }
}
```

`assignInstanceId` 和 `initialTransform` 均可缺省；缺省变换为 identity。`instanceId` 不作为
`addInstance` 的输入字段，避免和 `assignInstanceId` 产生双重来源。

### 3.2 `removeInstance`

```json
{
  "type": "removeInstance",
  "instanceId": "instance-42"
}
```

删除不存在的实例、重复实例 id、未导入或已释放的 `modelId` 必须 fail-closed；具体实现和负例
由 H-A-02 落地。

### 3.3 隐式场景创建

```text
触发条件：sceneHandle 缺省 + scene 缺省或 {} + operations 至少包含一个 addInstance
revision：currentSceneRevision=0 且 expectedSceneRevision=0
响应：sceneHandle 条件必填
后续请求：使用返回的 sceneHandle 和最新 revision
```

多个 operation 按数组顺序在同一个原子提交中执行；任一操作失败时不得暴露部分成功场景。

## 4. Alternatives Considered

| 方案 | 结论 | 原因 |
|---|---|---|
| 新增 `scene.create` / `scene.close` 能力 | 不采纳 | 会改变冻结的 15 项能力面并增加打印侧回签范围 |
| 宿主传完整 scene JSON | 不采纳 | 泄露内部 schema，违背最少改动移植目标 |
| 用 `pm_release` 关闭场景 | 不采纳 | ABI 类型不匹配；`pm_release` 只接受 `pm_job_t*` |
| 在 `scene.apply_operation` 增加 add/remove | 采纳 | 保持 SPI v1、11 导出和 15 能力不变，且可向前兼容 |

## 5. Consequences

- 正向：H-A-02 后宿主可仅凭 `modelId` 建立场景，不再拼装内部 scene JSON。
- 正向：旧四种变换的字段和语义保持不变。
- 代价：模块实例存活期间需管理 scene session；显式单场景回收暂不提供。
- 边界：v1.5 合同先于实现冻结；H-A-02 完成前，新枚举不得宣称运行时可用。
- 外部状态：打印侧 ACK 继续为 `PENDING / DEFERRED`，不得写成 PASS。

## 6. Validation

H-A-01 必须验证：

```text
能力 DTO JSON 可解析；
能力数量精确为 15；
PM_SPI_VERSION 精确为 1；
pm_* 导出声明精确为 11；
add/remove 条件字段与隐式创建规则被机器测试锁定；
旧四种变换的类型顺序和字段规格保持不变；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 不变。
```

## 7. Follow-up

1. H-A-02：实现 `SceneFacade` / `SceneCapabilityAdapter`、幂等和负例。
2. H-A-04：另行受控加入 `applyGridLayout`，不得偷渡到 H-A-01。
3. H-A-03：完成空场景端到端闭环并重新运行 ABI、Worker、S1、S2 和交付包门禁。
4. 打印侧在外部验收恢复时，对 DTO v1.5 和场景会话语义进行书面 ACK。
