# DOC_DECISION_14F-R2 HOSTFLOW 隐式场景初始化上下文受控修订

## Status

**ACCEPTED / USER AUTHORIZED**

> 日期：2026-08-07  
> 关联任务：HOSTFLOW H-A-02  
> 上游：`DOC_DECISION_14F_R1_HOSTFLOW场景生命周期合同受控修订.md`  
> 授权记录：用户于 2026-08-07 明确授予 HQ-07 及 H-A 后续开发权限。

## Context

H-A-01 已冻结 `addInstance`、`removeInstance` 和“首个 `addInstance` 可隐式创建场景”的
DTO v1.5。进入 H-A-02 代码审计后确认，当前合同缺少创建可生产场景所需的两个场景级输入：

1. `resolvedProfileId`。现有 `MultiModelScene` 和生产 Worker 都要求 scene、instance 与提交
   Profile 的材料身份一致；模块不能从 `modelId` 推断 Profile。
2. `buildVolume`。HOSTFLOW 执行指令已经冻结“构建体积由宿主提供，切片模块只消费和
   fail-closed 校验”，因此不能把 `230 x 100 x 60 mm` 的内部默认值冒充宿主权威。

若直接按 DTO v1.5 实现，只有三种错误选择：写死默认 Profile、写死默认构建体积，或生成无法
通过生产校验的场景。三者都会让 H-A-03 的“空场景到切片”闭环成为伪通过。

## Decision

建议在 H-A-02 实现前，将能力 DTO 合同从 v1.5 受控提升到 v1.6。SPI major、11 个导出和
15 项能力保持不变，仅为 `scene.apply_operation` 的隐式创建请求增加场景级对象：

```json
{
  "sceneContext": {
    "resolvedProfileId": "profile-id",
    "buildVolume": {
      "source": "device_profile",
      "widthMm": 230.0,
      "heightMm": 100.0,
      "zLimitMm": 60.0,
      "origin": "lower_left",
      "xDirection": "positive",
      "yDirection": "positive",
      "isFixture": false
    }
  }
}
```

冻结语义如下：

1. 仅在“无 `sceneHandle`、`scene` 缺省或 `{}`、operations 含 `addInstance`”的隐式创建路径中，
   `sceneContext` 条件必填。
2. `resolvedProfileId` 必须为非空字符串；模块原样写入 scene 和新增 instance，不读取仓库
   Profile JSON，也不根据名称猜测材料策略。
3. `buildVolume` 必须由宿主给出完整 canonical 对象；`source` 必须为 `device_profile`，
   `isFixture` 必须为 `false`，宽高必须为有限正数，Z 上限可选但出现时必须为有限正数。
4. 已有 `sceneHandle` 或非空 inline scene 请求禁止携带 `sceneContext`，避免两个权威源并存。
5. 模块生成 scene identity、resource scope、model source 和 instance identity；宿主仍不构造
   内部 scene JSON。
6. `operationId` 的 canonical fingerprint 必须包含完整 `sceneContext`。同一 operationId 改变
   Profile 或 buildVolume 时必须 fail-closed，不得复用旧结果。
7. 正常 Commit 响应保持 DTO v1.5 结构；隐式创建继续条件返回 `sceneHandle`。

## Alternatives Considered

| 方案 | 结论 | 原因 |
|---|---|---|
| 模块写死 `230 x 100 x 60` 和默认 Profile | 拒绝 | 违反 buildVolume 宿主权威；Profile 不能安全猜测 |
| 在 `pm_create(options_json)` 写入 Profile/buildVolume | 拒绝 | Profile 是场景/作业选择，不应成为模块实例全局状态；多场景会互相污染 |
| 宿主继续提交完整 inline scene | 拒绝 | 直接破坏 H-A-03“宿主不构造 scene JSON”的核心目标 |
| 把 Profile/buildVolume 放进每个 `addInstance` | 拒绝 | 两者是场景级属性，多实例请求会产生重复和冲突来源 |
| 增加 `sceneContext` | 建议采纳 | 保持既有能力与导出不变，同时让宿主权威输入显式、可校验、可幂等 |

## Consequences

- 正向：H-A-02 可以生成真正可进入 Worker 的场景，不需要内部默认值或名称推断。
- 正向：H-A-03 可证明空场景链路，并验证 Profile/buildVolume 与生产请求一致。
- 代价：DTO 合同从 v1.5 升为 v1.6，需要切片侧重新冻结并由打印侧延期回签时采用 v1.6。
- 兼容：旧 inline scene 和已有 handle 请求不变；新字段只在此前尚未实现的隐式创建路径条件必填。
- 边界：本修订不包含 `applyGridLayout`；H-A-04 仍需独立授权。

## Validation

授权后必须新增机器门禁：

```text
sceneContext 仅在 implicit_scene_create 条件必填；
resolvedProfileId 空值失败；
device_profile buildVolume 非法值失败；
已有 handle / 非空 scene 同时携带 sceneContext 失败；
operationId replay 包含 sceneContext identity；
PM_SPI_VERSION=1、11 导出、15 能力保持不变；
旧四种变换与非空 inline scene 回归不变。
```

## Follow-up

1. 用户明确授权 HQ-07 后执行合同 v1.6 修订和门禁。
2. 继续 H-A-02：实现 core add/remove、Adapter 隐式 session、实例 id 分配和负例。
3. 单独授权并执行 H-A-04，再进入 H-A-03 空场景端到端闭环。
