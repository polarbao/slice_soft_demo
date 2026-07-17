# DOC_PREP_12E-R1 Global Partition Service 骨架准备

> 文档状态：IMPLEMENTED / 12E-02 COMPLETE
> 日期：2026-07-17
> 前置任务：12E-01 Config 与 DTO 契约 COMPLETE
> 覆盖任务：12E-02 Global Partition Service 骨架

## 1. 准备结论

12E-01 已建立配置与安全门禁；12E-02 已按本准备文档实现 backend-neutral request/candidate/result、可注入 backend service、3D mask 和统一不变量校验。

12E-02 仍属于 backend-neutral diagnostic contract。它没有实现 mesh occupancy、三维距离、最近表面查询、OpenVDB adapter、composer、TIFF writer 或 Qt UI。

## 2. 实施前代码事实与当前结果

```text
TextureSurfaceShellConfig 已可解析并静态校验；
GlobalTextureFillPartitionOptions 已存在；
TextureFillPartitionReportData 只表达 unavailable/blocked；
传统和 OpenVDB 候选入口均在模型加载及写包前阻断 global_surface_shell；
实施前没有 Mask3D/GridSpec3D/GlobalTextureFillPartitionResult；
实施前没有 GlobalTextureFillPartitionService；
实施前没有 3D partition invariant validator；
实施前没有 texture_fill_partition_service_unit_tests target；
当前上述 DTO、service、validator 和独立测试 target 均已实现。
```

因此 12E-02 只能建立服务和不变量骨架，不能移除 `E_12E_PARTITION_BACKEND_UNAVAILABLE` 生产门禁。

## 3. 建议 DTO

新增 backend-neutral 类型：

```text
TextureFillPartitionGridSpec
  width/height/depth
  originX/Y/ZMm
  spacingX/Y/ZMm

TextureFillPartitionMask3D
  grid
  values: vector<uint8_t>, 只接受 0/1

TextureFillPartitionStats
  modelVoxels
  textureSurfaceVoxels
  modelFillVoxels
  overlapTextureFillVoxels
  unassignedModelVoxels
  textureOutsideModelVoxels
  modelFillOutsideModelVoxels

GlobalTextureFillPartitionResult
  availability/status/productionAcceptance
  backend/backendRole
  options
  model/textureSurface/modelFill
  stats
  partitionPass
  issues
```

Public DTO 只使用 STL 和项目类型，不得暴露 Qt、OpenVDB 或生产 TIFF 类型。

## 4. Service 边界

建议新增：

```text
IGlobalTextureFillPartitionBackend
  Evaluate(request) -> candidate result

GlobalTextureFillPartitionService
  backend 可为空；
  backend 为空时返回 unavailable，不抛出虚假 pass；
  backend 返回 candidate 后统一执行尺寸、二值性和分区不变量检查；
  只产生 diagnostic result，不写 package。
```

12E-02 可通过测试 fake backend 注入 generated masks；正式 CPU/OpenVDB backend 分别留给 12E-03/04。

## 5. 固定不变量

对每个体素：

```text
Model=0 -> TextureSurface=0 AND ModelFill=0；
Model=1 -> TextureSurface XOR ModelFill；
TextureSurface ∩ ModelFill = Empty；
TextureSurface ∪ ModelFill = Model；
全部 mask grid 完全一致；
全部 mask 值只能为 0 或 1。
```

统计必须由统一 validator 计算，不能信任 backend 自报计数。

## 6. 状态与错误

允许状态：

```text
availability = available | unavailable；
status = unavailable | blocked | diagnostic | pass | fail；
productionAcceptance = not_evaluated；
backendRole = production_candidate | conformance_candidate | unavailable。
```

12E-02 不允许 `productionAcceptance=passed`。建议新增稳定 issue code：

```text
E_12E_PARTITION_GRID_INVALID
E_12E_PARTITION_MASK_SIZE_MISMATCH
E_12E_PARTITION_MASK_NON_BINARY
E_12E_TEXTURE_OUTSIDE_MODEL
E_12E_MODEL_FILL_OUTSIDE_MODEL
E_12E_TEXTURE_FILL_OVERLAP
E_12E_MODEL_VOXEL_UNASSIGNED
E_12E_PARTITION_BACKEND_UNAVAILABLE
E_12E_PARTITION_BACKEND_FAILED
```

## 7. 文件范围

允许修改：

```text
src/slicer_core/materials/texture_application/TextureFillPartitionTypes.*
src/slicer_core/materials/texture_application/GlobalTextureFillPartitionService.*
src/slicer_core/reports/TextureFillPartitionReport.*
tests/unit/texture_fill_partition_service/main.cpp
CMakeLists.txt
docs/slice 的 12E 状态、schema、matrix 和任务文档
```

禁止修改：

```text
config 语义和 12E-01 稳定错误；
slicer.cpp/composer/TIFF writer；
OpenVDB adapter/default；
Qt UI；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
12D repair。
```

## 8. 最小测试矩阵

| Case | 期望 |
|---|---|
| null backend | unavailable/blocked/not_evaluated |
| backend throws | blocked/not_evaluated + stable backend failure |
| exact 1-voxel model -> texture | pass invariant，fill=0 |
| exact 1-voxel model -> fill | pass invariant，texture=0 |
| mixed generated partition | union=model、overlap=0、unassigned=0 |
| grid dimension invalid | stable grid error |
| mask size mismatch | stable size error |
| requested grid mismatch | stable size error |
| non-binary value | stable binary error |
| texture outside model | fail + stable issue |
| fill outside model | fail + stable issue |
| overlap | fail + overlap count |
| unassigned model | fail + unassigned count |
| repeat evaluation | deterministic counts/issues |

测试中的 pass 只表示 partition invariant diagnostic pass，不表示 production admission。

## 9. 验证计划

```powershell
cmake --build build --config Debug --target texture_fill_partition_service_unit_tests
.\build\Debug\texture_fill_partition_service_unit_tests.exe
ctest --test-dir build -C Debug -R "texture_fill_partition_service|experimental_config" --output-on-failure
git diff --check
```

12E-02 完成后才可准备 12E-03 Legacy CPU whole-model 3D distance candidate。12E-02 不自动进入 12E-03。

## 10. 准入结论

```text
12E-R0：COMPLETE；
12E-01：COMPLETE；
12E-02：COMPLETE；
12E-03：PREPARED / READY FOR USER ADMISSION；
12E production：NOT ADMITTED。
```
