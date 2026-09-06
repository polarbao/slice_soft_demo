# DOC DECISION HOSTFLOW H-F-15 导入姿态与批次原点受控修订

> 状态：**ACCEPTED / USER AUTHORIZED / IMPLEMENTED**
> 日期：2026-09-04
> 合同：`slicer_capability_dtos` v1.14，SPI v1

## 1. 问题

参考宿主已有规则排版开关，但 `model.import` 始终采用默认自动定向。关闭规则排版只能保留
定向后的 XY，无法复现 OBJ 的源姿态和整组源坐标。源模型组合轻微越界时，也缺少只作用于
本次导入批次的统一 XY 修正。

## 2. 决策

1. `model.import.options.autoOrient` 新增为可选布尔字段，缺省 `true`；显式 `false` 保留源姿态。
2. Facade 元数据与模块持有的 SceneModel 必须使用同一选项，禁止宿主侧逆算旋转。
3. 宿主新增默认勾选的“导入时自动定向”，与“导入后自动排版”相互独立。
4. 关闭自动排版后开放批次原点 X/Y；数值作为本次全部新增实例共同的初始 XY 平移，保持相对位置。
5. 批次原点不包含 Z。每个新增实例继续提交 `landOnBuildPlate=true`，源姿态模式也必须触底。
6. 自动排版开启时批次原点控件禁用并按零传递，避免规则排版覆盖源位置产生歧义。
7. 导入选择随 `ModelMetadata` 进入 `ModelSource`。只有 `false` 显式写入场景；Worker 完整预检
   与生产重载按场景值覆盖 Profile 的 `autoOrient.enabled`，保证显示与切片姿态一致。

## 3. 兼容边界

- 合同由 v1.13 增量提升为 v1.14；省略新字段的旧请求行为不变。
- SPI v1、11 个导出和 15 项能力不变。
- 不修改 Worker 请求/响应、Profile、场景操作枚举、RGBWSV、TIFF 或生产包协议；只修正 Worker
  内部模型重载，使其遵循场景已经冻结的导入姿态。
- 默认 `autoOrient=true` 不写入场景，旧场景按 true 读取，canonical JSON 与 scene hash 不漂移。
- 自动定向选择会改变模型几何身份，这是显式用户意图；同一模型句柄内的元数据、预检、视图和切片保持一致。

## 4. 验证

- 合成高模型：默认导入执行自动定向，显式关闭后保留 10 mm 源高度。
- 源模型原点 `(10,20)` 加批次偏移 `(-9,-19)` 后，权威 XY 最小点为 `(1,1)`。
- 同一实例权威有效包围盒 `minZ=0`，确认 XY 偏移没有关闭触底。
- 场景合同验证默认值省略及 `false` 往返；合成高模型验证 Worker 在 Profile 开启定向时仍按
  场景的源姿态完成模型加载和场景准入。
- DTO 合同、旧请求兼容、场景、生产服务、宿主导入、面板、Grid 和宿主 smoke 共 8 项 Debug CTest 通过。
