# DOC_DECISION_R1_R0后进入核心模块边界重构阶段

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_R0 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：R0 架构审查完成后，进入 R1 核心模块边界重构阶段

---

## 1. 阶段判断

根据 `REPORT_R0_正式项目架构审查与重构设计当前状态.md`，R0 已完成以下判断：

```text
1. 当前项目进入 P0 Demo Feature Freeze；
2. R0 已完成架构审查与设计输出；
3. model.cpp / slicer.cpp 职责集中度已经不适合继续堆功能；
4. 正式目录结构、pipeline step、config/report/test 方向已经明确；
5. TextureApplicationPolicy / VarnishGeometryPolicy 已纳入正式架构约束；
6. R1 可以进入，但必须只做模块边界重构，不新增大型功能。
```

因此建议进入：

```text
R1：核心模块边界重构
```

---

## 2. R1 阶段性质

R1 是代码结构重构阶段，不是新功能阶段。

R1 目标：

```text
把当前 Demo 的集中式实现逐步拆成正式项目模块边界，
但保持输出协议、CLI、RIP Reader、Qt Debug UI、regression 行为不变。
```

---

## 3. R1 必须遵守的边界

R1 必须保持：

```text
1. p0.rgbwsv.2 输出协议不变；
2. R/G/B/W/S/V 通道顺序不变；
3. 8-bit / black_is_print 极性不变；
4. slicer_cli 基本命令行不变；
5. rip_reader_test 基本命令行不变；
6. slicer_debug_ui target 可构建；
7. quick regression 必须作为每个拆分点的守门；
8. R1 不实现 surface_shell_texture；
9. R1 不实现 compensated_varnish；
10. R1 不引入 OpenVDB；
11. R1 不引入设备通信。
```

---

## 4. R1 方法论

R1 必须使用：

```text
wrap first
move later
rewrite last
```

含义：

```text
1. 先把当前 slicer.cpp/model.cpp 内的阶段逻辑包成小函数或 step；
2. 再移动到新模块文件；
3. 最后才考虑内部实现优化；
4. 每次移动后运行 quick regression。
```

---

## 5. R1 与 R2 的边界

R1 负责：

```text
模块拆分
目录结构落地
pipeline step wrapper
importer / material / support / output / reports 边界建立
```

R2 负责：

```text
config schema version
config migration
report schema 统一
unit/golden/schema/ui smoke test 分层
CI 入口固化
```

不要把 R2 的 schema 工程化任务提前塞进 R1。

---

## 6. 是否现在制定 R2 详细任务

不建议现在制定 R2 详细执行任务。

现在只保留 R2 边界说明即可。R2 的具体任务依赖 R1 实际拆分结果。

---

## 7. R1 完成后的状态报告

R1 完成后必须生成：

```text
docs/slicer/REPORT_R1_核心模块边界重构当前状态.md
```

报告中必须说明：

```text
1. 拆分了哪些模块；
2. 哪些逻辑仍保留在 legacy 文件；
3. quick regression 是否通过；
4. 输出协议是否不变；
5. 是否可以进入 R2。
```
