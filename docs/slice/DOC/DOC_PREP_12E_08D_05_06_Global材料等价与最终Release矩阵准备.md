# DOC_PREP_12E-08D-05/06 Global 材料等价与最终 Release 矩阵准备

> 文档状态：08D-05 COMPLETE / 08D-06 READY
> 日期：2026-07-23
> 前置：12E-08D-01..04 COMPLETE；受限 RGB + W Profile GO

## 1. 任务定义

现有 08D-04 只准入 RGB 纹理与 W 模型填充，明确阻断 S 支撑、V 光油和最终
0.01 mm 真实模型矩阵。用户已授权继续 08D-05/06，因此将两个剩余 Gate 固化为：

```text
12E-08D-05：Global S/V 材料等价接入；
12E-08D-06：0.01 mm Release 矩阵、RIP 与最终分层 GO/NO-GO。
```

## 2. 08D-05 范围

新增显式 Profile：

```text
materialProcessProfile.target =
  global_surface_shell_material_parity_candidate
```

首个材料等价候选只允许可证明的保守范围：

```text
Texture Surface：RGB；
Model Fill：W；
Support：S，placement=lower；
Internal Void Support：随 lower support 一并覆盖；
Surface Varnish：V，可选，限定模型域内；
Outer Varnish：V，可选，按 thicknessMm 扩张 XY；
冲突优先级：Model > OuterVarnishShell > Support > Empty；
surface varnish 可与模型 RGB/W 同像素叠加；
closure repair：关闭；
OpenVDB：保持 optional/OFF；
不允许 silent fallback。
```

暂不准入：

```text
upper/both/full_vertical_projection support；
support offset、shape、bridge、dilation；
legacy materialPolicy/materialRoleMapping；
复杂自相交模型绕过 strict preflight。
```

验收：

```text
合成体素测试证明 lower support、internal void、surface/outer varnish 和优先级；
正向 package 的 S/V printPixels > 0；
full closure PASS；
RIP strict PASS；
不支持的 support placement 稳定失败且无 package；
restricted Profile 的既有输出行为不变。
```

## 3. 08D-06 范围

08D-05 通过后，使用 xiao_ma 与 yecan 两个 strict-PASS 真实模型族运行：

```text
Release；
600 dpi；
layerThicknessMm=0.01；
legacy 与 Global 分开记录；
Global restricted 与 material-parity Profile 分开记录；
TIFF layer list 完整；
RIP strict PASS；
记录 config/model/slice/write/total、峰值内存和通道统计；
失败必须保留明确 blocker，不得回退 legacy。
```

08D-06 只依据实测结果给出分层 GO/NO-GO，不把候选预算宣传为产品 SLA。

## 4. 固定协议

两项任务都不得改变：

```text
schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
uint8；
black_is_print；
0=打印；
255=不打印；
Legacy 默认；
共享 TIFF/package writer；
production success 必须有 TIFF。
```

## 5. 原子提交

```text
08D-05：代码、单测、材料等价配置/脚本、执行证据和状态更新独立提交；
08D-06：0.01 mm Release 脚本、机器可读摘要、决策和状态更新独立提交。
```
