# DEV_R2_配置报告测试CI工程化设计

> 文档版本：v0.1  
> 文档状态：DEV / 工程化设计  
> 适用阶段：R2  
> 建议提交目录：`docs/slicer/`

## 1. 技术目标

在 R1 模块边界基础上新增：

```text
config schema / migration
report schema helper
diagnostics model
schema tests
golden tests
CI quick guard
```

## 2. Config Schema 与 Migration

建议新增：

```text
src/slicer_core/config/ConfigSchema.*
src/slicer_core/config/ConfigMigration.*
src/slicer_core/config/NormalizedConfig.*
```

第一版设计：

```text
检查 schema 字段
无 schema 时视为 legacy config
legacy config 迁移为 NormalizedConfig
NormalizedConfig 再转为现有 SliceConfig
保留旧 load_slice_config()
```

不要一次性删除 legacy parser。

## 3. Config Schema 目标结构

```json
{
  "schema": "slicer.config.1",
  "input": {},
  "output": {},
  "pipeline": {},
  "geometry": {},
  "texture": {},
  "materials": {
    "roleMapping": {},
    "materialPolicy": {},
    "materialProcessProfile": {},
    "textureApplication": {},
    "varnishGeometry": {}
  },
  "support": {},
  "preview": {},
  "diagnostics": {}
}
```

## 4. Report Schema Helper

建议新增：

```text
src/slicer_core/reports/ReportBase.*
src/slicer_core/reports/ReportSchemaValidator.*
```

第一版提供：

```text
MakeReportBase(schema, source, configSnapshot)
AppendWarning
AppendError
AppendTiming
```

R2 不要求一次性重写所有 report，但新 report 应使用 helper。

## 5. Diagnostics Model

扩展 R1 `diagnostics/Diagnostics.*`：

```cpp
enum class DiagnosticSeverity { Info, Warning, Error, Fatal };

struct Diagnostic {
    DiagnosticSeverity severity;
    std::string code;
    std::string message;
    std::string source;
    Json context;
};
```

## 6. Schema Tests

优先用脚本，不引入新依赖：

```text
scripts/run_schema_tests.ps1
```

检查：

```text
slicer.config.1 样例
manifest schema
preview_report schema
material_process_report 关键字段
```

## 7. Golden Tests

新增：

```text
scripts/run_golden_tests.ps1
tests/golden/expected/*.json
```

第一版只保存摘要 JSON，不保存大体积二进制 package。

## 8. CI Quick Guard

新增：

```text
scripts/run_ci_quick.ps1
```

执行 build、quick regression、schema tests、golden tests、UI self-test、overlay-load-real。
