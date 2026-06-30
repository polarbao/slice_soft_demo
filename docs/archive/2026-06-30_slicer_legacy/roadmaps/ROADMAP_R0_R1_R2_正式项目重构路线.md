# ROADMAP_R0_R1_R2_正式项目重构路线

> 文档版本：v0.1  
> 文档状态：Roadmap Draft  
> 适用阶段：R0 / R1 / R2  
> 建议提交目录：`docs/slicer/`

---

## R0：架构审查与重构设计

性质：

```text
文档和架构设计阶段
```

输出：

```text
模块边界
目录结构
pipeline step
config schema
report schema
test/CI 策略
R1/R2 任务边界
```

不做：

```text
大规模移动代码
复杂策略实现
OpenVDB
设备通信
```

---

## R1：核心模块边界重构

性质：

```text
代码结构重构阶段
```

目标：

```text
拆分 model.cpp / slicer.cpp
建立 scene / importer / pipeline / materials / support / output / reports 边界
保持 quick regression 通过
```

R1 任务应在 R0 完成后再详细生成。

---

## R2：配置、报告、测试、CI 工程化收口

性质：

```text
工程化固化阶段
```

目标：

```text
config schema version
config migration
report schema version
unit/golden/regression/ui smoke test 分层
CI 入口
错误码和 diagnostics 统一
```

R2 任务应在 R1 实际拆分完成后再细化。

---

## 是否现在生成 R1/R2 详细任务

不建议现在生成完整 R1/R2 执行文档。

建议现在只生成本 roadmap，原因：

```text
R1 的具体任务依赖 R0 架构审查结论；
R2 的具体任务依赖 R1 拆分后的实际模块边界。
```

现在如果提前写死 R1/R2 细任务，容易和 R0 结果冲突。
