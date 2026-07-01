# DEV_11_MultiModel_CapabilityDecision

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 11-5
> 生成日期：2026-07-01
> 任务：Task 11-5 Multi-model capability decision

---

## 1. 结论

Stage 11 不进入多模型 production 切片实现。

Stage 11 只固化多模型能力边界、数据模型、资源隔离和验证口径：

```text
允许：UI / scene / report 层表达多模型能力；
允许：读取 fixture 或配置样例进行 capability report；
允许：评估顺序切片和联合切片；
禁止：默认生成多模型 production RGBWSV package；
禁止：修改 p0.rgbwsv.2、RGBWSV 通道顺序、8-bit 位深和 black_is_print 极性。
```

---

## 2. 当前依据

当前代码事实：

```text
src/slicer_core/scene/SceneModel.h 已有 SceneModel / SceneSummary 轻量边界；
当前 manifest.source 仍只记录单一 modelPath / format；
当前 layer stats 不含 modelId / instanceId 拆分；
当前 support / varnish / texture transfer 仍按单模型或单 scene 聚合统计；
当前 UI 可读取 package / report / preview，但没有多模型列表和实例变换编辑器。
```

因此，多模型不能被视为“传入多个文件”这么简单。它会影响坐标、资源、材质、支撑、光油、纹理、统计和验收。

---

## 3. 多模型数据模型

### 3.1 MultiModelScene

建议后续 experimental scene 使用：

```json
{
  "schema": "p0.multimodel_scene.1",
  "productionEnabled": false,
  "models": [],
  "instances": [],
  "resourceScopes": [],
  "admission": {}
}
```

### 3.2 SceneModelSource

```json
{
  "modelId": "model_001",
  "sourcePath": "samples/models/a.obj",
  "format": "obj",
  "resourceScopeId": "scope_001",
  "defaultMaterialProfile": "nail_rgb_white_varnish",
  "declaredMaterials": [],
  "declaredTextures": []
}
```

规则：

```text
modelId 标识几何/材质资源来源；
同一 modelId 可以被多个 instanceId 引用；
modelId 不等于文件名，避免同名资源冲突；
sourcePath 只用于导入，不进入 TIFF 协议。
```

### 3.3 ModelInstance

```json
{
  "instanceId": "inst_001",
  "modelId": "model_001",
  "transform": {
    "translateMm": [0.0, 0.0, 0.0],
    "rotateDeg": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0]
  },
  "visible": true,
  "locked": false,
  "productionEligible": false
}
```

规则：

```text
instanceId 标识一次摆放；
transform 必须在进入切片前归一化为毫米坐标；
productionEligible 默认 false，除非后续阶段完成 admission；
UI 可以隐藏 / 锁定 / 选择 instance，但不能绕过 admission。
```

### 3.4 ResourceScope

```json
{
  "resourceScopeId": "scope_001",
  "baseDir": "samples/models/a",
  "mtlBaseDir": "samples/models/a",
  "textureBaseDir": "samples/models/a/textures",
  "pathPolicy": "relative_to_model"
}
```

规则：

```text
OBJ / MTL / texture 的相对路径必须限制在 resourceScope 内解释；
不同 modelId 的同名 texture 不共享缓存 key；
3MF 内部关系资源使用 package-local scope；
后续报告必须能指出 missing texture 属于哪个 modelId / resourceScopeId。
```

---

## 4. 切片策略评估

### 4.1 顺序切片

定义：

```text
逐 model / instance 调用现有单模型 pipeline；
每个 instance 生成独立 package 或独立 intermediate；
最终可由上层 job 编排。
```

优点：

```text
最小化对 p0.rgbwsv.2 的影响；
复用现有单模型验证；
错误定位清晰。
```

风险：

```text
不能处理跨模型重叠；
不能联合支撑；
不能在同一 build volume 内做统一 layer stats；
输出 package 合并仍需新契约。
```

Stage 11 结论：

```text
可作为下一阶段优先候选，但仍需 report / package orchestration 设计。
```

### 4.2 联合切片

定义：

```text
所有 instance 先进入同一 scene / build volume；
统一 rasterize / support / material / texture / varnish；
输出单个 package。
```

优点：

```text
可以处理重叠、碰撞、联合支撑和统一统计；
更接近正式生产排版。
```

风险：

```text
会触碰几何合并、材质冲突、资源隔离、per-model stats、support 联合优化；
需要 build volume / placement / nesting 子系统；
需要扩展 package metadata，但不能破坏 p0.rgbwsv.2。
```

Stage 11 结论：

```text
不进入实现；只保留为后续独立阶段候选。
```

---

## 5. Capability Report

建议后续生成：

```json
{
  "schema": "p0.multimodel_capability_report.1",
  "productionAllowed": false,
  "recommendedPath": "sequential_first",
  "models": [],
  "instances": [],
  "resourceScopes": [],
  "blockerCodes": [],
  "warningCodes": [],
  "nextStageRecommendations": []
}
```

首批 blocker code：

```text
MULTI_MODEL_PRODUCTION_DISABLED
MODEL_INSTANCE_STATS_UNAVAILABLE
BUILD_VOLUME_NOT_DEFINED
PLACEMENT_ENGINE_NOT_DEFINED
JOINT_SUPPORT_NOT_VALIDATED
RESOURCE_SCOPE_NOT_VALIDATED
PACKAGE_METADATA_NOT_EXTENDED
```

---

## 6. UI 表达

Stage 11 UI 可以显示：

```text
model list；
instance list；
transform 只读摘要；
resource scope；
capability blocker / warning；
production disabled 标记。
```

Stage 11 UI 不做：

```text
复杂自动排版；
跨模型碰撞求解；
联合支撑编辑；
多模型 production run 按钮；
绕过 OpenVDB / geometry admission。
```

---

## 7. REPORT_11 必答项

REPORT_11 需要记录：

```text
多模型当前只完成能力评估和数据模型；
production 多模型输出未启用；
建议下一阶段优先做顺序切片 orchestration；
联合切片需要 build volume / placement / nesting / package metadata 后再进入；
任何多模型输出不得修改 p0.rgbwsv.2 的通道协议。
```
