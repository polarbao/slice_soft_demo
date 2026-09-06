# REPORT HOSTFLOW H-F-15 自动定向开关与批次原点偏移

> 状态：**COMPLETE**
> 日期：2026-09-04
> 上游：H-F-14 坐标审计、DTO v1.13

## 1. 当前实现

“变换与排版”现提供：

- 默认勾选的“导入时自动定向”；
- 默认勾选的“导入后自动排版”；
- 关闭自动排版后可编辑的“批次原点 X/Y”。

关闭两个开关后，模型使用 OBJ 源姿态和源 XY；批次原点作为相同增量应用到本次所有新增实例，
所以整组平移而不改变模型间相对位置。所有新增实例仍使用 `landOnBuildPlate=true`，Z 独立触底。

## 2. 合同与数据一致性

DTO v1.14 新增可选 `options.autoOrient`，缺省 true。`ModelFacade` 生成元数据和
`ModelCapabilityAdapter` 保留 SceneModel 时使用同一值，并把选择持久化到 `ModelSource`。
默认 true 不写入场景，旧场景缺字段也按 true 读取；显式 false 随场景传给 Worker，完整预检
与生产模型重载均覆盖 Profile 的定向默认值，避免工作区和切片出现两个姿态。批次 XY 复用既有
`addInstance.initialTransform.translateX/YMm`，不新增场景操作。

## 3. 验证结果

- Debug 构建：`slicer_module`、`hostflow_hb01_model_import_tests`、
  `hostflow_hb03_transform_layout_tests`、`multimodel_scene_contract_unit_tests`、
  `multi_model_production_service_unit_tests`、`slicer_ui_host_sim` 通过。
- 合成高模型默认定向后高度小于 9 mm，关闭后源高度为 10 mm。
- 批次偏移 `(-9,-19)` 后权威 bbox 最小点为 `(1,1,0)`，Z 触底保持。
- 默认姿态场景不新增 JSON 字段，旧 scene hash 保持；显式 false 可序列化往返。
- Profile 保持自动定向开启的合成高模型，在 Worker 生产重载中仍以场景源姿态通过准入。
- DTO 合同、旧请求、场景合同、生产服务、Grid、导入、面板和宿主 smoke 共 8 项 CTest 通过。
- Release `slicesoft_runtime` 聚合目标通过并部署到 `runtime/slicesoft/Release`；因运行目录文件
  被外部进程短暂占用，脚本使用内置的就地不可变载荷更新并保留 `output`。构建/运行宿主均为
  `0.2.323-dev`，运行目录离屏 `--self-test` 返回 `STAGE14E02_SELF_TEST_PASS`。

未执行真机打印验证；该任务不改变打印数据或材料语义。
