# PRD_R2_配置报告测试CI工程化固化

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：R2  
> 建议提交目录：`docs/slicer/`

## 1. 产品目标

R2 的目标是把 R1 的模块边界进一步固化为正式工程基础设施：

```text
配置可版本化
报告可验证
诊断可追踪
测试可分层
CI 可守门
旧样例可兼容
```

## 2. 必须支持能力

### 2.1 Config Schema Version

新增正式配置 schema：

```text
schema = slicer.config.1
```

同时保持旧配置兼容。

### 2.2 Config Migration

新增配置迁移层：

```text
legacy config → slicer.config.1 normalized config → legacy SliceConfig DTO
```

### 2.3 Report Schema

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

### 2.4 Diagnostics / Error Code

统一诊断结构：

```text
severity
code
message
source
context
```

### 2.5 Test 分层

整理为：

```text
unit_tests
schema_tests
golden_tests
negative_package_tests
regression_quick
regression_full
regression_heavy
ui_smoke_tests
```

### 2.6 CI Guard

新增统一脚本：

```text
scripts/run_ci_quick.ps1
```

第一版包装：

```text
build
quick regression
schema tests
golden tests
ui self-test
overlay-load-real
```

## 3. 验收标准

```text
legacy configs 仍可运行
至少一个 slicer.config.1 样例可运行
schema test 能验证 config schema
schema test 能验证主要 report schema
golden test 能验证关键 package 输出摘要
run_ci_quick.ps1 可一键执行
quick regression 仍通过
UI self-test 仍通过
overlay-load-real 仍通过
p0.rgbwsv.2 输出协议不变
```
