# DEMO_06A_3MF兼容性与负向测试验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：PRD_06A / DEV_06A  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

验证 06A 的 3MF 兼容性增强：

```text
stored 3MF 仍通过
deflate 3MF 通过
bad 3MF package 能按预期失败
three_mf_report 增强字段存在
quick regression 通过
```

---

## 2. 正向样例

新增或确认：

```text
samples/models/3mf/single_rgb_cube_stored.3mf
samples/models/3mf/single_rgb_cube_deflate.3mf
samples/models/3mf/multi_material_rgb_white_varnish_deflate.3mf
```

配置：

```text
samples/configs/3mf/three_mf_single_rgb_stored.json
samples/configs/3mf/three_mf_single_rgb_deflate.json
samples/configs/3mf/three_mf_multi_material_deflate.json
```

---

## 3. 负向样例

新增：

```text
tests/packages/bad_3mf/bad_3mf_missing_content_types
tests/packages/bad_3mf/bad_3mf_missing_rels
tests/packages/bad_3mf/bad_3mf_missing_model_part
tests/packages/bad_3mf/bad_3mf_xml_parse_failed
tests/packages/bad_3mf/bad_3mf_path_traversal
tests/packages/bad_3mf/bad_3mf_too_many_entries
tests/packages/bad_3mf/bad_3mf_too_large_uncompressed
tests/packages/bad_3mf/bad_3mf_unknown_material_id
tests/packages/bad_3mf/bad_3mf_invalid_component_reference
tests/packages/bad_3mf/bad_3mf_invalid_triangle_indices
tests/packages/bad_3mf/bad_3mf_unsupported_unit
```

---

## 4. 验证命令

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_single_rgb_deflate.json
build\Debug\rip_reader_test.exe --package output\ThreeMfSingleRgbDeflate --summary

build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_multi_material_deflate.json
build\Debug\rip_reader_test.exe --package output\ThreeMfMultiMaterialDeflate --summary

.\scripts\run_regression.ps1 -Mode quick
```

负向：

```powershell
build\Debug\slicer_cli.exe --config tests\packages\bad_3mf\bad_3mf_missing_model_part\config.json
```

或通过专门测试脚本：

```powershell
.\scripts\run_3mf_negative_tests.ps1
```

---

## 5. 验收 Checklist

- [ ] stored 3MF positive case pass。
- [ ] deflate 3MF positive case pass。
- [ ] multi material deflate 3MF pass。
- [ ] missing model part 能失败。
- [ ] XML parse failed 能失败。
- [ ] path traversal 能失败。
- [ ] invalid triangle index 能失败。
- [ ] invalid component reference 能失败。
- [ ] unknown material id 能 warning/fallback 或明确失败。
- [ ] unsupported extension 能记录 report。
- [ ] `three_mf_report.json` 有 zip/xml/validation 增强字段。
- [ ] `run_regression.ps1 -Mode quick` 通过。
- [ ] p0.rgbwsv.2 输出协议不变。

---

## 6. 非目标

不验证：

```text
完整 3MF Texture2DGroup
PBR
Production Extension 完整语义
Beam Lattice
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
```

---

## 7. 状态报告

完成后生成：

```text
docs/slicer/REPORT_06A_3MF兼容性增强与负向测试当前实现状态.md
```
