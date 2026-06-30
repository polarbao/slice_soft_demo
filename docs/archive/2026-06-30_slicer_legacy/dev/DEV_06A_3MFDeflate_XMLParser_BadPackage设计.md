# DEV_06A_3MFDeflate_XMLParser_BadPackage设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：PRD_06A  
> 所属模块：3MF Importer / ZIP / XML / Regression  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

在 06 当前 3MF 基础 importer 上增强：

```text
ZIP deflate 读取
受限 XML parser
bad 3MF package
three_mf_report 增强
安全限制
```

不改变：

```text
MaterialRoleMapping
MaterialPolicy
Support
p0.rgbwsv.2 writer/reader
```

---

## 2. ZIP 读取设计

### 2.1 当前问题

06 当前只支持：

```text
compression method = 0 stored
```

真实 3MF 通常使用：

```text
compression method = 8 deflate
```

### 2.2 推荐实现

推荐使用：

```text
miniz
```

或 vcpkg 依赖：

```text
minizip / libzip
```

第一版优先推荐：

```text
miniz
```

原因：

```text
轻量
容易集成
能处理 deflate
适合当前 CLI/core 项目
```

### 2.3 安全限制

必须保留：

```text
maxEntryCount
maxTotalUncompressedBytes
path traversal check
no external resources by default
```

---

## 3. XML Parser 设计

### 3.1 当前问题

06 当前 XML 属于轻量字符串解析，适合样例，不适合真实 3MF。

### 3.2 推荐实现

推荐引入：

```text
tinyxml2
```

CMake：

```cmake
find_package(tinyxml2 CONFIG REQUIRED)
target_link_libraries(slicer_core PRIVATE tinyxml2::tinyxml2)
```

如果不引入 tinyxml2，也必须封装一个 `ThreeMfXmlReader`，避免 XML 字符串扫描逻辑散落。

---

## 4. 模块边界

建议拆出：

```text
src/slicer_core/three_mf/
  three_mf_package.*
  three_mf_xml_reader.*
  three_mf_importer.*
  three_mf_validation.*
  three_mf_report.*
```

如果本阶段不大重构，也至少保持函数边界：

```text
read_3mf_zip_entries
parse_3mf_model_xml
validate_3mf_references
collect_unsupported_3mf_resources
write_three_mf_report
```

---

## 5. 错误码 / 错误关键词

建议新增：

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

## 6. 3MF Validation

必须验证：

```text
vertices 非空
triangles 非空
triangle index 不越界
component reference 存在
build item reference 存在
material id / pid / pindex 可解析或 fallback
unit 可识别
```

---

## 7. Bad Package 生成

建议新增：

```text
tests/packages/bad_3mf/
```

或脚本：

```text
scripts/make_bad_3mf_packages.ps1
```

生成：

```text
bad_3mf_missing_content_types
bad_3mf_missing_rels
bad_3mf_missing_model_part
bad_3mf_xml_parse_failed
bad_3mf_path_traversal
bad_3mf_too_many_entries
bad_3mf_too_large_uncompressed
bad_3mf_unknown_material_id
bad_3mf_invalid_component_reference
bad_3mf_invalid_triangle_indices
bad_3mf_unsupported_unit
```

---

## 8. Report 增强

`three_mf_report.json` 新增：

```json
{
  "zip": {
    "entryCount": 0,
    "storedEntryCount": 0,
    "deflatedEntryCount": 0,
    "totalUncompressedBytes": 0
  },
  "xml": {
    "parser": "tinyxml2",
    "parseWarnings": []
  },
  "validation": {
    "invalidReferenceCount": 0,
    "unknownMaterialCount": 0,
    "ignoredResourceCount": 0
  },
  "unsupportedResources": [],
  "unsupportedExtensions": [],
  "warnings": []
}
```

---

## 9. Regression

`run_regression.ps1 -Mode quick` 增加：

```text
3MF stored positive
3MF deflate positive
bad_3mf minimal negative cases
```

`-Mode full` 增加更多 bad 3MF。

---

## 10. 实施顺序

```text
1. 确定 ZIP 解压依赖；
2. 支持 deflate 3MF；
3. 封装 ThreeMfPackageReader；
4. 引入/封装 XML parser；
5. 增加 validation；
6. 增强 three_mf_report；
7. 增加 bad 3MF packages；
8. 更新 run_regression；
9. 生成 REPORT_06A。
```

---

## 11. 非目标

不做：

```text
3MF Texture2DGroup 正式采样
PBR
OpenVDB
Qt UI
RIP 半色调
MaterialPolicy 修改
```

---

## 12. 结论

DEV_06A 的重点是：

```text
把 3MF importer 从样例可用提升到真实输入更可靠。
```
