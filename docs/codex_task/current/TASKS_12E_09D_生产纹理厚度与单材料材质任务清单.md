# TASKS_12E-09D 生产纹理厚度与单材料材质任务清单

> 文档状态：PREPARED / WAIT 03D-LIBTIFF
> 日期：2026-07-31

## 12E-09D-01 合同与配置映射

状态：PREPARED

```text
新增 DTO/错误码/字段映射单测；
冻结 Legacy Z layer band、Global normal shell 和诊断宽度三种语义；
不改变输出。
```

## 12E-09D-02 Production Texture Settings

状态：WAIT 09D-01

```text
实现 requested/effective/backend；
Legacy topSurfaceLayers 与有效 Z 厚度；
Global width/mode；
session effective config。
```

## 12E-09D-03 Single Material Relief Resolver

状态：WAIT 09D-02

```text
W/V 原子字段组；
配置一致性校验；
单测和负向错误。
```

## 12E-09D-04 Qt 生产控件

状态：WAIT 09D-03

```text
右侧切片设置条件化控件；
诊断/生产视觉分离；
stale、保存、回读、Profile 锁定。
```

## 12E-09D-05 一键切片与状态证据

状态：WAIT 09D-04

```text
单模型/scene effective config；
运行摘要和报告；
UI Smoke；
TIFF 原生预览。
```

## 12E-09D-06 Release 矩阵与收口

状态：WAIT 09D-05

```text
Legacy 1/3/10；
Global min/mid/allTexture；
single relief W/V；
RIP strict；
REPORT、用户说明、上下文。
```

## 停止条件

```text
不得在 03D 优先任务前擅自开始；
不得把诊断值直接写成生产值；
不得把 Legacy 层数描述成 Global 宽度；
不得补丁式修改纯白 RGB；
不得修改固定 TIFF 协议。
```
