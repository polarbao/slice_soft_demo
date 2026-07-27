# DOC_DECISION 12E-09A/09B Qt 任务顺序与职责边界

> 状态：APPROVED / 09B、09C COMPLETE / NEXT 09A-02
> 日期：2026-07-23
> 决策范围：12E-09A diagnostic UI、12E-09B production UI、12E-09C DPI、12E-10 阶段收口

## 1. 决策

12E-08D-01..06 完成后，当前生产主线进入 `12E-09B`。`12E-09A` 不是 09B 的版本前置，
而是 08D 准入前即可独立推进的诊断 UI 支线。

```text
生产主线：12E-08D -> 12E-09B -> 12E-09C -> 12E-10
诊断支线：12E-09A-01 COMPLETE -> 12E-09A-02..06
阶段收口：12E-10A 同时依赖 09A-05、09B-05 与 09C；12E-10D 等待 10A/10B/10C
```

因此：

```text
12E-09B-01..06：COMPLETE；
12E-09C-01..06：COMPLETE；
当前下一准备任务：补齐 09A 执行级文档；随后执行 12E-09A-02；
12E-09A-02..06：保持未完成，按顺序执行；
12E-09A-05：仍是 12E-10A 的明确前置；
09A 与 09B 不得重复拥有同一配置字段或同一控件。
```

## 2. 为什么不是先完成 09A

09A 的目标是让用户读取和编辑 `global_surface_shell` 诊断结果，不改变普通生产按钮语义。09B 的目标是
在 08D 已准入的能力范围内开放 Legacy/Global 产品模式。两者按职责而不是字母顺序排列：

| 范围 | 09A 所有权 | 09B 所有权 |
|---|---|---|
| Diagnostic Facade | 当前模型 diagnostic DTO、未评估值、阻断原因 | 只消费，不复制拓扑规则 |
| Effective Config | width、modelFill.material、派生阈值和诊断状态 | requested/effective mode、生产 Profile、capability lock |
| 控件 | 纹理宽度、填充材料、诊断状态 | 产品模式选择、能力限制、资源提示 |
| Worker | 模型诊断、取消、生命周期 | 生产切片进程、失败和结果绑定 |
| Preview | Texture/Fill/Partition 同层诊断预览 | 生产 package/preview/report 的模式来源与结果状态 |
| Smoke | 诊断流程和同层预览 | 双模式生产入口、无静默回退和完整 TIFF/RIP |

09B 不得借“吸收 09A”为由跳过 09A 的诊断合同；09A 也不得提前开放 Global 生产写包。

## 3. 09B 准入基线

已满足：

```text
12E-08D-01..06 COMPLETE；
legacy 默认生产路径可用；
global_surface_shell_restricted_candidate 显式候选 GO；
global_surface_shell_material_parity_candidate 显式候选 GO；
两候选均有 0.01 mm TIFF、manifest、preview/report 和 RIP strict 证据；
无静默 fallback；
用户已明确允许继续 12E 后续任务。
```

仍需在 09B UI 中披露：

```text
Global 只能显式选择；
Global 不能默认替换 Legacy；
当前 Global 总耗时约为 Legacy 的 4.82x~8.59x；
当前 Global 峰值内存约为 Legacy 的 8.19x~8.74x；
复杂浮雕 aishen/meigui/titian 仍存在 0/3 strict-PASS 覆盖缺口；
实际模型 topology blocker 必须 fail-closed。
```

上述倍数是当前参考矩阵证据，不是永久产品 SLA。UI 应显示“资源开销较高”和本次实际测量值，
不能把固定倍数硬编码成运行结果。

## 4. 固定产品边界

```text
产品模式只有 legacy / global_surface_shell；
OpenVDB 不是第三种产品模式；
Legacy 保持默认；
Global 仅对准入 Profile 开放；
Global 阻断时不得自动切换 Legacy；
repair 保持默认关闭；
协议保持 p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print；
两种模式共用现有 RGBWSV writer、manifest、preview/report 和 RIP Reader。
```

## 5. 后续顺序

```text
09B-01 能力目录与 UI DTO（COMPLETE）；
09B-02 Production Effective Config；
09B-03 中文选择器与能力锁定；
09B-04 一键切片路由、准入与 no-fallback；
09B-05 生产结果、preview/report 与资源提示；
09B-06 Smoke、回归、用户文档和状态收口；
09C-01..06 X/Y DPI、Reader、两引擎、光油、Qt 与物理比例 preview；
09A-02..06 按独立授权继续；
12E-10 在 09B-06、09C 及其对应 09A 依赖完成后收口。
```

## 6. 回滚

任一 09B Gate 失败时：

```text
隐藏或禁用 Global 产品入口；
保留 Legacy 默认入口；
保留高级 OpenVDB 诊断入口；
不修改长期 Profile fixture；
不写入 fallbackApplied=true；
不改变生产 TIFF 协议。
```

## 7. 2026-07-24 Stage 13 新依赖

新增模型俯视、多模型场景和 TIFF 原生预览需求后，职责保持不变，但执行顺序增加两个前置：

```text
13A-01/13B-01 先冻结 ModelInstance/scene identity；
09A-02 随后兼容 single_model/scene；
13C-03 在 09A-05 前提供 TIFF 原生生产底图；
09A-05 只叠加 Texture/Fill/Partition 等诊断语义；
12E-10A 最后验证同层一致性。
```

Stage 13B 的多模型联合切片不纳入 09A，也不修改 09B 的 Legacy/Global 产品模式所有权。
