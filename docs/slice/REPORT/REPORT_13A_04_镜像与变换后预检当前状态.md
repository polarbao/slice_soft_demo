# REPORT 13A-04 镜像与变换后预检当前状态

> 文档状态：COMPLETE
> 日期：2026-07-27
> 代码提交：`5caa7fb`
> 下一任务：13A-05 阶段收口

## 1. 任务结论

13A-04 已在 13A-03 精确变换闭环上补齐 `mirrorX/mirrorY` 和变换后模型预检。预检直接消费不可变
`SceneModel` 与当前有效 `ModelInstance`，不从屏幕投影或旧单模型配置反推几何；源模型和变换后模型
分别保留诊断结果，Legacy 与 Global 也分别计算准入结果。

镜像、快速连续变换、取消、stale revision、缺失资源和拓扑阻断均采用 fail-closed。阻断模型仍可在
俯视工作区查看和调整，但加载了可编辑场景时生产切片继续禁用，因为 `slicer_cli` 尚未消费 scene
effective config；该生产接线属于 13B，不由本任务绕过。

## 2. 核心实现

### 2.1 无 Qt transformed preflight

新增 `TransformedModelPreflightService`，输入包括：

```text
shared source SceneModel；
effective ModelInstance；
ModelPreflightOptions 和 admission context；
source/resource hash；
sceneId、sceneRevision、transformRevision、generation；
cancellation callback。
```

服务复用 `AdaptTransformedModel`、现有几何诊断和 `EvaluateModelPreflightAdmissions`。输出同时携带
source/transformed 结果、Legacy/Global admission、transform hash、revision、取消和 stale 状态。
已完成诊断按稳定输入身份缓存，缓存访问由 mutex 保护。

### 2.2 镜像语义

`ModelTransformPanel` 增加 X/Y 镜像按钮。镜像仍属于实例变换，不修改 OBJ/STL/3MF 源文件。
奇数轴镜像的 winding 与 UV 顶点顺序沿用 `TransformedModelAdapter` 既有合同，双轴镜像保持正行列式。
镜像值随 scene draft/effective config 保存和回读。

### 2.3 异步状态与最新代发布

Qt 新增 `TransformedModelPreflightLoader`。每次有效变换后：

```text
取消旧预检；
按当前 scene/transform revision 启动新请求；
只接受最新 generation 和 revision 的结果；
更新 source/transformed 预检与 Legacy/Global admission；
PENDING、FAILED、BLOCKED、stale 均不得启用生产切片。
```

Worker 捕获共享的源模型和服务生命周期，不依赖窗口或仓库裸指针。

### 2.4 UI 状态

变换面板显示：

```text
源模型预检；
变换后预检；
Legacy 准入；
Global 准入；
scene/transform revision；
dirty、stale 和错误状态。
```

开放网格等模型可以显示 Legacy warning 与 Global blocked，不再把一个模式的结果推导为另一个模式的
结果。生产按钮工具提示会区分预检等待、阻断和“预检通过但 scene 生产接线尚未完成”。

## 3. 验证结果

新增：

```text
transformed_model_preflight_unit_tests；
--ui-smoke-test --case model-transform-preflight。
```

覆盖：

```text
mirrorX/mirrorY 和双镜像配置回读；
严格闭合模型 source/transformed PASS；
开放拓扑 Legacy warning / Global blocked；
缺失纹理资源阻断两种模式；
取消和 stale revision fail-closed；
源模型不变性；
快速连续镜像只发布最新 revision；
阻断模型保持可见；
三种窗口尺寸。
```

实际验证命令：

```powershell
cmake --build build --config Debug --target transformed_model_preflight_unit_tests scene_transform_controller_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(model_transform_unit_tests|scene_view_geometry_unit_tests|scene_transform_controller_unit_tests|model_preflight_admission_unit_tests|model_preflight_service_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-transform-preflight
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view-transform
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view
.\scripts\run_ci_quick.ps1
git diff --check
```

上述构建、定向 CTest 6/6、Qt self-test、三个 UI Smoke、Quick CI 和 diff check 均通过。

## 4. 符合情况

| 13A-04 要求 | 结果 |
|---|---|
| mirrorX/mirrorY 实例命令 | 已实现 |
| 源模型不可变 | 已实现 |
| winding/UV 镜像修正 | 复用既有 adapter 并回归 |
| source/transformed 双诊断 | 已实现 |
| Legacy/Global 独立准入 | 已实现 |
| generation/revision/cancel fail-closed | 已实现 |
| blocked 可见、生产禁用 | 已实现 |
| 镜像 scene config 保存/回读 | 已实现 |
| transformed preflight 单测与 UI Smoke | 已实现 |
| 自动修复复杂自相交 | 未实现，明确非目标 |
| scene effective config 生产切片 | 未实现，属于 13B |

## 5. 固定边界

```text
Legacy 仍是默认生产引擎；
OpenVDB 仍为显式候选且默认关闭；
不自动修复 confirmed self-intersection；
不允许 silent fallback；
不修改 p0.rgbwsv.2；
不修改 R G B W S V 通道顺序；
不修改 uint8 和 black_is_print；
不新增多模型排版、联合切片、3D 视口或 TIFF 预览。
```

## 6. 下一步

13A-05 只做阶段收口：

```text
统一回归 13A-01..04；
执行 Qt self-test 和三窗口 UI Smoke；
用 strict-PASS、纹理正向和复杂模型 blocked 反向资产复核；
补齐用户说明、阶段总报告、索引和上下文；
确认 M13-1 单模型俯视与变换候选；
不新增新的模型编辑或生产能力。
```
