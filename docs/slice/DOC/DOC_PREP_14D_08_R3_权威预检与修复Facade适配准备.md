# DOC_PREP_14D-08-R3 权威预检与修复 Facade 适配准备

> 编制日期：2026-08-06
>
> 对应任务：`14D-08-R3`
>
> 文档状态：`PREPARATION_GATE=PASS_WITH_BLOCKED_IMPLEMENTATION`
>
> 父任务状态：`14D-08=BLOCKED`

## 1. 准备结论

R3 的职责可以冻结，但现有代码还不能直接注册 `geometry.preflight.full` 或
`geometry.repair` 的生产 executor。两个公开 Facade 目前只有抽象接口；既有预检服务面向单一
Profile/模型，既有修复服务是低层保守处理，不等于多模型 scene 的权威预检和修复后严格复检。

因此 R3 拆分为：

| 子任务 | 内容 | 状态 |
|---|---|---|
| `14D-08-R3-00` | DTO/服务/错误和证据映射审计 | **READY / 本文完成准备** |
| `14D-08-R3-01A` | scene-wide authoritative full preflight service | **PREPARATION REQUIRED** |
| `14D-08-R3-01B` | `PreflightFullFacade` 工厂和 Worker executor | **BLOCKED_BY_R3_01A** |
| `14D-08-R3-02A` | repair DTO、输出资产和 strict 复检合同冻结 | **PREPARATION REQUIRED** |
| `14D-08-R3-02B` | `RepairFacade` 工厂和 Worker executor | **BLOCKED_BY_R3_02A** |

## 2. 当前代码事实

1. `IPreflightFullFacade`/`IRepairFacade` 已定义 Qt-free API，但没有具体 factory。
2. `ModelPreflightService` 从一个 Profile 配置出发，面向单模型，不能证明 committed scene 中所有
   visible instance 和其 effective transform 都已检查。
3. `TransformedModelPreflight` 和 scene admission 能提供部分可复用能力，但尚无单一 service 聚合
   全场景资源、实例、拓扑和材料结论。
4. `MeshRepairService` 是低层保守修复组件；confirmed self-intersection、non-manifold 和证据不完整
   仍必须阻断，不能因调用了 repair 就改为 PASS。
5. `WorkerJobDispatcher` 已具备注入式 executor 和协作取消边界；R3 必须复用该入口。

## 3. R3-01 权威 full preflight

### 3.1 必须覆盖的对象

权威 scene preflight 必须以 R2 物化后的 committed scene 为唯一输入，按稳定顺序检查：

1. scene schema、revision、sceneHash、device build volume、resolved Profile；
2. 每个 resource scope 和每个 model source 的存在性、scope containment、hash/identity；
3. 每个 visible instance 的 model reference、effective transform、effective bounds；
4. 应用 effective transform 后的完整几何拓扑、边界、non-manifold、winding、duplicate、
   self-intersection 和预算完成度；
5. 全场景碰撞/越界、Profile 一致性和生产 admission；
6. 结果按 scene/model/instance id 稳定排序并聚合为公开 DTO。

不得通过循环改写一个 Profile 的 `input.modelPath` 来复用单模型服务，这会丢失 scene identity、
instance transform、资源范围和并发安全语义。

### 3.2 authoritative 规则

只有以下条件全部满足时才允许 `authoritative=true`：

- 所有 visible instance 均完成完整检查；
- 无 budget/resource incomplete；
- 未取消、未超时；
- 没有被降级或跳过的 required diagnostic；
- 结果绑定 committed sceneHash 和 effective transform identity。

`budget_exceeded`、`resource_blocked`、`cancelled`、`partial` 均不得表示 PASS。诊断 warning 与生产
admission 必须分开表达，不能把 warning 计数为权威失败，也不能隐藏 blocking issue。

### 3.3 R3-01A 输出

先建立 engine 内部 scene preflight service，再由 01B 做 API/Worker 适配。service 应返回：

- sceneHash、sceneRevision、authoritative、productionAdmitted；
- 稳定 issue code、severity、scene/model/instance identity、message/context；
- checked/blocked/skipped instance 计数；
- 完整性和取消状态；
- 不包含 Worker job/result 路径逻辑。

### 3.4 R3-01B 适配

`PreflightFullFacade` 只能做 DTO 转换、取消桥接和稳定错误映射。Worker executor 只负责把
`request.input` 物化为 Facade request、调用 factory，并封装 R1 result；不得实现几何算法。

## 4. R3-02 repair

### 4.1 当前阻塞

当前公开 `RepairRequest` 尚不足以完整表达：

- repair policy/version；
- 是否要求修复后 strict PASS；
- 输出 repaired asset 的 job 级绝对路径和所有权；
- source/repaired hash、attribute/UV/material 保真证据；
- 多模型 scene 中目标 model/instance 的选择规则。

在这些字段未通过受控合同映射冻结前，不能让 Worker 猜测默认修复策略。

### 4.2 必须保持的修复边界

1. 默认 strict/no-repair 行为不变。
2. 只允许已批准的保守修复操作；不得自动重建复杂自相交模型。
3. 修复后必须重新运行完整 strict preflight。
4. `manual_repair_required`、confirmed/coplanar self-intersection、残留 non-manifold、属性证据不完整
   必须 fail-closed。
5. repaired asset 和 evidence 必须写入 job-owned staging；最终发布继续归 14D-05。
6. repair 成功不得隐式触发 slice，也不得修改原始模型文件。

## 5. 稳定错误映射

| 类别 | 建议稳定码 | 约束 |
|---|---|---|
| scene/instance identity stale | `PM-SLICER-LAYOUT-0022` | 不运行预检/修复 |
| Profile/策略冲突 | `PM-SLICER-PROFILE-0030/0031` | 不猜默认策略 |
| 模型资源缺失/越界 | `PM-SLICER-INPUT-0001` | 现有稳定码定义为模型文件不存在或不可读；不扫描 scope 外路径 |
| topology/admission blocked | 既有 geometry/admission 稳定码 | 保留原始 issue 投影 |
| 取消 | `PM-SLICER-CANCELLED-0070` | authoritative=false，无发布 |
| 输出失败 | `PM-SLICER-OUTPUT-0050` | 不报告 repair success |
| 未分类内部失败 | `PM-SLICER-INTERNAL-0099` | executor 边界转换 |

## 6. 文件所有权建议

```text
src/slicer_core/preflight/ScenePreflightService.h/.cpp
src/slicer_core/engine/ProductionPreflightFullFacadeFactory.h/.cpp
src/slicer_core/engine/ProductionRepairFacadeFactory.h/.cpp
apps/slicer_worker/preflight/WorkerPreflightExecutor.h/.cpp
apps/slicer_worker/repair/WorkerRepairExecutor.h/.cpp
tests/stage14d_08_r3/*
```

Facade 接口继续位于 base；具体 service/factory/executor 位于 engine-linked target。Qt、WorkerClient、
TIFF Writer、UI 和模块 DLL 不进入这些实现文件。

## 7. 验收边界

### 7.1 R3-01

- 单模型和多模型 scene 均覆盖每个 visible instance；
- transform 后检测结果绑定 instance identity；
- blocked、budget incomplete、cancelled 均 `authoritative=false` 或 `productionAdmitted=false`；
- 同输入双运行稳定投影一致；
- Worker 与直接 Facade 的业务结果一致；
- 不创建生产 package。

### 7.2 R3-02

- 允许的保守修复产出新资产和完整 evidence；
- 修复后 strict 必须重新运行并决定 admission；
- confirmed self-intersection 等现有阻断不被降级；
- 原资产字节不变；
- 取消、失败、崩溃不留下可误认成已发布的 repaired asset。

## 8. 并行与顺序

- R3-00 可与 R2-01、14D-07-R0 并行。
- R3-01A 与 R3-02A 可以在合同/文件所有权隔离后并行准备，但两个 factory 接线必须串行修改
  production registry 和根 CMake。
- R3-01B 是 R2-02 的硬前置；没有权威 full preflight 时 slice executor 不得注册。
- R3-02 不阻塞最窄 slice 路径，但阻塞父任务 14D-08 的三能力 COMPLETE。
- 14D-05 仍负责 artifact publish/recovery，R3 不自行建立第二套发布器。

## 9. 后续准备缺口

R3-01A 开发前必须补齐：

1. scene-wide service DTO 与稳定 issue 聚合表；
2. visible/hidden、同 model 多 instance、transform、取消和预算负例；
3. 与 `ProductionAdmissionPolicy` 的唯一调用关系；
4. target/source 所有权和定向测试命令。

R3-02A 开发前必须补齐：

1. file-contract `input` 到 repair request 的字段级映射；
2. policy/version、目标模型、输出资产与 evidence 路径合同；
3. 修复后 strict 复检和失败清理顺序；
4. 是否需要 `file_contract_v1` minor 兼容修订的明确结论。

## 10. 门禁结论

```text
14D_08_R3_PREPARATION_GATE=PASS_WITH_BLOCKED_IMPLEMENTATION
14D_08_R3_00_PREPARATION_GATE=PASS
14D_08_R3_01A_PREPARATION_GATE=PREPARATION_REQUIRED
14D_08_R3_01B_PREPARATION_GATE=BLOCKED_BY_R3_01A
14D_08_R3_02A_PREPARATION_GATE=PREPARATION_REQUIRED
14D_08_R3_02B_PREPARATION_GATE=BLOCKED_BY_R3_02A
14D_08_PARENT_GATE=BLOCKED
```

该结论关闭了“R3 做什么”的歧义，但没有把缺少实现的 Facade 伪记为可用。下一批可并行准备
R3-01A 与 R3-02A；当前开发关键路径仍是 `14D-08-R2-01 -> R3-01A/01B -> R2-02`。
