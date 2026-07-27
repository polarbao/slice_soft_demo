# DOC_MATRIX_13 模型场景专项依赖与准入矩阵

> 版本：v0.4
> 日期：2026-07-27
> 状态：P0 ATOMIC PREPARATION COMPLETE / 13A-01..04、13B-01、跨阶段 09A-02 COMPLETE / NEXT 13A-05

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
| 13B-03 | 13B-02 | 11x2 规则排版、20/30 mm UI 参数 | 13B-04 |
| 13B-04 | 13B-03、ProductionAdmissionPolicy | 幅面、碰撞和逐实例准入 | 13B-05 |
| 13B-05 | 13B-04 | 全局 raster 与逐实例层合成 | 13B-06 |
| 13B-06 | 13B-05、共享 RGBWSV writer | 单 package、scene report、RIP strict | 13B-07 |
| 13B-07 | 13B-06 | 真实模型矩阵与阶段报告 | Stage 13B GO/NO-GO |
| 13C-01 | 当前 TIFF reader | 异步 TIFF layer source + LRU cache | 13C-02 |
| 13C-02 | 13C-01 | RGB/W/S/V 单通道与组合合成器 | 13C-03 |
| 13C-03 | 13C-02 | 统一生产预览，含 RGB+S+W+V | 12E-09A-05、13C-04 |
| 13C-04 | 13C-03 | 默认关闭重复生产 preview PNG | 13C-05 |
| 13C-05 | 13C-04 | UI/IO/协议回归与阶段报告 | 12E-09A-05 |

首批执行准备状态：

| 任务 | 准备状态 | 说明 |
|---|---|---|
| 13A-01 | COMPLETE（2026-07-27） | Public DTO、数学、pivot、Z、revision、错误码、adapter 和单测已落地 |
| 13A-02 | COMPLETE（2026-07-27） | +Z 俯视 core DTO、异步导入、毫米网格、选择、blocked 显示和 UI Smoke 已落地 |
| 13A-03 | COMPLETE | 精确 X/Y、绕 Z、统一缩放、revision、异步重投影和 session 回读已实现 |
| 13A-04 | COMPLETE | mirror 与 transformed preflight、Legacy/Global 独立准入已实现 |
| 13A-05 | READY FOR DEVELOPMENT | 统一回归、用户说明、REPORT_13A 和 M13-1 收口合同已冻结 |
| 13B-01 | COMPLETE（2026-07-27） | MultiModelScene、ResourceScope、scene identity、Scene Effective Config、正负 fixture、单测和回归已落地 |
| 13B-02 | READY BY DEPENDENCY / SCHEDULE AFTER 13A-05 | 1..22 实例、资源隔离、列表命令和场景草稿边界已准备 |
| 13C-01 | READY / SCHEDULE AFTER IDENTITY WAVE | TIFF source、cache、异步 identity、错误码和测试已冻结 |

17 个近程任务的建议文件所有权、计划测试 target、任务输出和停止条件统一登记在
`DOC_PREP_13_全阶段原子任务实施准备与文件所有权.md`。该登记不代表建议代码文件已经存在。

## 2. 优先级

```text
P0：13A-01、13B-01，先冻结 scene/instance/config identity；
P0：12E-09A-02，改为兼容 single_model/scene；
P0：13A-R1 单模型俯视与变换；
P0：13B-R1/R2 规则排版和联合切片；
P0：13C-R1 TIFF 原生生产预览；
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
| 设备 buildVolume/轴方向 | 13A、13B-01..03、13C、scene draft | 13B-04 production、13B-07 GO |
| 多实例材料 Profile 决策 | P0 `scene_profile_only` | mixed-profile 扩展 |
| 22 实例性能预算 | 功能实现和实测 | 13B-07 production GO |
| 3D 后端选择 | 13A-R1 Qt 俯视 | 13A-R2/R3 |

完整登记见 `DOC_CHECKLIST_13_未决产品输入与阶段Gate.md`。
