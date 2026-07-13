# ROADMAP_12D 材料闭环分阶段执行路线

> 文档状态：ROADMAP / Ready
> 日期：2026-07-13

## 1. 目标

将“横截面看起来接触”升级为可检测、可报告、可选修复、可回归的生产语义闭环能力。

## 2. 依赖

```text
12A：提供 TextureSurface、ModelFill、SupportFill、OuterVarnish 语义；
12C：提供 generated config、统一 layer state 和 DiagnosticsDock；
12D：不得改变 12A 材料优先级或 RGBWSV 协议。
```

## 3. R0 文档准入

状态：COMPLETE

```text
关闭需求开放项；
冻结 report schema；
建立 synthetic/real fixture matrix；
冻结 candidate/exact/repair 边界；
生成任务和执行指令。
```

## 4. R1 候选诊断链路

```text
12D-02 MaterialClosureConfig；
12D-03 MaterialClosureReport writer；
12D-04 RGBWSV TIFF inferred candidate detector。
```

退出标准：candidate report 可解析、不修改 TIFF、不允许 production pass。

## 5. R2 精确诊断链路

```text
12D-05 composer semantic masks 接入；
12D-06 repair-disabled TIFF 不变性。
```

退出标准：exact 逐层分类不依赖 preview，diagnostic 模式 TIFF hash 不变。

## 6. R3 修复与产品验收

```text
12D-07 显式 1px repair；
12D-08 external background guard；
12D-09 Qt DiagnosticsDock 展示；
12D-10 三个真实模型验收。
```

退出标准：1px fixture 可修复、2px 不误修、背景全 255、UI 可定位 worst layer、真实模型报告完整。

## 7. 回退

```text
materialClosure.enabled=false：回到 12A 既有输出；
mode=diagnostic + repair.enabled=false：保留报告但不改生产 mask；
UI 无报告时显示 not_available，不自行推断。
```

## 8. 阶段顺序

```text
12C-R1-03 -> 12C-R1-04
-> 12C-R2-01..05
-> 12D-R1
-> 12D-R2
-> 12D-R3
```
