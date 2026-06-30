# TASKS_06A_3MF兼容性增强任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：PRD_06A / DEV_06A  
> 建议提交目录：`docs/slicer/`

---

## Milestone 06A-0：阅读确认

- [x] 阅读 `REPORT_06_3MF与OBJ_MTL多材料输入当前实现状态.md`
- [x] 阅读 `DOC_DECISION_06A_REPORT06后进入3MF兼容性增强与负向测试.md`
- [x] 阅读 `PRD_06A / DEV_06A / DEMO_06A`
- [x] 确认不改 p0.rgbwsv.2 输出协议
- [x] 确认不改 MaterialRoleMapping 语义
- [x] 确认本阶段不做 OpenVDB / Qt / RIP 半色调

---

## Milestone 06A-1：ZIP Deflate 支持

- [x] 确定 ZIP 解压依赖
- [x] 支持 stored entry
- [x] 支持 deflate entry
- [x] 保留 path traversal 检查
- [x] 保留 max entry count
- [x] 保留 max total uncompressed bytes
- [x] report 记录 compression stats

---

## Milestone 06A-2：XML Parser 封装

- [x] 引入或封装受限 XML parser
- [x] 禁止外部实体
- [x] 禁止外部 DTD
- [x] 禁止网络资源
- [x] 替换散落字符串解析
- [x] report 记录 parser 名称

---

## Milestone 06A-3：3MF Validation

- [x] 校验 model part 存在
- [x] 校验 vertices 非空
- [x] 校验 triangles 非空
- [x] 校验 triangle index 不越界
- [x] 校验 component reference
- [x] 校验 material reference
- [x] 校验 unit
- [x] 记录 unsupported resources/extensions

---

## Milestone 06A-4：Bad 3MF Packages

- [x] bad_3mf_missing_content_types
- [x] bad_3mf_missing_rels
- [x] bad_3mf_missing_model_part
- [x] bad_3mf_xml_parse_failed
- [x] bad_3mf_path_traversal
- [x] bad_3mf_too_many_entries
- [x] bad_3mf_too_large_uncompressed
- [x] bad_3mf_unknown_material_id
- [x] bad_3mf_invalid_component_reference
- [x] bad_3mf_invalid_triangle_indices
- [x] bad_3mf_unsupported_unit

---

## Milestone 06A-5：Report 增强

- [x] three_mf_report 增加 zip section
- [x] three_mf_report 增加 xml section
- [x] three_mf_report 增加 validation section
- [x] three_mf_report 增加 unsupportedResources
- [x] three_mf_report 增加 errors/warnings 统计

---

## Milestone 06A-6：Regression

- [x] quick 增加 deflate 3MF positive
- [x] quick 增加 minimal bad 3MF
- [x] full 增加更多 bad 3MF
- [x] stored 3MF 原样例继续通过
- [x] OBJ/MTL material mapping 继续通过

---

## Milestone 06A-7：状态报告

- [x] 生成 `REPORT_06A_3MF兼容性增强与负向测试当前实现状态.md`
- [x] 记录 deflate 支持状态
- [x] 记录 XML parser 状态
- [x] 记录 bad 3MF 测试结果
- [x] 记录是否建议进入 06B / 05A / 07

