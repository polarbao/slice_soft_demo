# DOC_CHECKLIST_13 未决产品输入与阶段 Gate

> 文档状态：CURRENT OPEN-INPUT REGISTER
> 版本：v1.1
> 日期：2026-07-27

## 1. 使用方式

本表区分“工程文档不完整”和“外部产品/设备事实尚未提供”。未知值不得被 fixture 或开发者假设替代。

| 输入 | 当前状态 | 临时工程规则 | 阻断任务 | Owner |
|---|---|---|---|---|
| buildVolume.widthMm/heightMm | OPEN | schema 使用 optional/unresolved；fixture 显式标记 | 13B-04 production、13B-07 GO | 设备/Profile |
| 场景原点、机器 X/Y 正方向 | OPEN | UI 使用软件坐标 +X 右、+Y 上；生产映射 unresolved | 13B-04 production | 设备/Profile |
| 多实例是否允许不同材料 Profile | OPEN | P0 `scene_profile_only`，不同 Profile fail-closed | 不阻断 13B-01；阻断未来 mixed-profile | 产品/工艺 |
| 22 实例性能预算 | OPEN | 先记录实测，不预设 PASS 阈值 | 13B-07 GO | 产品/性能 |
| 最大实例数是否长期固定 22 | OPEN / P0 FROZEN | P0 固定 22 | 不阻断 P0 | 产品 |
| 自动 nesting 优先级 | DEFERRED | 13B-R4，P0 只做 grid | 不阻断 P0 | 产品 |
| 中期 3D 后端 | DEFERRED TO SPIKE | 13A-R1 使用 Qt 2D；R2 比较 VTK/QOpenGLWidget | 13A-R2/R3 | 架构/产品 |

## 2. 当前可直接开发

```text
Stage 13 P0 需求/设计/验证和 17 个近程原子任务准备：COMPLETE；
13A-01..05：COMPLETE；
13B-01..06：COMPLETE；
13B-04：FUNCTIONAL FIXTURE PASS / PRODUCTION INPUT OPEN；
13B-06：FIXTURE COMPLETE / PRODUCTION INPUT OPEN；
13B-07：PREPARATION，功能矩阵不被外部输入阻断，production GO 继续等待；
13C-01：READY，但按单贡献者顺序排在 identity wave 后；
12E-09A-02：COMPLETE。
```

“准备完整”不关闭本表的外部输入，也不代表任何 Stage 13 代码或生产证据已经存在。

## 3. 当前不可宣称

```text
Stage 13 全阶段生产就绪；
22 实例满足正式设备幅面；
多实例混合 MaterialProcessProfile；
多模型正式性能 SLA；
VTK、Qt3D 或 QOpenGLWidget 已选型；
自动 nesting 或跨模型联合支撑已设计完成。
```

## 4. 输入关闭要求

每项关闭时必须记录：

```text
决策值；
来源文档/Profile/设备版本；
生效日期；
兼容和迁移规则；
受影响任务；
对应验证 fixture 或真实设备证据。
```
