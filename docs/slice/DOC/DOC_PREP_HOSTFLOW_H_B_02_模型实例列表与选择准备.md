# DOC PREP HOSTFLOW H-B-02 模型实例列表与选择准备

> 状态：**IMPLEMENTED / VERIFIED**
> 日期：2026-08-07
> 任务：HOSTFLOW H-B-02
> 范围：模型实例列表、添加入口、原子删除、多选、全选与中央工作区选择联动。

## 1. 准入与边界

H-B-02 所需的 H-A-01 `removeInstance` 和 H-B-01 导入入场流程均已完成。实现只使用公开
SPI v1，不新增能力或导出，不修改 RGBWSV/TIFF 协议，也不修改 `apps/slicer_debug_ui`。

主干行为基线为 `ModelListPanel`：扩展选择、全选、删除、列表摘要和选择联动。参考宿主只移植
核心作业流程，不提前复制主干中的实例复制、可见性和锁定命令；这些命令不属于 H-B-02 卡面。

## 2. 业务流程

```text
添加：模型列表工具栏 -> H-B-01 导入流程 -> 列表追加并选中新实例
选择：单选 / Ctrl / Shift / 全选 -> 中央工作区显示稳定 instanceId
删除：收集选中 instanceId -> 一次 scene.apply_operation(removeInstance[])
     -> revision 只增加一次 -> 本地列表与中央选择同步更新
```

- 多选和工作区联动完全在宿主本地完成，不产生 DLL 调用。
- 删除使用单次原子 Commit；任一未知实例会在提交前 fail-closed，不允许部分删除。
- 删除成功后释放不再使用的导入模型资源；资源释放失败不回滚已提交的权威 scene revision。
- 场景可删除到零实例，但模块 session 和稳定 `sceneHandle` 保留，允许后续继续导入。

## 3. 实现落点

| 文件 | 责任 |
|---|---|
| `apps/slicer_ui_host_sim/HostModelListPanel.*` | 模型列表、扩展选择、全选、添加/删除命令和摘要 |
| `apps/slicer_ui_host_sim/HostModelImportWorkflow.*` | 批量 `removeInstance` 原子提交及实例/模型身份跟踪 |
| `apps/slicer_ui_host_sim/ViewWorkspaceWidget.*` | 本地选择状态展示，保持零 DLL 调用 |
| `apps/slicer_ui_host_sim/HostMainWindow.*` | 列表、工作流和中央视图之间的接线 |
| `tests/hostflow/HostModelListPanelTests.cpp` | OBJ/3MF 入场、全选、联动、批量删除与负例 |

## 4. 验证结果

参考宿主 Debug/Release 均通过六项联合门禁：

```text
slicer_stage14e02_qt_host_boundary_test
hostflow_ha03_qt_end_to_end
hostflow_hb01_model_import
hostflow_hb01_import_ui_smoke
hostflow_hb02_model_list_selection
slicer_stage14e04d_dual_view_contract_test
```

主干 A/B 行为基线使用本次源代码重新构建的 `slicer_debug_ui` 执行：

```text
slicer_debug_ui --ui-smoke-test --case multi-model-list --yes
PASS multi-model-list add/share/duplicate/visibility/lock/delete/selection/
     select-all/material-appearance/textured-import/auto-layout/three-window-sizes
```

## 5. 后续与并行边界

- 下一卡 H-B-03 在相同模型列表、HostMainWindow 和 scene revision 上增加变换/排版入口。
- H-B-04 也会修改 HostMainWindow 顶部和 session 上下文；单工作树中不与 H-B-03 并行编码。
- 可并行进行 H-B-04 的只读能力查询审计、Profile DTO 核对和夹具准备，但实现提交必须按原子卡
  串行，避免同一窗口和 session 状态出现竞争性设计。
