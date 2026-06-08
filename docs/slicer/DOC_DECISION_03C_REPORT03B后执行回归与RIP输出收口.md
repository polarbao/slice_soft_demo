# DOC_DECISION_03C_REPORT03B后执行回归与RIP输出收口

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_03B 之后  
> 建议提交目录：`docs/slicer/`

## 1. 阶段判断

根据 `REPORT_03B_TIFF存储模式兼容当前实现状态.md`，03B 已完成核心目标：

```text
默认输出从 tiled TIFF 改为 stripped TIFF
manifest schema 从 p0.rgbwsv.1 升级到 p0.rgbwsv.2
Writer 支持 stripped / tiled
Reader 支持 p0.rgbwsv.1 legacy tiled、p0.rgbwsv.2 stripped、p0.rgbwsv.2 tiled
新增 storage 相关错误码和 bad package
MaterialPolicy 六个样例保持回归通过
```

因此，03B 主功能可以收口。

## 2. 为什么不建议立刻进入 06

03B 报告同时暴露工程层面的收口问题：

```text
run_regression.ps1 完整模式包含重型 relief 样例，总耗时可能超过 10 分钟
rip_reader_test 在大层数模型上输出较长，不利于快速定位问题
heavy relief 验证与快速主回归混在一起
后续真实 RIP 对接需要确认目标 RIP 对 stripped/tiled、RowsPerStrip、TIFF tag 的兼容矩阵
```

这些问题不是切片功能错误，但会影响后续阶段效率。

因此建议新增轻量收口阶段：

```text
03C：回归脚本拆分与 RIP Reader 输出收口
```

## 3. 03C 阶段定位

03C 是工程效率和测试可维护性阶段，不是新切片功能阶段。

03C 不改变：

```text
RGBWSV
p0.rgbwsv.2
storageMode
MaterialPolicy
Texture
Support
Geometry
```

03C 只优化：

```text
rip_reader_test 输出模式
run_regression.ps1 脚本结构
heavy regression 拆分
RIP target compatibility checklist
报告可读性
```

## 4. 03C 完成后再进入什么

03C 完成后，再判断进入以下路线之一：

```text
路线 A：06 3MF 与多材料输入基础版
路线 B：05A 真实模型材料参数验证
路线 C：08 支撑形态优化专项
```

当前默认推荐：

```text
03C 完成后进入 06：3MF 与多材料输入基础版
```

## 5. 非目标

03C 不做：

```text
3MF
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
材料策略修改
纹理采样修改
支撑形态修复
TIFF storage 再次改造
```
