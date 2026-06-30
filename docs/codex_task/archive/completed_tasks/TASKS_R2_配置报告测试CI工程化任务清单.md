# TASKS_R2_配置报告测试CI工程化任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：R2  
> 建议提交目录：`docs/slicer/`

## Milestone R2-0：阅读确认

- [x] 阅读 `REPORT_R1_核心模块边界重构当前状态.md`
- [x] 确认不修改 p0.rgbwsv.2
- [x] 确认不实现 surface_shell_texture / compensated_varnish
- [x] 确认不引入 OpenVDB / 设备通信

## Milestone R2-1：Config Schema Skeleton

- [x] 新增 `config/ConfigSchema.*`
- [x] 新增 `config/ConfigMigration.*`
- [x] 新增 `config/NormalizedConfig.*`
- [x] 支持 schema 字段读取
- [x] 无 schema 时判定为 legacy
- [x] legacy config 可继续运行

## Milestone R2-2：slicer.config.1 样例

- [x] 新增 `samples/configs/schema_v1/slicer_config_v1_basic.json`
- [x] 映射到当前 SliceConfig
- [x] slicer_cli 可运行
- [x] rip_reader_test 可通过

## Milestone R2-3：Report Base Helper

- [x] 新增 `reports/ReportBase.*`
- [x] 新增 `reports/ReportSchemaValidator.*`
- [x] 提供 schema/source/configSnapshot/stats/warnings/errors/timings helper
- [x] 至少一个 report 接入 helper
- [x] 不破坏旧 report 字段

## Milestone R2-4：Diagnostics 小增强

- [x] 扩展 `diagnostics/Diagnostics.*`
- [x] 增加 severity/code/message/source/context
- [x] config/schema/report tests 可使用
- [x] 不强制替换所有 legacy warning

## Milestone R2-5：Schema Tests

- [x] 新增 `scripts/run_schema_tests.ps1`
- [x] 检查 slicer.config.1 样例
- [x] 检查 manifest schema
- [x] 检查 preview_report schema
- [x] 检查 material_process_report 关键字段
- [x] 返回码可用于 CI

## Milestone R2-6：Golden Tests

- [x] 新增 `scripts/run_golden_tests.ps1`
- [x] 新增 expected summary JSON
- [x] 覆盖 P0 basic / material process / ui overlay fixture
- [x] 不保存大体积二进制 golden
- [x] 比较关键摘要字段

## Milestone R2-7：CI Quick Guard

- [x] 新增 `scripts/run_ci_quick.ps1`
- [x] 包含 build / quick regression / schema tests / golden tests
- [x] 包含 UI self-test / overlay-load-real

## Milestone R2-8：状态报告

- [x] 生成 `REPORT_R2_配置报告测试CI工程化当前状态.md`
- [x] 判断是否进入 08
