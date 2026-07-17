# DOC_PREP_12E-R3 Texture Transfer 与 Diagnostic Composer 准备

> 文档状态：IMPLEMENTED / 12E-06 COMPLETE
> 日期：2026-07-17
> 前置任务：12E-01 至 12E-05 COMPLETE
> 覆盖任务：12E-06 Texture Transfer 与 Diagnostic Composer

## 1. 准备结论

12E-05 已冻结 `slicesoft.texture_fill_partition.12e.1` 成功报告、真实 Z 层 voxel
统计、代表性 Width Sweep、0.01 mm 量化、单调性守门和 all-texture endpoint。
12E-06 已复用 12E 分区结果中的 `TextureSurfaceMask3D` 与
`closestSurfaceReferences`，执行一次后端无关的 OBJ/3MF 属性传递，并把 exact
texture/fill mask 转换为内存诊断 composer 输入。

本任务仍是 diagnostic-only：不得写生产 TIFF、不得修改 manifest、不得接入 Qt、不得
改变 legacy 或 OpenVDB candidate production pipeline，也不得授予 OpenVDB production role。

## 2. 当前代码事实

已具备：

```text
GlobalTextureFillPartitionResult：同 grid 的 model/texture/fill masks；
TextureFillClosestSurfaceReference：triangleIndex、barycentric、distanceMm；
NearestTriangleQuery：distance -> barycentric interior margin -> triangle index 稳定 tie-break；
AdaptedTriangleMesh：accepted triangle 与 triangle_attributes 一一对应；
SurfaceAttributeMap：triangle -> UV/material，material -> diffuse/texture；
texture_image：nearest/bilinear、clamp/repeat、flipV；
SurfaceTextureTransfer：已有 OpenVDB shell 原型、纹理缓存和 fallback 统计；
MaterialChannelComposer：已有 RGB/W/S/V 内存合成与模型优先级；
12E report：textureTransfer 段当前明确为 not_evaluated；
12D exact semantic mask contract：已完成，可在 12E-07 联动。
```

明确缺口：

```text
现有 SurfaceTextureTransfer 绑定 OpenVdbLevelSet/OpenVdbSurfaceShellResult；
现有 transfer 会再次执行 NearestTriangleQuery，不能复用 12E closest reference；
现有 OpenVdbCandidateLayerBufferBuilder 使用 shell/interior 旧 DTO，不接受 12E exact masks；
没有 backend-neutral texture transfer DTO；
没有 texture mask 外 RGB 必须 empty 的服务不变量；
没有 modelFill.material -> W/V/RGB 的 12E diagnostic composer 映射；
没有把 transfer stats 写入 12E report；
closest reference 尚未输出 medial-axis tie 计数。
```

## 3. 冻结输入契约

建议新增：

```cpp
struct TextureFillPartitionTextureTransferRequest
{
    const AdaptedTriangleMesh* adaptedMesh;
    const GlobalTextureFillPartitionResult* partition;
    TextureSampleOptions textureSample;
    std::array<std::uint8_t, 3> fallbackRgb;
    std::string missingTexturePolicy;
};
```

硬约束：

```text
partition.available=true 且 partitionPass=true；
partition masks、closest references 与同一 grid voxelCount 对齐；
adaptedMesh.mesh.triangles.size == triangle_attributes.size；
每个 TextureSurfaceMask=1 的 voxel 必须有 valid closest reference；
reference.triangleIndex 必须落在 accepted triangle/attribute 范围；
ModelFillMask 和 model 外 voxel 不允许执行纹理采样；
不重新执行 nearest-surface 查询。
```

OBJ 与 3MF 使用同一 `AdaptedTriangleMesh` 契约，不在 transfer service 内按文件格式分支。

## 4. 冻结输出契约

建议新增：

```text
TextureFillPartitionTextureTransferResult：
  available/status/productionAcceptance；
  voxelRgb：长度等于 grid voxelCount，非 texture voxel 固定 [255,255,255]；
  colorSources：texture/material_diffuse/fallback/not_colored；
  sampledTextureCount/materialDiffuseCount/fallbackCount；
  missingUvCount/missingTextureCount/uvOutOfRangeCount；
  outsideColoredCount/maxTransferDistanceMm/medialAxisTieCount；
  loadedTextureCount/cacheHits/cacheMisses/cacheBytes；
  transferMs/issues。
```

不变量：

```text
outsideColoredCount = 0；
colored voxel count = TextureSurfaceMask voxel count；
ModelFillMask voxel 的 transfer RGB 必须保持 empty；
texture + materialDiffuse + fallback = textureSurfaceVoxels；
所有计数使用 voxel，不伪装为 production pixel；
productionAcceptance 固定 not_evaluated。
```

## 5. 属性选择与 fallback

每个 texture voxel：

```text
1. 读取 closest reference；
2. 通过 triangleIndex 读取 SurfaceTriangleAttributes；
3. 有 UV 且 material texture 可加载：按 sampler/address/flipV 采样；
4. 无 UV：missingUvCount++，按 policy 处理；
5. 纹理缺失或加载失败：missingTextureCount++，按 policy 处理；
6. 无可用纹理但有 diffuse：使用 material diffuse；
7. 否则使用 fallbackRgb。
```

`missingTexturePolicy`：

```text
warn_and_fallback：完成 diagnostic 结果并输出 warning issue；
fail_fast：结果 blocked，不生成伪完整 transfer；
```

fallback 只属于 `TextureSurfaceMask`。不得把 fallback 写入 `ModelFillMask`，也不得以 fallback
替代 white/varnish 模型填充材料。

## 6. Medial-axis tie 规则

当前稳定选择顺序冻结为：

```text
distance；
barycentric interior margin（更靠近三角形内部优先）；
accepted triangle index（较小者优先）。
```

12E-06 允许扩展 `NearestTriangleQueryStats` 或 closest reference 证据以记录 tie 次数，但不能
改变已验证的最终 triangle 选择。CPU/OpenVDB 对同一 closest reference 的纹理结果必须一致。

## 7. Diagnostic Composer

建议新增 backend-neutral 层构建器：

```text
TextureFillPartitionDiagnosticComposer
  input：partition + texture transfer + modelFill material/value；
  output：按真实 layerIndex 的内存 RGBWSV buffer、TextureSurface/ModelFill semantic masks、stats；
```

映射：

```text
TextureSurfaceMask -> RGB = transferred color；
ModelFillMask + material=white -> W = configured print value；
ModelFillMask + material=varnish -> V = configured print value；
ModelFillMask + material=rgb -> RGB = explicit model/fallback color；
非模型 -> 全通道 255；
S 通道在 12E-06 保持 255，不提前接入支撑；
TextureSurface XOR ModelFill 必须成立；
```

composer 只返回内存和诊断报告，不调用 TIFF writer、ReportWriter 或 package publish。12E-07
再将 exact semantic masks 交给 12D closure；12E-08 才可能讨论 production admission。

## 8. Report 接入

扩展 `BuildTextureFillPartitionReport`，允许传入 transfer/diagnostic composer 可选证据：

```text
未执行：textureTransfer.status=not_evaluated，测量值 null；
成功：status=diagnostic，输出真实 voxel 计数和 transferMs；
blocked：保留稳定 issue，不将 0 冒充成功；
outsideColoredCount 必须为 0；
performance.textureTransferMs 只计属性传递，不含 JSON/TIFF/PNG I/O；
```

## 9. 稳定错误码

建议冻结：

```text
E_12E_TEXTURE_TRANSFER_INPUT_INVALID
E_12E_TEXTURE_REFERENCE_MISSING
E_12E_TEXTURE_TRIANGLE_OUT_OF_RANGE
E_12E_TEXTURE_MISSING_UV
E_12E_TEXTURE_MISSING_RESOURCE
E_12E_TEXTURE_SAMPLE_FAILED
E_12E_TEXTURE_OUTSIDE_MODEL
E_12E_DIAGNOSTIC_COMPOSER_INPUT_INVALID
E_12E_DIAGNOSTIC_COMPOSER_PARTITION_INVALID
```

missing UV/resource 在 `warn_and_fallback` 下是 warning，在 `fail_fast` 下升级为 error/blocker。

## 10. Fixture 与验收矩阵

| Fixture | 必须断言 |
|---|---|
| OBJ + MTL + PNG | sampledTextureCount > 0；outsideColored=0 |
| 3MF Texture2D | 同一 transfer service；missingTexture=0 |
| 3MF ColorGroup/base material | diffuse source 可用；不误报 texture missing |
| missing UV | warn/fallback 与 fail-fast 两种稳定结果 |
| missing texture | fallbackCount、missingTextureCount、stable issue |
| UV clamp/repeat | uvOutOfRangeCount 与颜色确定 |
| tie fixture | 重复运行 triangle/color/summary 一致 |
| allTexture | fill=0，全部模型 voxel 有颜色来源 |
| intermediate width | texture/fill 精确互补，fill voxel 不着色 |
| invalid reference | blocked，不访问越界 triangle |
| CPU/OpenVDB same reference | RGB 与 transfer stats 一致 |
| diagnostic composer white/varnish/rgb | 通道与 modelFill.material 一致，S 保持 empty |

优先复用：

```text
model/obj 标准真实模型；
samples/models/textured 的 missing UV/resource fixture；
samples/models/3mf 的 Texture2D/ColorGroup fixture；
generated tie/invalid-reference 小模型。
```

真实大模型只做后续 smoke；单测不得依赖 `output/ui_sessions` 或用户机器绝对路径。

## 11. 文件边界

允许新增或修改：

```text
src/slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.*；
src/slicer_core/materials/texture_application/TextureFillPartitionTypes.*；
src/slicer_core/materials/texture_application/SurfaceAttributeMap.*；
src/slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.*；
src/slicer_core/reports/TextureFillPartitionReport.*；
tests/unit/texture_fill_partition_texture_transfer/*；
tests/unit/texture_fill_partition_diagnostic_composer/*；
tests/golden/expected/12e_texture_transfer_*；
CMakeLists.txt；12E 文档。
```

禁止修改：

```text
slicer.cpp production generation；
TIFF writer / manifest / RIP Reader；
Qt UI；
12D repair 规则；
support/outer varnish production composition；
OpenVDB 默认值、vcpkg 根与依赖策略；
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print。
```

## 12. 验证命令

默认 OFF：

```powershell
cmake --build build --config Debug --target texture_fill_partition_texture_transfer_unit_tests texture_fill_partition_diagnostic_composer_unit_tests texture_fill_partition_report_unit_tests
.\build\Debug\texture_fill_partition_texture_transfer_unit_tests.exe
.\build\Debug\texture_fill_partition_diagnostic_composer_unit_tests.exe
ctest --test-dir build -C Debug -R "texture_fill_partition_(texture_transfer|diagnostic_composer|report)|legacy_cpu_global_distance" --output-on-failure
```

OpenVDB ON：

```powershell
cmake --build build-openvdb-09p --config Debug --target texture_fill_partition_texture_transfer_unit_tests texture_fill_partition_diagnostic_composer_unit_tests
ctest --test-dir build-openvdb-09p -C Debug -R "texture_fill_partition_(texture_transfer|diagnostic_composer)|openvdb_texture_fill_conformance" --output-on-failure
```

共同守门：

```powershell
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

## 13. Gate 结论

```text
12E-01..06：COMPLETE；
12E-07：PREPARED / READY FOR USER ADMISSION；
12E production：NOT ADMITTED。
```

12E-06 完成不代表 diagnostic RGBWSV 内存 buffer 可以写入生产 package，也不自动执行
12E-07。12E-07 准备入口为
`DOC_PREP_12E_R3_12DClosure联动准备.md`。

## 14. 实际实现与验证

实现：

```text
TextureFillPartitionTextureTransfer：统一 OBJ/3MF 属性传递；
closest reference 复用、确定性 tie、纹理缓存和 fallback 统计；
TextureFillPartitionDiagnosticComposer：真实 Z 层 exact masks 与内存 RGBWSV；
white/varnish/rgb fill 分别写 W/V/RGB，S 保持 255；
TextureFillPartitionReport：序列化 transfer/composer/timing/issues；
12 个 transfer、6 个 composer、5 个 report 单元用例与一个 golden。
```

实际守门：

```text
默认 OFF 定向 CTest：4/4 PASS；
OpenVDB ON 定向 CTest：4/4 PASS；
OpenVDB ON target build：PASS；
默认 OFF 全量 build：PASS；全量 CTest：16/16 PASS；
production TIFF/manifest/package：未写入。
```
