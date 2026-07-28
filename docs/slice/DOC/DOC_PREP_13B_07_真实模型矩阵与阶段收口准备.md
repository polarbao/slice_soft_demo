# DOC_PREP 13B-07 真实模型矩阵与阶段收口准备

> 文档状态：READY FOR FUNCTIONAL MATRIX DEVELOPMENT / PRODUCTION INPUT OPEN
> 日期：2026-07-28
> 前置：13B-06 FIXTURE COMPLETE
> 下一报告：`REPORT_13B_07_真实模型矩阵与阶段收口当前状态.md`

## 1. 目标

在 13B-01..06 已建立的场景身份、规则排版、逐实例准入、联合内存层和单 package writer 上，
使用仓库内真实 OBJ、控制组 3MF 和确定性负向 fixture 完成 Stage 13B 功能矩阵。

本任务必须区分两个结论：

```text
functional matrix：
  使用显式 fixture buildVolume 和功能 Profile；
  验证 1/11/12/22 实例的完整场景链；
  可以形成工程功能 PASS/FAIL。

production GO：
  必须使用正式设备 buildVolume、原点、X/Y 轴向和已冻结性能预算；
  外部输入未关闭时保持 OPEN，不得由 fixture 结果替代。
```

## 2. 已核对的代码事实

当前可直接复用：

```text
MultiModelScene / SceneEffectiveConfig：稳定场景、模型和实例身份；
GridLayoutPolicy：1..22、11x2、row_major、edge_clearance；
SceneViewGeometry / SceneCollisionService：变换后投影、幅面、碰撞和逐实例准入；
LegacySceneLayerAdapter：不落盘的 Legacy RGBWSV 本地层；
GlobalSceneLayerAdapter：显式 Global writer-ready 层适配；
ComposeAdmittedSceneRasters：共享 Grid 和联合层合成；
WriteMultiModelSceneProductionPackage：单 package、scene report、原子发布和 RIP strict。
```

当前缺口：

```text
没有 Stage 13B 的真实模型矩阵可执行入口；
没有统一的机器可读 matrix report；
没有对 1/11/12/22 的 import/layout/admission/slice/compose/write 分段计时；
没有把 OBJ、3MF、资源隔离、碰撞、越界和 23 实例阻断汇总到同一收口报告；
正式设备幅面、轴向和 22 实例性能预算仍未提供。
```

## 3. 固定输入资产

正向真实资产：

```text
OBJ 主模型：
  model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj

OBJ 独立模型族：
  model/obj/yecan/3.obj

Texture2D 3MF 控制组：
  samples/models/3mf/texture2d_checker_cube.3mf
```

选择依据：

```text
xiao_ma 和 yecan 已在 REPORT_12E_08C_R4_模型资产预检清单中 strict PASS；
二者属于独立模型族，可验证 modelId/resourceScope 隔离；
model 目录当前没有 strict PASS 3MF，因此 3MF 使用仓库跟踪的 Texture2D 控制组；
aishen/meigui/titian 复杂浮雕保持 0/3 披露缺口，不得伪装成正向准入资产。
```

负向 fixture：

```text
23 实例；
显式重叠；
显式越界；
缺失 buildVolume；
资源 scope 逃逸或缺失；
一个实例 admission blocked；
stale scene/transform revision；
混合 Legacy/Global；
不同 DPI/layerHeight。
```

## 4. 功能 Profile

13B-07 使用独立、低成本、可重复的功能 Profile，不覆盖用户生产 Profile：

```text
pipeline=legacy；
dpiX=127；
dpiY=127；
pixelSizeX/Y=0.20 mm；
layerHeight=0.20 mm；
preview.enabled=false；
storageMode=stripped；
rowsPerStrip=64；
support/model-fill/material 继续输出 RGBWSV 生产语义；
fixture buildVolume 从排版结果加显式边界得到，并标记 isFixture=true。
```

采用 127 DPI 是为了让 20/30 mm 净距精确落在整数像素，并控制 22 实例功能矩阵的内存和 TIFF
规模。该 Profile 只证明功能合同，不代表设备生产分辨率或正式性能。

## 5. 正向矩阵

| Case | 实例 | 资产组成 | 必须验证 |
|---|---:|---|---|
| 13B-M01 | 1 | xiao_ma | 单实例兼容、一个 package、RIP strict |
| 13B-M11 | 11 | xiao_ma 重复实例 | 第一行填满、资源复用、确定性 |
| 13B-M12 | 12 | xiao_ma + yecan | 第 12 个进入第二行、独立资源 |
| 13B-M22 | 22 | xiao_ma/yecan 交替 | 两行填满、单 package、峰值内存和耗时 |
| 13B-M3F | 2 | OBJ + Texture2D 3MF | 格式混合、资源 scope、纹理/材料不串 |

所有正向 case 必须满足：

```text
scene_profile_only；
同一 DPI、layerHeight、pipeline mode；
所有 visible instance 已准入且变换证据新鲜；
实例之间无正面积重叠且在 fixture buildVolume 内；
每个全局 layerIndex 只有一个 TIFF；
scene report 的 per-instance/global 统计可对账；
manifest 保持 p0.rgbwsv.2；
RIP strict PASS；
相同输入的稳定业务投影一致。
```

## 6. 资源复用和内存边界

同一 modelId 的纯 XY 平移实例允许复用一次本地切片结果，但必须满足：

```text
源模型、资源、Profile、DPI、layerHeight 和非平移变换完全一致；
每个实例重新绑定 instanceId、transformRevision 和 transformHash；
local grid origin 按有效 XY 平移更新；
平移必须能量化到全局像素；
复用不得修改原型 raster；
旋转、缩放、镜像或材料/Profile 变化必须重新生产本地层。
```

报告至少记录：

```text
uniqueModelCount；
sliceProducerInvocationCount；
reusedInstanceCount；
resourceReuseRatio；
compose peakWorkingBytes；
进程 peakWorkingSetBytes；
packageBytes。
```

## 7. 计时合同

使用单调时钟记录：

```text
importMs；
layoutMs；
preflightAdmissionMs；
sliceMs；
composeMs；
tiffAndReportWriteMs；
ripValidationMs；
totalMs。
```

本阶段不虚构共享 writer 内部尚未暴露的 TIFF/report 子阶段。`tiffAndReportWriteMs` 是写包事务整体，
包含 TIFF、报告、manifest、内部 strict 校验和原子发布。若后续需要拆分 TIFF 与 JSON 时间，应在
独立性能任务中为共享 writer 增加稳定 telemetry，不在本任务以文件时间戳推算。

## 8. 机器可读报告

矩阵入口输出：

```text
output/benchmarks/13b_07/real_model_matrix.json；
output/benchmarks/13b_07/real_model_matrix.md；
各正向 case 的 package；
REPORT_13B_07_真实模型矩阵与阶段收口当前状态.md。
```

JSON schema：

```text
slicesoft.multimodel_scene_matrix.13b.1
```

顶层至少包含：

```text
status；
functionalMatrixPass；
productionGo=false；
productionBlockers；
build/config/compiler；
fixedProtocol；
assets 及 hash；
cases；
timingSummary；
memorySummary；
knownCoverageGaps。
```

## 9. 两引擎范围

```text
Legacy：13B-07 必测主矩阵，继续为默认生产路径；
Global/OpenVDB：仅在独立依赖环境可用且资产通过 strict admission 时运行显式候选子矩阵；
默认 OpenVDB OFF 构建中，Global 子矩阵记录 NOT_RUN_DEPENDENCY_UNAVAILABLE，不使 Legacy
功能矩阵失败，也不得声明 Global 多模型 production PASS；
禁止同一 scene 混合 Legacy 和 Global；
禁止 silent fallback。
```

## 10. 原子任务

```text
13B-07A：冻结 PREP/PROMPT、资产、Profile、报告 schema 和 Gate；
13B-07B：实现矩阵 runner、低成本真实资产 config 和结果 JSON/Markdown；
13B-07C：完成 1/11/12/22、OBJ/3MF、资源复用和单 package/RIP 正向矩阵；
13B-07D：完成 overlap/out-of-bounds/23/stale/mixed-mode 等负向矩阵；
13B-07E：运行 Debug/Release 定向验证和 Quick CI，生成 REPORT_13B_07；
13B-07F：只在外部输入关闭后评审 production GO，否则正式记录 INPUT OPEN。
```

## 11. 验证命令

```powershell
cmake --build build --config Debug --target multi_model_scene_matrix multi_model_scene_matrix_unit_tests rip_reader_test
ctest --test-dir build -C Debug -R "^(multi_model_scene_matrix_unit_tests|grid_layout_policy_unit_tests|scene_collision_admission_unit_tests|scene_layer_adapters_unit_tests|multi_model_layer_composer_unit_tests|multi_model_package_writer_unit_tests)$" --output-on-failure
.\scripts\run_13b_07_real_model_matrix.ps1 -BuildDir build -Config Debug
cmake --build build --config Release --target multi_model_scene_matrix rip_reader_test
.\scripts\run_13b_07_real_model_matrix.ps1 -BuildDir build -Config Release -SkipBuild
.\scripts\run_ci_quick.ps1
git diff --check
```

## 12. 非目标和停止条件

本任务不做：

```text
Qt 一键联合切片接线；
自动 nesting；
跨模型联合支撑；
混合 Profile；
复杂浮雕重建；
修改 p0.rgbwsv.2；
13C TIFF 原生预览；
把 fixture buildVolume 或低 DPI 指标写成生产设备指标。
```

发生以下任一情况立即停止并输出 NO-GO：

```text
任一正向 case 需要绕过 admission；
实例重叠后仍进入合成；
需要混合引擎或 silent fallback；
scene report 与 package identity/统计不一致；
RIP strict 失败；
22 实例功能 case 无法在受控资源边界完成；
需要虚构正式设备幅面、坐标或性能阈值。
```

## 13. 准备结论

13B-07 的功能输入、资产、低成本 Profile、正负矩阵、资源复用、计时口径、报告 schema、引擎边界和
停止条件已经冻结，可进入功能矩阵开发。正式 production GO 仍等待设备 buildVolume、原点/X/Y 轴向
和 22 实例性能预算，不阻断本阶段工程功能矩阵。

