# CODEX_PROMPT 13B-07 真实模型矩阵与阶段收口执行指令

> 状态：READY FOR FUNCTIONAL MATRIX DEVELOPMENT / PRODUCTION INPUT OPEN
> 日期：2026-07-28
> 前置：13B-06 FIXTURE COMPLETE

## 1. 必读

```text
AGENTS.md；
docs/slice/REPORT/REPORT_13B_06_单Package与SceneReport当前状态.md；
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md；
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md；
docs/slice/DEMO/DEMO_13B_多模型排版联合切片验证方案.md；
docs/slice/DOC/DOC_PREP_13B_07_真实模型矩阵与阶段收口准备.md；
docs/slice/DOC/DOC_CHECKLIST_13_未决产品输入与阶段Gate.md；
docs/slice/REPORT/REPORT_12E_08C_R4_模型资产预检清单.md。
```

## 2. 执行顺序

```text
13B-07A：确认工作树并冻结 PREP/PROMPT；
13B-07B：先写 matrix report/schema/negative unit tests；
13B-07C：实现真实 OBJ/3MF 功能 runner 和纯平移实例本地层复用；
13B-07D：执行 1/11/12/22、资源隔离、碰撞、越界、23 实例和 stale 负向矩阵；
13B-07E：执行 RIP strict、Release 指标、Quick CI；
13B-07F：生成 REPORT_13B_07，并把 production 外部 Gate 保持 OPEN。
```

## 3. 固定资产和 Profile

```text
OBJ：
  model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj；
  model/obj/yecan/3.obj。

3MF：
  samples/models/3mf/texture2d_checker_cube.3mf。

功能 Profile：
  Legacy；
  127x127 DPI；
  0.20 mm layerHeight；
  white model fill / all_model；
  preview OFF；
  fixture buildVolume 显式标记；
  20/30 mm edge clearance；
  1/11/12/22 实例。
```

不得把 aishen/meigui/titian 的 strict FAIL 资产作为正向 case。

## 4. 固定边界

```text
一个 scene 只发布一个 package；
每层一个全局 TIFF；
保持 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
Legacy 保持默认；
Global/OpenVDB 仅显式可选，默认 OFF；
禁止混合引擎和 silent fallback；
重叠、越界、23 实例、stale evidence 必须 fail-closed；
功能 fixture PASS 不等于正式设备 production GO；
不实现 Qt 一键联合切片和 13C 预览。
```

## 5. 验收

```text
1/11/12/22 和 OBJ+3MF 正向 case 完成；
第 12 个实例进入第二行；
相同 modelId 的纯平移实例只生产一次本地层并记录复用；
scene report 的实例/global 统计与生产 TIFF 对账；
每个 case 只有一个 package 且每层只有一个 TIFF；
RIP strict PASS；
负向 case 不留下伪成功 package；
JSON/Markdown 矩阵报告包含计时、内存、资产 hash、生产 blocker；
重复运行稳定业务投影一致；
Quick CI PASS。
```

## 6. 生产结论规则

若以下任一输入仍未关闭：

```text
正式 buildVolume；
设备原点和 X/Y 轴向；
22 实例性能预算；
```

则报告必须写：

```text
functionalMatrixPass=<实测结果>；
productionGo=false；
productionStatus=INPUT_OPEN。
```

不得用 127 DPI fixture 的耗时或幅面结果替代正式生产证据。

## 7. 停止条件

```text
需要改变 TIFF/manifest 固定协议；
需要放宽 RIP strict；
需要绕过逐实例 admission；
需要让重叠实例进入材料合成；
需要把 Global 设为默认或添加 fallback；
需要虚构设备输入或性能阈值；
真实正向资产与预检清单冲突。
```
