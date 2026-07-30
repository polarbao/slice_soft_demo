# DOC_MATRIX_13 模型场景专项依赖与准入矩阵

> 版本：v1.2
> 日期：2026-07-28
> 状态：原 P0 17/17 COMPLETE / 13B-08 COMPLETE / 13D-01..04 COMPLETE

## 1. 阶段依赖

| 任务 | 输入依赖 | 完成输出 | 下一 Gate |
|---|---|---|---|
| 13A-01 | 当前 SceneModel、几何准入 | ModelTransform/ModelInstance 合同 | 13A-02、13B-01 |
| 13A-02 | 13A-01 | 单模型俯视图、XY 坐标和选择 | 13A-03 |
| 13A-03 | 13A-02、scene-aware 12E-09A-02 | 移动、rotateZ、uniformScale 与 session effective transform | 13A-04 |
| 13A-04 | 13A-03 | mirrorX/mirrorY 与 post-transform preflight | 13A-05 |
| 13A-05 | 13A-04 | UI smoke、用户说明、13A 报告 | 13B UI |
| 13B-01 | 13A-01 | MultiModelScene 与 Scene Effective Config schema | 12E-09A-02、13B-02 |
| 13B-02 | 13B-01、13A-02 | 模型列表、导入/复制/删除/锁定 | 13B-03 |
| 13B-03 | 13B-02 | 11x2 规则排版、默认 10/10 mm UI 参数 | 13B-04 |
| 13B-04 | 13B-03、ProductionAdmissionPolicy | 幅面、碰撞和逐实例准入 | 13B-05 |
| 13B-05 | 13B-04 | 全局 raster 与逐实例层合成 | 13B-06 |
| 13B-06 | 13B-05、共享 RGBWSV writer | 单 package、scene report、RIP strict | 13B-07 |
| 13B-07 | 13B-06 | 真实模型矩阵与阶段报告 | 13B-08 |
| 13B-08-01 | 13B-07、SceneDocument/ModelTopViewLoader | 批量导入队列和主动作占位 | 13B-08-02 |
| 13B-08-02 | 13B-08-01、13B-05..07 | 场景生产服务和显式 scene CLI | 13B-08-03 |
| 13B-08-03 | 13B-08-02 | Qt 当前场景切片、预检和 Package 回载 | 13B-08-04 |
| 13B-08-04 | 13B-08-03 | 真实模型作业流矩阵与阶段报告 | 13C-04 |
| 13C-01 | 当前 TIFF reader | 异步 TIFF layer source + LRU cache | 13C-02 |
| 13C-02 | 13C-01 | RGB/W/S/V 单通道与组合合成器 | 13C-03 |
| 13C-03 | 13C-02 | 统一生产预览，含 RGB+S+W+V | 12E-09A-05、13C-04 |
| 13C-04 | 13C-03 | 默认关闭重复生产 preview PNG | 13C-05 |
| 13C-05 | 13C-04 | UI/IO/协议回归与阶段报告 | 12E-09A-05 |
| 13D-01 | 13B-08、13C-05 | 顶部作业栏 | 13D-02 |
| 13D-02 | 13D-01 | 单一 Context Inspector | 13D-03 |
| 13D-03 | 13D-02 | 项目区与 DiagnosticsDock 收口 | 13D-04 |
| 13D-04 | 13D-03 | 响应式/UI Smoke/用户说明和报告 | 12E-09A-03 |

首批执行准备状态：

| 任务 | 准备状态 | 说明 |
|---|---|---|
| 13A-01 | COMPLETE（2026-07-27） | Public DTO、数学、pivot、Z、revision、错误码、adapter 和单测已落地 |
| 13A-02 | COMPLETE（2026-07-27） | +Z 俯视 core DTO、异步导入、毫米网格、选择、blocked 显示和 UI Smoke 已落地 |
| 13A-03 | COMPLETE | 精确 X/Y、绕 Z、统一缩放、revision、异步重投影和 session 回读已实现 |
| 13A-04 | COMPLETE | mirror 与 transformed preflight、Legacy/Global 独立准入已实现 |
| 13A-05 | COMPLETE（2026-07-27） | 统一回归、用户说明、REPORT_13A 和 M13-1 候选 PASS |
| 13B-01 | COMPLETE（2026-07-27） | MultiModelScene、ResourceScope、scene identity、Scene Effective Config、正负 fixture、单测和回归已落地 |
| 13B-02 | COMPLETE | 1..22 实例、资源隔离、列表命令、场景草稿、UI Smoke 和 Quick CI 已完成 |
| 13B-03 | COMPLETE | 11x2、row-major、默认 10/10 mm 净距、锁定、原子恢复、配置回读和 Qt UI 已完成 |
| 13B-04 | FUNCTIONAL FIXTURE COMPLETE / PRODUCTION INPUT OPEN | SceneCollisionService、稳定错误、两阶段投影碰撞和逐实例准入已通过回归 |
| 13B-05 | FIXTURE COMPLETE | 公共 Raster/Layer、Legacy/Global adapter、共享 Grid、联合合成和回归证据已完成 |
| 13B-06 | FIXTURE COMPLETE / PRODUCTION INPUT OPEN | 单 package、typed scene extension、scene report、原子发布和 RIP strict 已通过 fixture 回归 |
| 13B-07 | FUNCTIONAL MATRIX COMPLETE / PRODUCTION INPUT OPEN | Debug/Release 功能矩阵 PASS；production GO 等待设备输入和 22 实例预算 |
| 13B-08-01..04 | COMPLETE（2026-07-28） | 批量导入、场景生产 CLI、Qt 当前场景动作、真实 OBJ/3MF 矩阵和 RIP strict 已闭环 |
| 13C-01 | COMPLETE（2026-07-28） | TIFF source、5 层/256 MiB LRU、Qt 异步 generation、稳定错误和定向测试已落地 |
| 13C-02 | COMPLETE（2026-07-28） | 同层 RGBWSV 的单通道、组合、全材料、统计、六通道探针和稳定错误已落地 |
| 13C-03 | COMPLETE（2026-07-28） | TIFF 原生统一生产预览、同层材料组合和 UI 接线已完成 |
| 13C-04 | COMPLETE（2026-07-28） | TIFF 原生默认、显式诊断、兼容迁移、UI 和 IO 证据已闭环 |
| 13C-05 | COMPLETE（2026-07-28） | stripped/tiled、DPI、全材料、错误矩阵、RIP 和报告已收口 |
| 13D-01 | COMPLETE（2026-07-29） | 顶部导入、保存、模式/Profile、切片、取消和状态摘要已实现并通过 Smoke |
| 13D-02 | COMPLETE（2026-07-29） | 单一 Context Inspector、identity 稳定和配置跳转 Smoke 通过 |
| 13D-03 | COMPLETE（2026-07-29） | 项目工具 Dock、统一 DiagnosticsDock 和五页 Context Inspector 已通过 Smoke |
| 13D-04 | COMPLETE（2026-07-29） | 版本化布局恢复、响应式、快捷键、用户说明和阶段证据已收口 |

17 个近程任务的建议文件所有权、计划测试 target、任务输出和停止条件统一登记在
`DOC_PREP_13_全阶段原子任务实施准备与文件所有权.md`。该登记不代表建议代码文件已经存在。

## 2. 优先级

```text
P0：13A-01、13B-01，先冻结 scene/instance/config identity；
P0：12E-09A-02，改为兼容 single_model/scene；
P0：13A-R1 单模型俯视与变换；
P0：13B-R1/R2 规则排版和联合切片；
P0：13B-08 Qt 场景作业流，优先关闭批量导入和主切片入口断点；
P0：13C-R1 TIFF 原生生产预览；
P0/P1：13D 工作台布局，在 13C-05 后收口；
P1：12E-09A-03..06 与 12E-10；
P1：13A 中期 3D viewport；
P2：自动 nesting、跨模型联合支撑和完整 3D gizmo。
```

12G-TCWS 纹理载体/白色分色/RIP 铺底专项为 `FROZEN`，不属于 Stage 13 依赖，不得夹带进入
13A/13B/13C。

## 3. 生产准入

联合切片必须同时满足：

```text
scene schema 校验通过；
实例数量 1..22；
所有资源路径在各自 resourceScope 内；
所有实例 post-transform preflight 通过；
没有实例重叠；
所有实例位于已知 buildVolume 内；
全场景 layerHeight、dpiX、dpiY 一致；
每层 TIFF 通过 p0.rgbwsv.2 和 RIP strict；
manifest/report 能追踪 modelId/instanceId；
失败时不产生可被误认成成功的最终 package。
```

## 4. 预览准入

TIFF 原生预览必须满足：

```text
只读取 manifest 列出的真实 layerIndex；
同层合成，不跨层兜底；
W/S/V 判断使用通道值 < 255；
真实空白必须为六通道 255；
伪彩可配置但不回写 TIFF；
RGB+S+W+V 可显示并保留六通道像素探针；
无 preview PNG 时生产层检查仍可完整使用；
诊断 mask 缺失时显示“未提供”，不得伪造。
```

## 5. 外部输入 Gate

| 输入 | 不阻断 | 阻断 |
|---|---|---|
| 设备 buildVolume/轴方向 | 13A、13B-01..06 fixture、13C、scene draft | 13B-04 production、13B-07 GO |
| 多实例材料 Profile 决策 | P0 `scene_profile_only` | mixed-profile 扩展 |
| 22 实例性能预算 | 功能实现和实测 | 13B-07 production GO |
| 3D 后端选择 | 13A-R1 Qt 俯视 | 13A-R2/R3 |

完整登记见 `DOC_CHECKLIST_13_未决产品输入与阶段Gate.md`。
