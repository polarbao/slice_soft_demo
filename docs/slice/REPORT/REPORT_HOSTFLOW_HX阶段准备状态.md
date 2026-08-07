# REPORT HOSTFLOW H-X 阶段准备状态

> 状态：**ACTIVE / H-A COMPLETE / H-B-01 COMPLETE / H-B-02 NEXT**
> 日期：2026-08-07
> 范围：HOSTFLOW H-A、H-B、H-C，不属于 Stage 14 编号任务。

## Current State

| 任务组 | 准备状态 | 当前可执行范围 | 阻断 |
|---|---|---|---|
| H-A 场景生命周期 | COMPLETE | H-A-01..04 全部完成；DTO 当前为 v1.7 | 无切片侧阻断 |
| H-B 宿主业务 UI | ACTIVE | H-B-01 已完成；H-B-02..08 的能力前置均已具备 | 后续各卡仍须显式启动 |
| H-C 移植交付 | NOT READY | 可提前建立文件清单模板 | H-C-01 等 H-B-07；H-C-02/03 继续依赖 H-C-01/H-B-07 |

H-A-02 已完成：Facade 支持 add/remove 与候选态原子提交；Adapter 支持既有 handle、inline scene、
受控隐式 scene 三条路径；import model resource 可映射到 scene authority；DTO v1.6 `sceneContext`
提供宿主权威 Profile/buildVolume。Debug/Release 两组测试均为 2/2 PASS。

H-A-04 已完成：DTO v1.7 在既有 `scene.apply_operation` 增加单操作 `applyGridLayout`，
复用 `GridLayoutPolicy` 完成稳定顺序、隐藏占位、锁定、10 mm 净距、11×2 和 22 实例排版；
成功只增加一次 revision，容量/参数/混批/replay 冲突均 fail-closed。Debug/Release 各 4/4 PASS。

H-A-03 已完成：纯 C 与 Qt 宿主都从空场景经 11 个导出完成 import、add、layout、transform、
slice 和 verify。宿主手工 scene builder 已移除；`scene.get_snapshot` 返回完整权威 scene 并由宿主
不透明透传给 Worker。源路径身份和 signed-zero scene hash 已收口。Debug/Release 联合回归各
5/5 PASS，合同与宿主边界门禁通过。

H-B-01 已完成：参考宿主增加 OBJ/3MF 文件选择，经公开 SPI 完成 `model.import`、
`scene.apply_operation(addInstance)` 与快速预检，并在右侧展示模型列表、导入元数据和预检问题。
缺失文件与不支持格式显式 fail-closed；Debug/Release 各 5/5 联合门禁通过。

## Target State

```text
H-A-01 → HQ-07/v1.6 → H-A-02 → H-A-04 → H-A-03
                                      ↓
H-B-01..08 核心业务宿主 → H-C-01..03 移植交付物
```

H-A-03 已证明宿主只经 11 个导出实现：

```text
空场景 → import → addInstance → transform/layout → slice → verify
```

宿主不得构造内部 scene JSON；SPI v1、11 导出、15 能力、p0.rgbwsv.2 和 TIFF 均不变。

## Historical State

- Stage 14F 切片侧已收口，打印侧外部验收延期。
- H-A-01 在延期窗口内完成 add/remove 合同修订，但尚未实现运行时。
- 原 HOSTFLOW 草案把默认构建体积当作可直接使用的实现输入；当前审计已纠正为宿主权威。

## Remaining Decisions

1. **HQ-07 已关闭**：用户接受 `sceneContext`，DTO 已受控提升到 v1.6。
2. **H-A-04 已关闭**：`applyGridLayout` 已独立实现并通过门禁，不新增能力或导出。
3. 打印侧 ACK 继续为 `PENDING / DEFERRED`，不得写成 PASS。

## Next Action

下一候选为 H-B-02；H-B-03 与 H-B-04 → H-B-08 同样已具备能力前置。按照专项
单卡规则，下一张卡仍须由用户显式启动。H-C-01/03 继续等待 H-B-07，打印侧 ACK 维持
`PENDING / DEFERRED`。
