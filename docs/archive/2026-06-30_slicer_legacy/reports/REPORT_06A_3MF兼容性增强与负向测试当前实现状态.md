# REPORT_06A_3MF兼容性增强与负向测试当前实现状态

> 日期：2026-06-08  
> 阶段：06A / 3MF 兼容性增强与负向测试  
> 状态：已完成实现与 quick regression 验证

---

## 1. 本阶段目标

06A 在 06 的 3MF 基础 importer 上增强真实文件兼容性和输入安全性：

- 支持 ZIP stored entry。
- 支持 ZIP deflate entry。
- 封装受限 XML reader。
- 增加 3MF reference validation。
- 增加 bad 3MF package 负向测试。
- 增强 `three_mf_report.json`。

本阶段保持不变：

- `schema = p0.rgbwsv.2`
- `channelOrder = R G B W S V`
- `bitDepth = 8`
- `polarity = black_is_print`
- `0=打印，255=不打印`
- `MaterialRoleMapping` 语义不变
- `MaterialPolicy` RGB/W/V 语义不变
- S 支撑仍由 Support pipeline 生成

---

## 2. ZIP Deflate 支持

已引入 vendored `miniz`：

```text
src/third_party/miniz/
```

CMake 已更新：

- `project(... LANGUAGES C CXX)`
- `slicer_core` 编译 `miniz.c / miniz_tdef.c / miniz_tinfl.c / miniz_zip.c`
- include `src/third_party/miniz`

支持：

- compression method `0` stored。
- compression method `8` deflate。
- entry count limit：256。
- total uncompressed limit：128 MiB。
- path traversal 拒绝。

依赖选择说明：

- `miniz`：轻量、public domain / MIT 风格许可、可 vendored、无需 vcpkg 和 DLL 部署；适合当前 CLI demo。
- `minizip / libzip`：生态成熟但需要 vcpkg/CMake package 和运行时依赖管理；当前阶段引入成本更高。
- 06A 选择 `miniz`，风险是 vendored 源码需要后续手动维护版本。

---

## 3. XML Reader 状态

当前未引入 `tinyxml2`，而是封装：

```text
ThreeMfXmlReader
```

支持：

- 禁止 `<!DOCTYPE>`。
- 禁止 `<!ENTITY>`。
- 禁止外部 DTD / external entity。
- 不访问网络。
- 基础 tag matching 校验。
- 集中提供 `root_attributes / tags / block / blocks / tags_in / block_in`。

依赖选择说明：

- `tinyxml2`：解析能力更强，适合后续真实 3MF 扩展，但需要 vcpkg 或源码引入。
- 当前受限 reader：依赖少、改动小、可满足 06A bad package 和基础 3MF 结构校验。

限制：

- 当前 reader 仍不是完整 XML parser。
- 复杂 namespace、本地名匹配、XML edge case 仍建议在 06B 或 06A 后续小版本替换为 `tinyxml2`。

---

## 4. Validation 增强

已新增校验：

- model part 存在。
- model XML 可解析。
- unit 必须属于支持集合。
- mesh vertices 非空。
- mesh triangles 非空。
- triangle index 不越界。
- component reference 必须存在。
- build item object reference 必须存在。
- unknown material id 记录 warning 并 fallback。
- unsupported resource / extension 记录 report。

错误关键词：

```text
E_3MF_ZIP_OPEN_FAILED
E_3MF_ZIP_UNSUPPORTED_COMPRESSION
E_3MF_ZIP_TOO_MANY_ENTRIES
E_3MF_ZIP_TOO_LARGE
E_3MF_ZIP_PATH_TRAVERSAL
E_3MF_CONTENT_TYPES_MISSING
E_3MF_RELS_MISSING
E_3MF_MODEL_PART_MISSING
E_3MF_XML_PARSE_FAILED
E_3MF_UNSUPPORTED_UNIT
E_3MF_INVALID_TRIANGLE_INDEX
E_3MF_INVALID_COMPONENT_REFERENCE
E_3MF_UNKNOWN_MATERIAL_ID
```

---

## 5. Report 增强

`reports/three_mf_report.json` 新增：

```text
zipCompressionStats
entryCount
totalUncompressedBytes
zip.entryCount
zip.storedEntryCount
zip.deflatedEntryCount
zip.totalUncompressedBytes
xml.parser
xml.allowExternalEntities
xml.parseWarnings
validation.invalidReferenceCount
validation.unknownMaterialCount
validation.ignoredResourceCount
unsupportedResources
unsupportedExtensions
warnings
errors
```

deflate 样例验证摘录：

```text
ThreeMfSingleRgbDeflate:
zip.deflatedEntryCount = 3
zip.storedEntryCount = 0
xml.parser = restricted_string_xml_reader
validation.invalidReferenceCount = 0
```

stored 样例验证摘录：

```text
ThreeMfSingleRgbStored:
zip.storedEntryCount > 0
```

---

## 6. 样例与脚本

新增正向样例配置：

```text
samples/configs/3mf/three_mf_single_rgb_stored.json
samples/configs/3mf/three_mf_single_rgb_deflate.json
samples/configs/3mf/three_mf_multi_material_deflate.json
```

新增正向样例模型：

```text
samples/models/3mf/single_rgb_cube_stored.3mf
samples/models/3mf/single_rgb_cube_deflate.3mf
samples/models/3mf/multi_material_rgb_white_varnish_deflate.3mf
```

新增脚本：

```text
scripts/make_3mf_samples.ps1
scripts/make_bad_3mf_packages.ps1
scripts/run_3mf_negative_tests.ps1
```

`scripts/run_regression.ps1 -Mode quick` 已接入：

- 06A deflate positive cases。
- 06A stored positive case。
- bad 3MF package generation。
- bad 3MF package negative tests。

---

## 7. Bad 3MF 验证结果

已覆盖：

- `bad_3mf_missing_content_types`：失败，`E_3MF_CONTENT_TYPES_MISSING`。
- `bad_3mf_missing_rels`：成功 fallback，report 记录 `E_3MF_RELS_MISSING` warning。
- `bad_3mf_missing_model_part`：失败，`E_3MF_MODEL_PART_MISSING`。
- `bad_3mf_xml_parse_failed`：失败，`E_3MF_XML_PARSE_FAILED`。
- `bad_3mf_path_traversal`：失败，`E_3MF_ZIP_PATH_TRAVERSAL`。
- `bad_3mf_too_many_entries`：失败，`E_3MF_ZIP_TOO_MANY_ENTRIES`。
- `bad_3mf_too_large_uncompressed`：失败，`E_3MF_ZIP_TOO_LARGE`。
- `bad_3mf_unknown_material_id`：成功 fallback，report 记录 unknown material warning/count。
- `bad_3mf_invalid_component_reference`：失败，`E_3MF_INVALID_COMPONENT_REFERENCE`。
- `bad_3mf_invalid_triangle_indices`：失败，`E_3MF_INVALID_TRIANGLE_INDEX`。
- `bad_3mf_unsupported_unit`：失败，`E_3MF_UNSUPPORTED_UNIT`。

---

## 8. 已运行验证

已运行并通过：

```powershell
cmake --build build --config Debug
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_single_rgb_deflate.json
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_multi_material_deflate.json
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_single_rgb_stored.json
.\build\Debug\rip_reader_test.exe --package output\ThreeMfSingleRgbDeflate --summary
.\build\Debug\rip_reader_test.exe --package output\ThreeMfMultiMaterialDeflate --summary
.\scripts\make_bad_3mf_packages.ps1
.\scripts\run_3mf_negative_tests.ps1
.\scripts\run_regression.ps1 -Mode quick
```

`run_regression.ps1 -Mode quick` 最终结果：

```text
Regression complete. mode=quick
```

---

## 9. 当前未支持范围

06A 仍不支持：

- ZIP64。
- encrypted ZIP。
- external relationship resources。
- 完整 3MF Texture2DGroup 采样。
- 完整 ColorGroup 映射。
- CompositeMaterials / MultiProperties 完整语义。
- PBR / metallic / roughness。
- Production extension 完整语义。
- Beam lattice / slice extension。
- OpenVDB。
- Qt UI。
- RIP 半色调。
- ICC / CMYK。

---

## 10. 下一阶段建议

建议优先进入 `06B` 或 `05A`：

- 如果下一步目标是真实 3MF 彩色/纹理输入，进入 `06B`：Texture2DGroup / ColorGroup / 更完整 XML parser。
- 如果下一步目标是打印工艺效果，进入 `05A`：真实模型材料参数、白墨 underbase、光油 top layer 策略验证。
- 如果下一步目标是工程调试效率，进入 `07`：Qt 调试 UI。

当前建议：

```text
优先 06B 小步增强 XML/tinyxml2 与 3MF Texture2D/ColorGroup 基础支持；
若业务更关注打印效果，则转入 05A。
```
