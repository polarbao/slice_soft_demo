# DOC_DECISION_R2_R1后进入配置报告测试CI工程化固化阶段

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_R1 之后  
> 建议提交目录：`docs/slicer/`

## 1. 阶段判断

根据 `REPORT_R1_核心模块边界重构当前状态.md`，R1 已完成第一轮核心模块边界重构：

```text
scene / importers / pipeline / materials / support / raster / output / reports / diagnostics 边界已落地
wrapper/API 已可编译
legacy 行为未改变
quick regression 通过
UI self-test 通过
overlay-load-real 通过
输出协议保持 p0.rgbwsv.2 不变
model.cpp / slicer.cpp / config.cpp legacy 留存职责已明确
```

因此可以进入：

```text
R2：配置、报告、测试与 CI 工程化固化
```

## 2. 为什么进入 R2

R1 建立的是模块边界，但工程化基础设施仍未固化：

```text
config 仍以 legacy SliceConfig 为主
config schema 尚未版本化
report schema 风格尚未统一
diagnostics / error code 尚未统一为跨模块结构
unit / golden / schema / ui smoke 测试尚未形成正式分层
CI 守门入口尚未固化
```

## 3. R2 必须保持不变

```text
不修改 p0.rgbwsv.2
不修改 R G B W S V 通道顺序
不修改 8-bit / black_is_print 极性
不修改 slicer_cli / rip_reader_test 基本调用方式
不实现 surface_shell_texture
不实现 compensated_varnish
不引入 OpenVDB
不引入设备通信
不做 RIP 半色调
```

## 4. R2 完成后路线

R2 完成后，才建议进入：

```text
08：支撑形态与工艺优化
```
