# TASKS_03C_回归与RIP输出收口任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：03C  
> 建议提交目录：`docs/slicer/`

## Milestone 03C-0：阅读确认

- [x] 阅读 `REPORT_03B_TIFF存储模式兼容当前实现状态.md`
- [x] 阅读 `DOC_DECISION_03C_REPORT03B后执行回归与RIP输出收口.md`
- [x] 阅读 `PRD_03C / DEV_03C / DEMO_03C`
- [x] 确认不修改 TIFF storage 语义
- [x] 确认不修改 MaterialPolicy 语义

## Milestone 03C-1：run_regression 分层

- [x] 增加 `-Mode quick`
- [x] 增加 `-Mode full`
- [x] 增加 `-Mode heavy`
- [x] 保留或兼容 `-SkipHeavyRelief`
- [x] 保留或兼容 `-SkipHeavyTexture`
- [x] 分组 cases
- [x] 增加 step 耗时输出

## Milestone 03C-2：rip_reader_test 输出模式

- [x] 增加 `--summary`
- [x] 增加 `--quiet`
- [x] summary 输出 schema / storage / layer count / channel stats
- [x] quiet 输出 PASS/FAIL
- [x] 负向测试仍支持 `--expect-error` / `--expect-code`

## Milestone 03C-3：RIP Compatibility Checklist

- [x] 新增 `docs/slicer/RIP_COMPATIBILITY_CHECKLIST_RGBWSV_TIFF.md`
- [x] 覆盖 schema
- [x] 覆盖 storageMode
- [x] 覆盖 rowsPerStrip / tileSize
- [x] 覆盖 SamplesPerPixel / BitsPerSample / PlanarConfig
- [x] 覆盖 printValue / emptyValue / polarity

## Milestone 03C-4：验证

- [x] `run_regression.ps1 -Mode quick` 通过
- [x] `run_regression.ps1 -Mode full` 通过
- [x] `run_regression.ps1 -Mode heavy` 可独立运行
- [x] `rip_reader_test --summary` 可用
- [x] `rip_reader_test --quiet` 可用

## Milestone 03C-5：状态报告

- [x] 生成 `REPORT_03C_回归与RIP输出收口当前实现状态.md`
- [x] 记录 quick/full/heavy 耗时
- [x] 记录 summary/quiet 输出示例
- [x] 说明是否建议进入 06
