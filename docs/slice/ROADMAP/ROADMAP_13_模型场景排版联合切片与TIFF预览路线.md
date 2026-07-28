# ROADMAP_13 模型场景、排版联合切片与 TIFF 原生预览路线

> 版本：v0.8
> 日期：2026-07-28
> 状态：P0 DESIGN AND ATOMIC PREPARATION COMPLETE / 13A-01..05、13B-01..07、13C-01/02、跨阶段 09A-02 COMPLETE / NEXT 13C-03 PREPARATION AUDIT

## 1. 总目标

把当前“导入单个模型后直接切片”的调试流程演进为：

```text
导入模型 -> 场景俯视检查 -> 选择和变换 -> 多模型规则排版
-> 几何/幅面/碰撞准入 -> 联合切片 -> 单一 RGBWSV package
-> 直接从生产 TIFF 检查单通道和全材料叠加。
```

## 2. 执行阶段

### R0 文档与合同

```text
Stage 13 专项拆分；
scene/model/instance/transform 术语；
排版间距语义；
TIFF 原生预览边界；
与 12E-09A/10 的依赖；
PRD/DEV/DEMO/TASKS/PROMPT。
```

完成标准：文档一致性检查通过，代码状态仍明确为 NOT STARTED。

2026-07-27 已补齐执行级准备：

```text
13A-01 transform/instance 合同；
13B-01 scene/effective config 合同；
13C-01 TIFF source/cache 合同；
13A-01..05、13B-01..07、13C-01..05 的原子任务实施准备和文件所有权；
未决设备和产品输入 Gate；
12X 剩余任务优先级和 12G-TCWS 冻结决策；
Stage 12/13 跨阶段执行看板。
```

### 13A-R1 俯视工作区

```text
ModelTransform/ModelInstance DTO；
从 +Z 查看 XY 俯视图；
选择、移动 XY、rotateZ、uniformScale；
mirrorX/mirrorY；
Z 落台与当前流程一致；
post-transform preflight；
UI smoke。
```

### 13B-R1 场景与配置

```text
MultiModelScene schema；
modelId/instanceId/resourceScope；
Scene Effective Config；
模型列表、复制、删除、显示、锁定；
单模型配置兼容迁移。
```

13B-R1 的 scene identity 与后续 `12E-09A-02` 已完成，Diagnostic Effective Config 已同时绑定
single_model 与 scene/current instance，不再只依赖 modelPath。

### 13B-R2 规则排版

```text
11 列 x 2 行；
列间净距默认 20 mm；
行间净距默认 30 mm；
UI 可配置；
规则排版后允许手动调整；
buildVolume 越界与碰撞 fail-closed。
```

### 13B-R3 联合切片

```text
逐实例变换和准入；
统一 Z 层序和全局 XY raster；
逐实例材料/支撑层映射；
场景层合成；
单一 p0.rgbwsv.2 package；
scene report 和 per-instance stats；
RIP strict 与真实模型矩阵。
```

### 13C-R1 TIFF 原生统一预览

```text
异步 TIFF 解码和 LRU；
单通道 R/G/B/W/S/V；
RGB、RGB+W、RGB+S、RGB+V、RGB+S+W+V；
统一 layerIndex/zMm；
像素探针；
常规生产流程不再依赖逐通道 preview PNG。
```

13C-R1 在 `12E-09A-05` 前完成，使诊断语义预览复用同一层状态和 TIFF 生产底图。

### 13A-R2 中期 3D Viewport

```text
VTK 与 QOpenGLWidget 技术 Spike；
真实透视/正交相机；
模型拾取、轨道相机、包围盒、材质/纹理显示；
部署体积、启动时间、帧率和许可证 Gate。
```

### 13A-R3 长期交互和 13B-R4 自动排版

```text
三轴 gizmo；
撤销/重做；
对齐、吸附、复制；
非均匀缩放策略；
自动朝向；
真正 nesting；
跨模型联合支撑；
增量重切片和缓存。
```

## 3. 与当前路线的正式顺序

```text
13-R0 文档准备
  -> 13A-01 + 13B-01
  -> 12E-09A-02
  -> 13A-R1
  -> 13B-R2/R3
  -> 13C-R1
  -> 12E-09A-03..06
  -> 12E-10A..D
  -> 13A-R2/R3、13B-R4
```

如果多模型业务优先级降低，可在 `13A-01 + 13B-01 + 12E-09A-02` 后暂时恢复 12E；但不得在没有
scene identity 的情况下按旧单模型假设完成 09A-02。

单贡献者执行时采用以下原子顺序：

```text
13A-01 -> 13B-01 -> 12E-09A-02
-> 13A-02..05 -> 13B-02..07
-> 13C-01..03 -> 12E-09A-03..06
-> 13C-04..05 -> 12E-10A..D。
```

若产品决定优先关闭 Stage 12，可在 09A-02 后调整为
`13C-01..03 -> 09A-03..06 -> 12E-10`，但必须保留 scene identity 和 TIFF 原生预览前置。

## 4. 里程碑

| 里程碑 | 可交付能力 | 生产状态 |
|---|---|---|
| M13-1 | 单模型俯视、选择和 XY 变换 | 可进入现有单模型切片 |
| M13-2 | 多模型列表与 11x2 规则排版 | 仅场景准备，不代表可打印 |
| M13-3 | 联合切片与单一 RGBWSV package | 通过 Gate 后可作为候选 |
| M13-4 | TIFF 原生统一预览 | 生产检查不依赖 preview PNG |
| M13-5 | 中期真实 3D viewport | 交互增强，不改变切片协议 |
| M13-6 | 自动 nesting 与高级交互 | 长期规划 |

## 5. 准备完成度

```text
Stage 13 R0 总体文档：COMPLETE；
Stage 13 P0 需求/设计/验证/原子任务准备：COMPLETE；
13A-01：COMPLETE；
13A-02：COMPLETE；
13B-01：COMPLETE；
12E-09A-02：COMPLETE；
13A-03：COMPLETE；
13A-04：COMPLETE；
13A-05：COMPLETE / M13-1 CANDIDATE PASS；
13B-02：COMPLETE；
13B-03：COMPLETE；
13B-04：FUNCTIONAL FIXTURE COMPLETE / PRODUCTION INPUT OPEN；
13B-05：FIXTURE COMPLETE；
13B-06：FIXTURE COMPLETE / PRODUCTION INPUT OPEN；
13B-07：FUNCTIONAL MATRIX COMPLETE，production GO 等待设备输入和预算；
13C-01：COMPLETE；
13C-02：COMPLETE；
13C-03：DEPENDENCY READY / PREPARATION AUDIT NEXT；
完整 Stage 13 production readiness：INCOMPLETE。
```

未完成项不是缺少通用 PRD/DEV，而是设备/Profile 外部输入和后续原子任务的逐步证据：

```text
buildVolume/轴方向；
22 实例性能预算；
未来 mixed-profile 决策；
中期 3D backend Spike；
13A/13B/13C 实际代码、测试和真实模型报告。
```

## 6. 回滚策略

```text
Stage 13 全部通过新 scene/Profile 显式启用；
单模型 legacy/global_surface_shell 入口继续保留；
scene 配置失败不得改写原单模型 fixture；
联合切片失败不得静默拆成多个 package；
TIFF 原生预览失败时可显示错误，不自动读取跨层 PNG；
preview PNG 调试开关保留，便于回归和算法诊断。
```
