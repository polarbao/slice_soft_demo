# DOC_DECISION_07_REPORT05A后进入Qt调试UI阶段

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_05A 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：05A 完成材料工艺 profile 验证后，进入 07 Qt 调试 UI 阶段

---

## 1. 阶段判断

根据 `REPORT_05A_真实材料工艺参数验证当前实现状态.md`，当前项目已经完成：

```text
1. materialProcessProfile 配置；
2. material_process_report.json；
3. RGB + W + V profile 验证；
4. W underbase 覆盖验证；
5. V top_n_layers 层分布差异验证；
6. compare_material_profiles.ps1；
7. 3MF Texture2DGroup 与 OBJ/MTL Texture 样例进入 quick regression；
8. run_regression.ps1 -Mode quick 通过。
```

因此，05A 主功能可以收口。

---

## 2. 为什么建议进入 07

当前项目已经具备从输入到输出的完整调试链路：

```text
OBJ / MTL / Texture
3MF basematerial / ColorGroup / Texture2DGroup
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
Support
p0.rgbwsv.2 SlicePackage
rip_reader_test summary / quiet
quick / full / heavy regression
```

但目前所有操作仍依赖：

```text
命令行
手工编辑 JSON
手工打开 reports
手工查看 preview 文件
手工比较 profile
```

05A 已经产出大量可诊断报告，下一阶段最需要的是：

```text
可视化查看
快速运行
快速对比
快速定位错误
```

因此推荐进入：

```text
07：Qt 调试 UI
```

---

## 3. 07 阶段定位

07 是调试 UI / 工程诊断 UI，不是最终生产 UI。

07 的目标：

```text
以 Qt 5.15 Widgets 构建本地调试工具，
包装 slicer_cli / rip_reader_test / regression / profile compare，
读取现有 manifest、reports、preview，
提供可视化查看和诊断入口。
```

---

## 4. 07 必须保持的冻结项

07 不改变核心算法与输出协议：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
MaterialRoleMapping 语义不变
MaterialPolicy 语义不变
MaterialProcessProfile 语义不变
S support 仍由 Support pipeline 独立生成
```

---

## 5. 07 不做什么

07 不做：

```text
设备通信
喷头 bitstream
RIP 半色调
ICC / CMYK
OpenVDB / SDF
新的切片算法
3MF CompositeMaterials 完整语义
复杂支撑形态优化
生产级权限系统
多设备任务调度
```

---

## 6. 推荐实现策略

07 第一版推荐采用：

```text
Qt Widgets + QProcess 调用现有命令行工具
```

而不是直接把所有 slicer_core 调入 UI 线程。

原因：

```text
1. 不污染 slicer_core；
2. 不破坏 CLI 回归；
3. UI 崩溃不影响核心库；
4. 后续可逐步把部分能力改为 direct library call；
5. QProcess 更适合快速封装 slicer_cli、rip_reader_test、PowerShell 脚本。
```

---

## 7. 07 完成后的后续路线

07 完成后再判断：

```text
路线 A：07A Qt 参数编辑与 profile 可视化增强；
路线 B：08 支撑形态与工艺优化；
路线 C：06C 复杂 3MF 材料扩展；
路线 D：09 OpenVDB / SDF 几何内核预研；
路线 E：RIP/设备侧真实集成文档。
```

当前建议：

```text
07 完成后，根据调试发现的问题决定 08 或 06C。
```
