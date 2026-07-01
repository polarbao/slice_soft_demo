# PRD_11_UI切片层预览交互配置与多模型能力

> 文档版本：v0.1
> 文档状态：Formal PRD / Stage 11
> 生成日期：2026-06-30
> 阶段定位：UI 切片层预览、交互配置、多模型能力评估

---

## 1. 阶段结论

当前只新增一个后续阶段：

```text
11：UI 切片层预览 / 交互配置 / 多模型能力评估
```

不新增独立 12 阶段。多模型先纳入 11 阶段做能力评估、数据模型设计和最小范围判断；是否进入完整多模型生产切片能力，必须等待 11 阶段 REPORT 后再决策。

---

## 2. 背景

当前项目主线不实现 RIP、设备通信、喷头 bitstream 或 RIP 半色调。本项目负责把模型切片为保留足够纹理、材料、支撑、白墨、光油信息的稳定输出。

已有 Qt Debug UI 可用于调试配置、查看报告和预览，但还不足以承担正式切片软件的使用体验：

```text
1. 切片完成后无法按层滑动查看每层切片结果；
2. 现有 UI 布局偏调试，正式作业视角不足；
3. 许多参数仍依赖配置文件修改；
4. 当前导入和切片逻辑以单模型为主，多模型能力边界不清楚。
```

---

## 3. 用户目标

### 3.1 层预览

用户需要在切片完成后，通过 UI 滑动浏览每一层切片数据。

最小能力：

```text
层号滑动条；
上一层 / 下一层；
当前层高度；
伪彩显示；
通道切换；
缩放 / 平移；
层统计摘要。
```

### 3.2 伪彩显示

伪彩用于帮助工程师检查切片结果，而不是替代真实打印色彩。

建议首批视图：

```text
RGB composite preview；
W 白墨强度；
S 支撑 mask；
V 光油 mask；
occupancy / model mask；
texture fidelity / fallback 标记；
diagnostic overlay。
```

### 3.3 UI 布局优化

目标是从 debug UI 过渡到更接近作业式工作台的布局：

```text
模型 / 作业区；
切片层预览区；
参数配置区；
报告 / 诊断区；
输出摘要区；
状态栏和错误提示。
```

### 3.4 交互配置

常用配置应可在 UI 中修改，不再每次手动编辑配置文件。

首批配置候选：

```text
layer height；
resolution / pixel size；
material profile；
texture application policy；
support enable / type；
white / varnish policy；
OpenVDB experimental enable 只作为显式实验开关；
output directory；
preview generation。
```

### 3.5 多模型能力评估

当前不直接承诺完整多模型 production 切片。11 阶段先回答：

```text
是否允许一次导入多个模型？
多个模型是否共享一个 build volume？
是否需要模型位姿、排版、碰撞检查？
多模型材质和纹理资源如何隔离？
输出报告如何记录 modelId / instanceId？
多模型是联合切片还是顺序切片？
```

---

## 4. 非目标

```text
不实现 RIP 半色调；
不实现设备通信；
不实现喷头 bitstream；
不修改 p0.rgbwsv.2；
不绕过 report/package 让 UI 直接访问 slicer.cpp 内部结构；
不让 UI 直接依赖 OpenVDB 类型；
不默认启用 OpenVDB；
不在 11 阶段承诺完整多模型 production 输出。
```

---

## 5. 验收标准

11 阶段完成后应满足：

```text
1. UI 能读取切片输出或预览数据契约；
2. UI 能按层滑动显示伪彩切片图；
3. UI 能切换关键通道视图；
4. UI 常用配置项不再只依赖手工编辑文件；
5. UI 布局形成正式作业工作台雏形；
6. 多模型能力有明确决策文档；
7. 相关 UI smoke / golden preview 验证可运行；
8. 输出 REPORT_11，说明已实现、未实现、风险和下一步。
```

