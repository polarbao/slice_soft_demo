# DOC PREP HOSTFLOW H-B-01 模型导入业务流程准备

> 状态：**IMPLEMENTED / VERIFIED**
> 日期：2026-08-07
> 任务：HOSTFLOW H-B-01
> 范围：参考宿主中的 OBJ/3MF 导入、实例入场、快速预检和结果展示。

## 1. 准入结论

H-B-01 的硬前置 H-A-03 已完成。参考宿主可只经公开 SPI v1 的 11 个导出完成
`model.import`、`scene.apply_operation(addInstance)` 和 `geometry.preflight`，无需构造或读取
切片内部 scene JSON。

本任务不修改 SPI 版本、导出函数数量、能力数量、RGBWSV 协议、TIFF 格式或生产切片策略；
也不修改 `apps/slicer_debug_ui`。

## 2. 业务流程

```text
文件对话框选择 OBJ/3MF
  -> model.import
  -> scene.apply_operation(addInstance)
  -> geometry.preflight(mode=fast)
  -> 模型列表、导入元数据和预检问题表同步刷新
```

- 首个模型通过宿主提供的 `sceneContext` 创建场景，后续模型复用模块返回的稳定
  `sceneHandle` 和 revision。
- `sceneContext` 中的 `host-reference-default` 与 230 x 100 x 60 mm 构建体积是当前参考宿主
  的显式输入，不是切片模块默认值。正式 Profile 与参数选择由 H-B-04/H-B-05 接管。
- 文件不存在、扩展名不支持、导入失败、实例入场失败和技术性预检失败均显式 fail-closed。
- 快速预检发现的模型问题以问题表展示；技术错误不会伪装成成功结果。

## 3. 实现落点

| 文件 | 责任 |
|---|---|
| `apps/slicer_ui_host_sim/HostModelImportWorkflow.*` | 公开 SPI 导入、实例入场、预检和场景 revision 管理 |
| `apps/slicer_ui_host_sim/HostMainWindow.*` | 文件选择、模型列表、元数据摘要和预检问题展示 |
| `tests/hostflow/HostModelImportWorkflowTests.cpp` | OBJ/3MF 正例与缺失文件 fail-closed |
| `apps/slicer_ui_host_sim/Main.cpp` | 离屏 UI smoke 入口 |
| `apps/slicer_ui_host_sim/CMakeLists.txt` | Debug/Release 构建与 CTest 注册 |

## 4. 验证夹具与验收

- OBJ：`samples/models/openvdb/surface_shell_cube_no_uv.obj`
- 3MF：`samples/models/3mf/single_rgb_cube_stored.3mf`
- 负例：不存在的 OBJ 路径，失败后场景 revision 不变化。
- UI smoke：导入按钮、模型列表和预检问题表存在且导入按钮可用。
- 宿主边界：`apps/slicer_ui_host_sim` 不依赖 `slicer_core`、`slicer_base` 或
  `slicer_engine` 内部头文件。

## 5. 验证结果

Debug 与 Release 均通过以下五项联合门禁：

```text
slicer_stage14e02_qt_host_boundary_test
hostflow_ha03_qt_end_to_end
hostflow_hb01_model_import
hostflow_hb01_import_ui_smoke
slicer_stage14e04d_dual_view_contract_test
```

H-B-01 验证期间同步修复两项回归：将能力覆盖报告生成从超长源文件拆出；场景交互控制器
采用 Adapter 返回的稳定 `sceneHandle`，不再把宿主可读的 `sceneId` 误当数值句柄。两项修复
不改变公开合同。

## 6. 后续边界

- H-B-02 接管实例增删、多选和选中联动，不在本任务内提前实现。
- H-B-03 接管变换与规则排版入口。
- H-B-04/H-B-05 接管正式 Profile、构建体积和切片参数选择。
- H-B-02/H-B-03 与 H-B-04..08 会修改相同宿主窗口和 session 状态；单工作树中应按原子卡
  串行实现。只读审计、夹具准备和文档核对可以并行。
