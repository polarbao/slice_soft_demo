# ROADMAP_12_切片语义引擎性能UI专项路线

> 文档版本：v0.1
> 文档状态：ROADMAP / Stage 12
> 生成日期：2026-07-05

---

## 1. 阶段目标

Stage 12 的目标是把 11B 后暴露的问题从“单点修复”升级为“正式专项路线”：

```text
12A：先定义正确切片语义；
12B：再判断哪种引擎能以更低耗时实现同一语义；
12C：最后让 UI 以用户可理解的方式配置和预览这些语义。
```

---

## 2. 推荐执行顺序

### Phase 12A-0：需求确认与术语冻结

输出：

```text
ModelLayerSemantic
TextureSurfaceLayer
ModelFillLayer
SupportFillLayer
OuterVarnishShell
InternalVoidSupport
Upper/Lower Support Placement
```

退出标准：

```text
用户确认“填充层”和“支撑填充”的业务含义；
PRD_12A 更新为可验收版本；
不得先写代码改变输出语义。
```

### Phase 12A-1：legacy 语义补齐

输出：

```text
ModelFillPolicy
SupportPlacementPolicy
InternalVoidSupportPolicy
OuterVarnishShellPolicy 配置占位
```

退出标准：

```text
单材料和彩色纹理模型走同一套模型/支撑/材料语义；
support_report / slice_report 可解释每类填充；
fixture 有 golden summary。
```

### Phase 12A-2：外侧光油与支撑增强

输出：

```text
外侧光油 AdditiveGrow 基础实现；
厚度 px/mm 换算；
内部镂空支撑填充；
上/下/上下支撑策略。
```

退出标准：

```text
RIP reader pass；
LayerPreview 可区分 RGB/W/S/V；
典型甲片模型视觉检查和统计报告一致。
```

### Phase 12B-1：core-only benchmark 扩展

输出：

```text
Release legacy benchmark；
Release OpenVDB benchmark；
同模型、同姿态、同 layerThickness、同 dpi；
coreComputeMs/endToEndMs 分离；
outputSemanticsComparable 判断。
```

退出标准：

```text
不能比较时报告原因；
能比较时输出耗时比、内存峰值、通道统计差异。
```

### Phase 12B-2：高效引擎路线评估

候选：

```text
legacy 优化：active edge table / z-bucket / BVH / 多线程 / tile cache；
2.5D heightfield fast path；
OpenVDB 仅用于 SDF/壳层/光油/clearance；
GPU raster / compute shader；
Embree/BVH CPU ray query；
hybrid：legacy 生产切片 + OpenVDB 局部策略。
```

退出标准：

```text
形成 DOC_DECISION：默认引擎、候选引擎、实验引擎各自用途。
```

### Phase 12C-1：UI 设置页产品化

输出：

```text
基础设置；
材料设置；
支撑设置；
光油设置；
性能/引擎设置；
预览设置；
高级/测试 fixture 隐藏策略。
```

退出标准：

```text
普通用户不必从 samples/configs 选择大量 JSON；
每个设置有短说明和文档链接。
```

### Phase 12C-2：预览工作区整合

输出：

```text
统一“预览”入口；
模式：生产层检查 / 材料叠加 / 原始文件；
报告和曲线移动到可折叠诊断区域；
像素探针和图例常驻。
```

退出标准：

```text
UI smoke 覆盖模式切换；
不降低 layer preview 当前生产检查能力。
```

---

## 3. 阶段依赖

```text
12C 依赖 12A 的术语和策略枚举；
12B 的替代结论依赖 12A 的输出语义；
OpenVDB replacement gate 依赖 12A 支撑/材料等价和 12B Release benchmark；
```

---

## 4. 不推荐路线

```text
1. 不建议直接把 OpenVDB 设为默认引擎；
2. 不建议直接合并所有 samples/configs；
3. 不建议在没有 ModelFillPolicy 的情况下继续加零散 materialPolicy 开关；
4. 不建议把 UI preview 的颜色当作生产 TIFF 语义；
5. 不建议用 Debug 端到端耗时判断引擎优劣。
```

---

## 5. 阶段验收总表

| 阶段 | 必须输出 | 验收方式 |
|---|---|---|
| 12A | PRD/DEV/fixture/golden/report | 典型模型切片 + RIP + LayerPreview + reports |
| 12B | benchmark report + engine decision | Release core-only benchmark + comparison matrix |
| 12C | UI 设置页 + 统一预览方案 | UI self-test + smoke + 手册 |
