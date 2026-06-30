# ARCH_03_R0_config_report_test_schema设计

> 文档版本：v0.1  
> 文档状态：Architecture Draft  
> 适用阶段：R0  
> 建议提交目录：`docs/slicer/`

---

## 1. Config Schema

正式配置建议引入：

```text
schema = slicer.config.1
```

推荐顶层：

```json
{
  "schema": "slicer.config.1",
  "input": {},
  "output": {},
  "pipeline": {},
  "geometry": {},
  "texture": {},
  "materials": {},
  "support": {},
  "preview": {},
  "diagnostics": {}
}
```

现有字段通过迁移层兼容。

---

## 2. 新增策略配置

```json
{
  "materials": {
    "textureApplication": {
      "mode": "full_volume",
      "shellThicknessPx": 3,
      "shellThicknessMm": 0.05,
      "fillRole": "base"
    },
    "varnishGeometry": {
      "mode": "additive",
      "thicknessLayers": 2,
      "thicknessMm": 0.02
    }
  }
}
```

---

## 3. Report Schema

统一 report 基础字段：

```json
{
  "schema": "p0.report.xxx.1",
  "source": {},
  "configSnapshot": {},
  "stats": {},
  "warnings": [],
  "errors": [],
  "timings": {}
}
```

重点统一：

```text
input_report
geometry_report
material_report
texture_report
support_report
process_report
diagnostics_report
preview_report
package_report
```

---

## 4. 测试分层

```text
unit_tests
golden_tests
regression_quick
regression_full
regression_heavy
ui_smoke_tests
schema_tests
```

R2 负责把当前脚本测试正式工程化。
