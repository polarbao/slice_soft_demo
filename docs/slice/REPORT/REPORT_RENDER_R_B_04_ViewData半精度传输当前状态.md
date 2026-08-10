# REPORT R-B-04 ViewData 半精度传输当前状态

> 状态：**COMPLETE / VALIDATED**
> 日期：2026-08-10
> 合同：`slicer_capability_dtos` v1.10

## 1. 当前成果

- ViewData 查询新增可选 `meshAttributeFormat=float32|float16`；缺省保持 float32；
- float16 路径用 meshoptimizer 量化 position、normal、texcoord0；
- wire 预算使用 2 B 标量，mesh identity 区分两种编码；
- 参考宿主显式请求 float16，同时保留 float32 解码兼容；
- descriptor 长度、格式和非有限值均 fail-closed；
- SPI v1、11 导出、15 能力与生产 RGBWSV 包未变化。

## 2. 验收口径

| 项目 | 目标 | 当前状态 |
|---|---|---|
| half 位模式 | 0、1、-2、65504 与 IEEE binary16 一致 | PASS |
| mesh blob 缩减 | 真实纹理 fixture ≥ 40% | PASS：304 B → 176 B，缩减 42.1% |
| 预算阈值 | 25k 三角进入 32 MiB / 22 单实例预算 | PASS |
| 兼容 | 字段缺省仍返回 float32 | PASS |
| 渲染 | float16 解码后真实纹理与相机零 DLL 调用通过 | PASS |
| Debug/Release | 合同、provider、three_d、source-size 门禁通过 | PASS：各 7/7 |

## 3. 实际验证

```powershell
cmake --build build-slicesoft/main --config Debug --target `
  textured_scene_viewdata_14b03a_unit_tests `
  textured_scene_viewdata_14b03a_real_fixture_tests `
  stage14e04c_three_d_tests hostflow_hd02_three_d_canvas_tests
ctest --test-dir build-slicesoft/main -C Debug `
  -R "^(slicer_capability_dto_contract_test|slicer_stage14b_target_graph_test|slicer_source_size_guard_self_test|textured_scene_viewdata_14b03a_unit_tests|textured_scene_viewdata_14b03a_real_fixture_tests|slicer_stage14e04c_three_d_contract_test|hostflow_hd02_three_d_canvas)$" `
  --output-on-failure
```

Debug 与 Release 同一组门禁均为 `7/7 PASS`；新增 decoder 被全部五个
`SceneRenderPolicy` 目标编译覆盖，双视图切换与 H-D-04 场景刷新相邻回归在两种配置均为
`2/2 PASS`。Release three_d 实测输出：

```text
14E-04c three_d contract: PASS
meshUploads=1 textureUploads=1 blobReads=2
floatBytes=304 halfBytes=176 cameraCalls=0
```

第三方 NOTICE 合同在 Debug/Release 均通过。Release 能力包重新打包并由纯 C 宿主验证，输出
`STAGE14F01_PACKAGE_VALIDATION_PASS`；SPI v1、11 导出、15 能力与 RGBWSV 生产闭环未回归。

## 4. 边界

本任务只压缩 ViewData 网格属性的 DLL wire 表达。core 内存仍保留 float32，不宣称降低几何构建
峰值；未引入索引压缩、屏幕空间 LOD、Qt 6 后端、TIFF 变更或 OpenVDB 变更。
