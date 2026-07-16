# DEMO_12E 全局纹理壳层与模型填充验证方案

> 文档状态：DEMO / Stage 12E Planning
> 日期：2026-07-16
> 对应 PRD：PRD_12E_全局纹理表面层与模型填充连续调节.md
> 对应 DEV：DEV_12E_全局纹理壳层与模型填充分区设计.md
> 实现状态：NOT STARTED；命令入口需由后续原子任务创建

## 1. 验证目标

证明以下事实：

```text
1. Texture Surface 与 Model Fill 在完整三维模型中互补；
2. widthMm 增大时 texture 单调增加、fill 单调减少；
3. 达到模型动态阈值时 fill 为 0 且 model 无未分配区域；
4. 斜面、侧壁、凹面、内腔和薄壁不是逐层二维近似；
5. UI requested/effective/range/coverage 与 report 一致；
6. 默认 OpenVDB OFF 构建不受影响；
7. OpenVDB ON 只做可选 conformance，不自动写 production TIFF；
8. RGBWSV 协议和 12D material closure 边界保持不变。
```

## 2. 证据层级

```text
L1：partition/config unit tests；
L2：generated geometry golden report；
L3：backend conformance；
L4：semantic mask exact + material closure；
L5：production package + RIP strict；
L6：Qt UI self-test/smoke；
L7：真实模型 Release 性能和内存。
```

preview 截图不能替代 mask/report/TIFF 证据。

## 3. Fixture Matrix

| Case | 目的 | 必须断言 |
|---|---|---|
| generated_closed_box | 最小值和动态最大值 | 分区互补；阈值处 fill=0 |
| generated_sphere_or_sloped_body | 证明三维距离 | 各方向厚度误差在报告阈值内 |
| generated_thin_wall | 两侧纹理合并 | 局部 fill 消失，无 overlap/unassigned |
| generated_closed_cavity | 内腔表面 | all_closed_surfaces 对内外表面一致 |
| generated_concave_body | 凹面和最近表面 | 不使用单层轮廓宽度代替 3D 距离 |
| generated_multi_surface_tie | medial axis tie | 结果确定、tie count 可报告 |
| open/non_manifold/self_intersection | production admission | strict 模式 fail fast，不写包 |
| real_nail_obj | 真实甲片 | 分区、closure、RIP、preview 一致 |
| real_textured_3mf | 3MF 纹理属性 | 最近表面纹理传递和 fallback 统计 |
| OpenVDB OFF/ON pair | backend 边界 | OFF 可用；ON 只做 conformance |

真实模型优先从仓库已有可复现样例中选择，不依赖未跟踪本地文件。

## 4. 宽度 Sweep

每个可用 closed fixture 至少执行：

```text
w0 = effectiveMinimumWidthMm
w1 = 25% of [min, allTextureThreshold]
w2 = 50% of [min, allTextureThreshold]
w3 = 75% of [min, allTextureThreshold]
w4 = allTextureThresholdMm
```

如果模型太薄导致多个点折叠为同一有效宽度，允许去重，但报告必须记录实际 sweep values。

对相邻 sweep 结果：

```text
textureSurfaceCount[i + 1] >= textureSurfaceCount[i]
modelFillCount[i + 1] <= modelFillCount[i]
modelCount 保持一致
overlapCount = 0
unassignedCount = 0
```

终点：

```text
allTexture = true
textureSurfaceCount = modelCount
modelFillCount = 0
```

## 5. 最小值验证

验证：

```text
baseMinimumWidthMm = 0.10
resolutionMm = max(classificationVoxelMm, pixelPitchXmm, pixelPitchYmm, layerThicknessMm)
effectiveMinimumWidthMm = max(0.10, 2 * resolutionMm)
allTextureThresholdMm = max(effectiveMinimumWidthMm, ceil(maxInteriorDistanceMm / 0.01) * 0.01)
```

Case：

```text
1. 默认 600 dpi/当前 layer thickness/0.05 mm classification voxel；
2. 更粗 voxel，使 effective minimum > 0.10 mm；
3. 非有限、0、负数和小于 effective minimum 的请求；
4. widthStepMm 不是 0.01 的非法配置。
```

非法配置必须明确拒绝，不得 clamp 后静默写生产包。UI 可在提交前 clamp，但 effective config 必须记录 requested/clamped。

## 6. 全模型而非逐层验证

### 6.1 斜面/球面

在多个 Z layer 上采样 texture/fill 边界到真实 3D 表面的距离。

验收：

```text
误差范围由 backend resolution 和 quantizationErrorMm 解释；
不能出现与 layer XY 轮廓方向相关的系统性厚度漂移；
report 标记 geometryMode=global_3d_distance。
```

### 6.2 内腔

闭合内腔表面也应生成向模型实体内部延伸的 texture shell。

验收：

```text
surfaceScope=all_closed_surfaces；
outer/inner surface components 均有 texture voxels；
中间 fill 是两侧 shell 的补集；
壳层相遇时 fill 自然消失。
```

### 6.3 薄壁

对局部厚度小于 `2 * widthMm` 的区域：

```text
texture shell union 覆盖整个薄壁；
modelFill=0 in thin region；
thinRegionMergedCount > 0；
不产生重叠计数。
```

## 7. 纹理传递验证

至少覆盖：

```text
OBJ/MTL/PNG bilinear + clamp；
OBJ missing texture；
OBJ missing UV；
3MF Texture2D；
多表面颜色 tie；
全纹理模式内部点最近表面属性。
```

必须检查：

```text
sampledTextureCount；
fallbackCount；
missingUvCount；
missingTextureCount；
uvOutOfRangeCount；
maxTransferDistanceMm；
medialAxisTieCount；
outsideColoredCount = 0。
```

## 8. 与 12D Closure 联合验证

普通 shell 宽度：

```text
TextureSurfaceMask + ModelFillMask = ModelMask；
ColorFillGap = 0；
ModelSupportGap 按 12D 规则判定；
repair disabled 时 TIFF 不因诊断改变。
```

全纹理宽度：

```text
ModelFillMask = Empty；
ColorFillGap = 0 or not_applicable(reason=all_texture_partition)；
unassignedModelPixels = 0；
closure 不能因 modelFillPixels=0 自动失败。
```

## 9. UI 验证

UI smoke 至少覆盖：

```text
1. 选择 global_surface_shell；
2. 模型 preflight 完成后动态设置 min/max；
3. slider/spinbox 双向同步；
4. 0.01 mm 步长；
5. 切换模型后阈值重算和 clamp；
6. 达到最大值时 allTexture 状态和 fill coverage=0；
7. 模型填充材料值仍保留；
8. effective config 包含 surfaceShell 和 complement scope；
9. Preview 显示 Texture Surface/Model Fill/Partition；
10. Diagnostics 显示 backend、resolution、partitionPass；
11. OpenVDB OFF 时不出现生产不可用误导。
```

## 10. Backend Conformance

在 OpenVDB ON lane 可用时，对相同 mesh/grid/width 比较 CPU candidate 与 OpenVDB candidate：

```text
model occupancy count；
texture/fill count；
partition invariants；
boundary distance error；
allTexture threshold；
closest-surface transfer stats；
runtime/peak memory。
```

差异必须输出，不要求位级一致。准入阈值由 12E-R1/R2 在看到实际数据后冻结，本计划不预设虚构数值。

## 11. Production Package 验证

production gate 通过后才运行：

```text
slicer_cli -> p0.rgbwsv.2 package；
rip_reader_test --summary/strict；
texture_fill_partition_report；
slice_report semantic；
material_closure_report；
LayerPreview/OverlayPreview。
```

必须保持：

```text
R G B W S V；
uint8；
black_is_print；
Model > OuterVarnishShell > Support > Empty；
OpenVDB experimental path 不直接写 production TIFF。
```

## 12. 计划命令入口

以下是后续任务拟建立的入口，当前不存在时不得宣称已运行：

```powershell
ctest --test-dir build -C Debug -R "texture_fill_partition|experimental_config" --output-on-failure
powershell -ExecutionPolicy Bypass -File .\scripts\run_12e_texture_fill_partition_tests.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run_12e_texture_fill_real_model_tests.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case texture-fill-partition
```

每个原子任务必须先确认实际 target/script 路径，再把命令写入完成记录。

## 13. 完成判定

```text
1. 所有 partition invariants PASS；
2. width sweep 单调；
3. 动态阈值处 fill=0、texture=model；
4. global 3D fixture 证明不是 per-layer approximation；
5. strict topology blocker 生效；
6. UI/effective config/report 一致；
7. 12D closure 联合验证通过；
8. 默认 OFF build 和现有 Profile 回归通过；
9. OpenVDB ON 结果只按其准入角色解释；
10. 真实模型性能/内存有实际报告；
11. REPORT_12E 记录实际命令、结果和残余风险。
```
