# PRD_11A_OpenVDB_OBJ彩色纹理切片前置计划

> 文档版本：v0.1
> 文档状态：PRD / Stage 11A
> 生成日期：2026-07-02
> 阶段定位：Stage 12 前置，OBJ 彩色纹理与 OpenVDB 候选切片计划

## 1. Goal

在 Stage 12 前，先完成 OBJ 彩色纹理真实模型的功能性测试基线和 OpenVDB OBJ 彩色纹理切片正式化计划。

用户应能通过 UI 按钮完成：

```text
1. 非 OpenVDB legacy 一键切片；
2. OpenVDB 实验诊断；
3. 后续 OpenVDB production candidate 一键切片。
```

## 2. Scope

本阶段覆盖：

```text
model/obj 标准模板目录登记；
标准 OBJ/MTL/PNG 模板功能性测试；
legacy OBJ 彩色纹理切片样例；
OpenVDB OBJ 彩色纹理 production candidate 需求拆分；
UI 按钮式交互要求；
Stage 12 前置验收标准。
```

## 3. Non-goals

本阶段不做：

```text
RIP 半色调；
设备通信；
喷头 bitstream；
默认启用 OpenVDB；
跳过 strict admission 写 production package；
改变 p0.rgbwsv.2 输出协议；
自动 mesh repair 的完整实现。
```

## 4. 标准测试模型

标准 OBJ 彩色纹理测试模型目录：

```text
model/obj
```

当前模板：

```text
MF_aishen_damuzhi_L_tx02.obj
MF_aishen_damuzhi_L_tx02.mtl
T_aishen_damuzhi_L_tx02.png
```

该模型用于：

```text
OBJ/MTL/Texture 导入验证；
UV 贴图采样验证；
legacy heightfield 彩色切片功能性测试；
OpenVDB surface-shell texture transfer 验收；
UI 一键导入模型功能性测试；
RIP reader package 验证。
```

标准功能性配置允许对该模板使用统一缩放：

```text
modelTransform.scale = [0.8, 0.8, 0.8]
```

原因是该模型原始最小厚度约 7.16mm，仅靠直角旋转无法满足 6mm 功能性测试高度约束。真实尺寸验收应另建 profile，不覆盖此标准功能性测试配置。

## 5. User Stories

### 5.1 Legacy 一键切片

作为调试人员，我希望点击“导入模型并切片”，选择任意目录中的 OBJ 模型后，UI 自动生成配置、运行切片、加载输出包，以便快速验证模型是否能被当前生产路径处理。

验收：

```text
可选择 model/obj/MF_aishen_damuzhi_L_tx02.obj；
生成 output/ui_sessions/<name>/slice_config.generated.json；
生成 p0.rgbwsv.2 输出包；
rip_reader_test PASS；
层预览可显示 texture_rgb。
```

### 5.2 OpenVDB 实验诊断

作为开发人员，我希望点击“导入模型并 OpenVDB 诊断”，选择同一 OBJ 模板后，UI 输出 OpenVDB experimental report，以便判断 OpenVDB 可用性、topology blocker 和 admission 状态。

验收：

```text
生成 p0.experimental_openvdb_shell_cli_report.1；
明确 productionPackageWritten=false；
明确 productionAllowed=false 或 blocker 状态；
UI 能加载并显示 report。
```

### 5.3 OpenVDB 候选切片

作为后续开发目标，我希望在严格准入通过后，通过按钮执行 OpenVDB OBJ 彩色纹理候选切片，生成与 legacy 一致的 RGBWSV package。

验收：

```text
只有 strict_closed 且无 blocker 时写 package；
OBJ 纹理颜色进入 RGB 通道；
manifest schema = p0.rgbwsv.2；
rip_reader_test PASS；
texture fidelity report 记录 sampled/fallback/uvOutOfRange；
legacy 路径不退化。
```

## 6. Acceptance

Stage 11A 完成标准：

```text
标准 OBJ 模板被文档和场景索引引用；
legacy 标准模板配置可 inspect / slice；
OpenVDB 当前阶段判断清晰；
OpenVDB production candidate 改造任务拆分清晰；
UI 操作手册覆盖两条当前按钮路径；
进入 Stage 12 前的门槛明确。
```
