# PRD_06A_3MF兼容性增强与负向测试

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_06 之后  
> 所属模块：Slicer / 3MF Importer / Test  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

06 阶段已经实现：

```text
3MF stored ZIP package 读取
3D/3dmodel.model 解析
mesh / object / component / transform 基础支持
basematerials / displaycolor 基础支持
MaterialRoleMapping 到 RGB/W/V
three_mf_report.json
quick regression 通过
```

但真实 3MF 文件通常会使用 deflate 压缩，且 XML 结构和资源关系更复杂。06 当前仍是基础子集，需要 06A 提升兼容性和负向测试覆盖。

---

## 2. 产品目标

06A 目标：

```text
让 3MF importer 更可靠地处理真实 3MF package，
并能对非法、缺失、危险或不支持的 3MF 输入给出明确错误或 warning。
```

---

## 3. 必须支持

### 3.1 Deflate 3MF package

必须支持：

```text
ZIP compression method = 8 / deflate
```

保留支持：

```text
ZIP compression method = 0 / stored
```

不强制支持：

```text
ZIP64
加密 ZIP
外部关系资源
```

---

### 3.2 受限 XML Parser

当前字符串解析应替换或封装为受限 XML parser。

推荐：

```text
tinyxml2
```

要求：

```text
不解析外部实体
不访问网络
不加载外部 DTD
不执行任何外部资源
```

---

### 3.3 3MF 负向测试

必须新增 bad 3MF packages：

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

### 3.4 Report 增强

`three_mf_report.json` 增强：

```text
zipCompressionStats
entryCount
totalUncompressedBytes
xmlParser
unsupportedResources
unsupportedExtensions
invalidReferenceCount
ignoredResourceCount
warnings
errors
```

---

## 4. 行为策略

### 4.1 不支持但可忽略

例如：

```text
Texture2DGroup
ColorGroup
CompositeMaterials
MultiProperties
Production extension metadata
```

第一版行为：

```text
记录到 unsupportedResources / unsupportedExtensions
使用 fallback material role
继续切片
```

### 4.2 不安全或结构错误

例如：

```text
path traversal
缺少 model part
XML 无法解析
三角面索引越界
component 引用不存在
解压总量超过限制
```

行为：

```text
直接 fail，并输出明确错误码或错误关键词。
```

---

## 5. 配置需求

新增或扩展：

```json
{
  "threeMf": {
    "enabled": true,
    "zipPolicy": {
      "allowDeflate": true,
      "allowStored": true,
      "allowZip64": false,
      "maxEntryCount": 256,
      "maxTotalUncompressedBytes": 134217728
    },
    "xmlPolicy": {
      "parser": "tinyxml2",
      "allowExternalEntities": false
    },
    "unsupportedExtensionPolicy": "warn_and_fallback"
  }
}
```

如果暂不新增完整配置，也必须在 report 中写出默认策略。

---

## 6. 验收标准

1. stored 3MF 仍可读取。
2. deflate 3MF 可读取。
3. missing content types 能报错。
4. missing rels 能报错或 fallback 到标准 model part，并记录 warning。
5. missing model part 能报错。
6. path traversal 被拒绝。
7. too many entries 被拒绝。
8. too large uncompressed 被拒绝。
9. unknown material id 能 warning/fallback 或明确错误。
10. invalid component reference 能明确错误。
11. invalid triangle indices 能明确错误。
12. unsupported extension 能记录 report。
13. `three_mf_report.json` 增强字段存在。
14. `run_regression.ps1 -Mode quick` 通过。
15. `rip_reader_test --summary` 通过。
16. 不改变 p0.rgbwsv.2 输出协议。

---

## 7. 非目标

06A 不做：

```text
完整 3MF Texture2DGroup 采样
完整 ColorGroup 映射
CompositeMaterials / MultiProperties 完整语义
PBR / metallic / roughness
Production Extension 完整支持
Beam lattice / slice extension
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
复杂支撑形态优化
```

---

## 8. 结论

06A 的核心是：

```text
增强 3MF importer 真实文件兼容性，
增加负向测试和安全边界，
不改变后端输出协议。
```
