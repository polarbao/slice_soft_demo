# DOC_DECISION_12E 全局纹理表面层与模型填充互补策略

> 文档状态：Decision / Stage 12E Planning
> 日期：2026-07-16
> 上游阶段：12A 彩色纹理材料填充支撑光油策略、12B-R2 OpenVDB SDF Utility、12C Qt 工作台、12D 横截面材料无缝闭环
> 实现状态：PARTIAL；12E-01..07 与 12E-08A/08B/08C COMPLETE，Release budget BLOCKED，12E-08D BLOCKED

## 1. 决策结论

新增 Stage 12E：全局纹理表面层与模型填充互补策略。

12E 不把 `Texture Surface Layer` 和 `Model Fill Layer` 继续作为两个互不关联的开关，而是把它们定义为模型实体体积上的互补分区：

```text
TextureSurfaceVolume(widthMm) = ModelVolume 内距离完整三维模型表面不超过 widthMm 的区域
ModelFillVolume(widthMm) = ModelVolume - TextureSurfaceVolume(widthMm)
```

必须满足：

```text
TextureSurfaceVolume ∩ ModelFillVolume = Empty
TextureSurfaceVolume ∪ ModelFillVolume = ModelVolume
UnassignedModelVolume = Empty
```

`widthMm` 增大时，纹理表面体积单调增大，模型填充体积单调减小。达到当前模型的全纹理阈值后，`ModelFillVolume` 可以为空，但模型体积不能出现未分配像素或体素。

## 2. 阶段编号选择

本补充不回写为 12A 已实现能力，而定义为 12E，原因如下：

```text
1. 12A 的 P0/P1 已完成并有状态报告，直接改写会混淆历史实现状态；
2. 12D 当前正在执行材料闭环诊断，不能被新的几何分区实现打断；
3. 12E 需要全模型三维距离、动态宽度上限、薄壁合并和 UI 联动，已超出 12A 的局部补丁范围；
4. 12E 可以复用 12B-R2 的 SDF 证据，但不能自动把 OpenVDB 提升为默认生产依赖。
```

12E 文档可以现在建立；代码实现必须等待用户明确启动某个 12E 原子任务。

## 3. Planning Baseline（建立决策时的 Current State）

当前 A 级代码事实：

```text
1. TextureConfig 只有 apply_mode/top_surface_layers 等字段，没有连续 widthMm；
2. legacy ShouldApplyTextureToLayer 基于列和 layer index 判断 top surface/band/full volume；
3. ModelFillConfig 已有 material/scope/value，但 scope 未与三维纹理壳层宽度形成正式互补契约；
4. compose_layer 可统计 textureSurfacePixels/modelFillPixels，但当前分类仍来自逐层/逐列判断；
5. OpenVdbSurfaceShell 已能在完整三维 SDF 上得到 inside/shell/interior mask，默认原型厚度为 0.10 mm；
6. OpenVDB 仍是 optional、disabled-by-default utility candidate，不得直接写生产 RGBWSV TIFF；
7. Qt QuickConfigPanel 已有纹理策略和模型填充材料控件，但没有全局纹理宽度及动态全纹理上限控件。
```

上述内容是 2026-07-16 建立决策时的基线。当前已完成 Config/DTO、分区 service、Legacy CPU/OpenVDB conformance candidate、Width Sweep、纹理传递、diagnostic composer、模型域与完整材料域 closure 和 raster mapping，但尚未完成 Release regression、Qt 或 production package；实际状态以 `REPORT_12E_启动准备状态.md` 为准。

## 4. Target State

12E 目标态：

```text
1. 使用完整变换后模型建立三维 occupancy 和到所有闭合表面的距离；
2. 一次性生成全模型 TextureSurfaceMask3D 与 ModelFillMask3D；
3. 每个切片层只读取三维分区结果，不在单层中重新腐蚀、膨胀或猜测表面；
4. 纹理宽度以 mm 连续配置，UI 步长 0.01 mm；
5. 初始工程最小宽度为 0.10 mm，并受实际分类分辨率下限约束；
6. 最大值按当前模型动态计算，达到最大值时允许 modelFillPixels=0；
7. 每层和整包都证明 texture + fill 恰好覆盖 model，且 overlap/unassigned 均为 0；
8. 12D 的 semantic mask exact 诊断能够直接读取该互补分区。
```

## 5. 宽度边界决策

### 5.1 最小值

首版工程最小值冻结为：

```text
baseMinimumWidthMm = 0.10 mm
resolutionMm = max(classificationVoxelMm, pixelPitchXmm, pixelPitchYmm, layerThicknessMm)
effectiveMinimumWidthMm = max(baseMinimumWidthMm, 2 * resolutionMm)
```

理由：

```text
1. 当前 OpenVDB 原型常用 0.05 mm voxel，0.10 mm 对应至少两个分类单元；
2. 600 dpi 下 42.3 um/px，0.10 mm 约为 2.36 个 XY 像素，离散后约覆盖 3 像素；
3. 小于两个最粗分类单元的壳层容易受量化、薄壁和锯齿影响；
4. 0.10 mm 是工程稳定下限，不是最终材料工艺标定值，生产 Profile 后续可以提高下限。
```

### 5.2 最大值

最大值不使用固定常量，而按模型计算：

```text
maxInteriorDistanceMm = max(distanceToClosedSurface(p)), p ∈ ModelVolume
allTextureThresholdMm = max(
  effectiveMinimumWidthMm,
  ceil(maxInteriorDistanceMm / 0.01) * 0.01)
```

当 `widthMm >= allTextureThresholdMm` 时：

```text
TextureSurfaceVolume = ModelVolume
ModelFillVolume = Empty
modelFillPixels = 0
```

浮点比较必须使用与分类分辨率一致的 epsilon，报告同时记录 requested/effective/threshold，不能只显示 UI 请求值。

### 5.3 连续设置含义

`widthMm` 允许按 0.01 mm 连续调节。由于最终输出是离散体素和像素，相邻两个请求值可能得到相同 mask；这属于量化结果，UI 和 report 必须显示 effective width/coverage，不得伪装成每 0.01 mm 都会改变一个像素。

## 6. 全模型处理边界

禁止把 12E 实现成逐层二维轮廓腐蚀或顶面层数扩展。

正式判定必须基于：

```text
1. 完整三维闭合模型及其内外部分类；
2. 到外表面、内腔表面等所有闭合表面的三维最短距离；
3. 薄壁两侧壳层在三维中自然相遇和合并；
4. 纹理颜色通过三维最近表面点/三角形属性传递；
5. 再把已生成的 3D mask 映射到各层 RGBWSV composer。
```

逐层预览只是结果视图，不是几何分类真源。

## 7. “填充为空”的生产解释

12A 的“生产 Profile 不允许内部填充为空”继续适用于以下错误：

```text
modelFill 被禁用，且纹理层未覆盖模型；
模型像素既不属于 TextureSurface，也不属于 ModelFill；
通过 none/empty 配置制造未打印模型区域。
```

12E 新增的合法例外是：

```text
modelFill.enabled 仍为 true；
ModelFillMask 是 TextureSurfaceMask 的严格补集；
因 widthMm 达到模型全纹理阈值，补集自然为空；
partitionPass=true、unassignedModelPixels=0、materialClosure exact PASS。
```

这不是“允许空材料”，而是“所有模型体积已归属纹理表面数据”。

## 8. RGBWSV 与材料组合边界

几何主分类与通道叠加必须分开：

```text
TextureSurfaceMask 与 ModelFillMask 互斥；
TextureSurface 像素可按 MaterialPolicy 叠加 W underbase 或 V surface varnish；
叠加通道不改变该像素的几何主分类；
OuterVarnishShell 和 Support 仍在 ModelVolume 之外按既有优先级处理。
```

继续保持：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
Model > OuterVarnishShell > Support > Empty
```

## 9. 引擎与依赖决策

12E 采用 engine-neutral 的全局距离服务，不把 OpenVDB 类型暴露给 composer 或 UI。

首轮比较：

| 候选 | CMake/vcpkg | 许可证 | 优点 | 风险 | 12E 定位 |
|---|---|---|---|---|---|
| 现有 mesh/BVH + CPU 3D 距离场 | 不新增第三方依赖；默认构建可用 | 项目自有代码 | 可控、默认 OFF lane 不受影响 | 性能、内存和符号距离鲁棒性需要验证 | production candidate |
| 现有 OpenVDB SDF | `USE_OPENVDB=ON`，继续通过独立 vcpkg lane | MPL-2.0 | 已有 whole-model SDF/shell/UV transfer 证据 | optional 依赖、拓扑准入、构建与内存成本 | conformance/utility candidate |

当前决策：不引入新的第三方库；默认构建仍 `USE_OPENVDB=OFF`。OpenVDB 只做交叉验证或明确批准后的候选后端，不能因 12E 自动获得生产写包权限。

## 10. Historical State

历史 C 级文档已提出 `surface_shell_texture` 和 3D SDF 方向，并指出 2D per-layer shell 只是近似。09B/09P 已有 OpenVDB 表面壳层与纹理转移原型；12B-R2 将其收口为 SDF utility candidate。

这些材料为 12E 提供技术证据，但不证明当前 production path 已实现全局纹理/填充互补。

## 11. Pending Confirmation

以下内容在代码实现前仍需通过 12E 原子任务验证或冻结：

```text
1. production candidate 最终采用 CPU 3D 距离场还是经过新决策准入的 OpenVDB 后端；
2. 真实打印工艺是否把 Profile 最小值提高到 0.10 mm 以上；
3. 最近表面颜色在 medial-axis tie 上的确定性规则和可接受误差；
4. 真实模型的内腔表面是否全部参与纹理传递；首版目标为 all_closed_surfaces；
5. 性能与内存预算通过 benchmark 后才能确定 production admission。
```

这些开放项不阻塞 12E-01 的 Config/DTO 契约实现。12E-01 必须保持 backend-neutral，并在 backend 不可用时显式阻断；它不能提前决定 production backend。

## 12. 后续入口

```text
docs/slice/PRD/PRD_12E_全局纹理表面层与模型填充连续调节.md
docs/slice/DEV/DEV_12E_全局纹理壳层与模型填充分区设计.md
docs/slice/DEMO/DEMO_12E_全局纹理壳层与模型填充验证方案.md
docs/slice/ROADMAP/ROADMAP_12E_全局纹理壳层与模型填充分阶段路线.md
docs/slice/DOC/DOC_PREP_12E_R0_ConfigDTO契约准备.md
docs/slice/DOC/DOC_SCHEMA_12E_TextureFillPartitionReport.md
docs/slice/DOC/DOC_MATRIX_12E_全局纹理填充分区验收矩阵.md
docs/slice/DOC/DOC_EXEC_12E_R4A_ClassificationRaster映射结果.md
docs/slice/DOC/DOC_PREP_12E_R5_QtUI与EffectiveConfig准备.md
docs/slice/REPORT/REPORT_12E_启动准备状态.md
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md
```
