# DOC PREP HOSTFLOW H-B-03 实例变换与规则排版准备

> 状态：**IMPLEMENTED / VERIFIED**
> 日期：2026-08-07
> 任务：HOSTFLOW H-B-03
> 范围：多选实例精确移动、旋转、缩放、镜像，以及模块权威规则排版入口。

## 1. 准入与边界

H-B-03 所需的 H-B-02 选择集与 H-A-04 `applyGridLayout` 均已完成。实现只使用公开
SPI v1 和冻结的 `scene.apply_operation`，不新增导出、能力或 DTO 字段，不修改
RGBWSV/TIFF 协议，也不修改 `apps/slicer_debug_ui`。

主干行为基线为 `ModelTransformPanel` 与 `SceneLayoutPanel`。参考宿主只补齐核心作业入口：

- 精确输入 X/Y/Z 增量、绕 Z 旋转角度和等比缩放因子；
- 对当前多选实例执行 X/Y 镜像；
- 配置 1..11 列、1..2 行、列间/行间净距并执行规则排版；
- 参数编辑和选择联动保持宿主本地，不跨 DLL；
- 点击提交后，一个用户命令只产生一次原子 Commit 和一次 revision 递增。

## 2. 业务流程与三车道边界

```text
Local/Transient：选择实例、编辑数值、切换镜像命令 -> 0 次 DLL 调用
Commit：点击“提交选中实例变换” -> transform operations[] -> revision +1
Commit：点击“执行规则排版” -> applyGridLayout -> revision +1
Recovery：仅 Stale/显式恢复使用 snapshot；正常 Commit 不追加 snapshot
```

规则排版算法归切片能力模块所有，宿主只提交冻结参数：`grid`、`edge_clearance`、
`row_major`、行列上限和净距。容量不足、未知实例、非法数值和无变化命令在宿主可判定时
直接 fail-closed，不允许产生部分提交。

## 3. 实现落点

| 文件 | 责任 |
|---|---|
| `apps/slicer_ui_host_sim/HostTransformLayoutPanel.*` | 精确变换、镜像和规则排版控件；本地编辑零模块调用 |
| `apps/slicer_ui_host_sim/HostModelImportWorkflowScene.cpp` | 生成 transform/applyGridLayout operations 并执行单次原子 Commit |
| `apps/slicer_ui_host_sim/HostMainWindowScene.cpp` | 场景编辑接线、命令互锁、结果与错误展示 |
| `apps/slicer_ui_host_sim/SceneInteractionController.cpp` | snapshot 响应返回 sceneHandle 时采用权威句柄，恢复既有 14E-03 回归 |
| `tests/hostflow/HostTransformLayoutPanelTests.cpp` | 多选变换、规则排版、零调用编辑、单 revision 与容量负例 |

## 4. 验证结果

Debug 与 Release 均通过八项联合门禁：

```text
slicer_stage14e02_qt_host_boundary_test
hostflow_ha03_qt_end_to_end
hostflow_hb01_model_import
hostflow_hb01_import_ui_smoke
hostflow_hb02_model_list_selection
hostflow_hb03_transform_layout
slicer_stage14e03_interaction_contract_test
slicer_stage14e04d_dual_view_contract_test
```

两种配置均为 `8/8 PASS`。H-B-03 定向测试同时确认：本地参数编辑为零 DLL 调用；
两实例组合变换只递增一次 revision；规则排版只递增一次 revision 且无碰撞/越界；容量不足
负例不跨 DLL 且 revision 不变。

主干 A/B 行为基线使用本次源码构建的 `slicer_debug_ui` 执行：

```text
PASS model-top-view-transform x/y/rotate/scale/center/reset/locked/dirty/latest-generation
PASS model-transform-preflight mirror-x/y/source/effective/latest-revision/global-blocked-viewable/three-window-sizes
PASS scene-grid-layout capacity/11x2/edge-gap/restore/workspace/three-window-sizes
```

## 5. 后续与并行边界

- 下一卡为 H-B-04：经 ABI 查询 Profile 和能力标签，禁止直接读取场景注册 JSON。
- H-B-04 会继续修改参考宿主窗口和 session 上下文；单工作树中不与 H-B-03 并行编码。
- H-B-04 的只读 DTO/能力查询审计与 fixture 准备可并行，但实现应在 H-B-03 提交后开始。
- H-B-04..08 为 Profile -> 参数 -> 切片 -> 结果 -> 持久化的依赖链，不能并行越过前置。
- H-C 文档/移植清单可提前只读准备；H-C-01 的实现仍硬依赖 H-B-07。
