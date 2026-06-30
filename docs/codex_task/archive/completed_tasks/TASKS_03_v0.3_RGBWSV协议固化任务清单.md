# TASKS_03_v0.3_RGBWSV协议固化任务清单

> 文档版本：v0.3  
> 文档状态：Draft / Codex 执行清单  
> 建议提交目录：`docs/slicer/`

---

## Milestone 03-0：阅读确认

- [ ] 阅读 PRD_03 v0.3
- [ ] 阅读 DEV_03 v0.3
- [ ] 确认不改变协议
- [ ] 确认只做固化与测试

---

## Milestone 03-1：Manifest Schema

- [ ] 固化 `schema = p0.rgbwsv.1`
- [ ] manifest 写入 tiff protocol fields
- [ ] manifest 写入 grid fields
- [ ] manifest 写入 slicing fields
- [ ] manifest 写入 layer list

---

## Milestone 03-2：Reader 校验增强

- [ ] 校验 schema
- [ ] 校验 channelOrder
- [ ] 校验 bitDepth
- [ ] 校验 polarity
- [ ] 校验 printValue / emptyValue
- [ ] 校验 layer count
- [ ] 校验 layer size
- [ ] 校验 samples per pixel
- [ ] 校验 planar config

---

## Milestone 03-3：错误码

- [ ] 定义 ValidationErrorCode
- [ ] 错误信息包含 code
- [ ] 错误信息包含 field path
- [ ] 错误信息包含 expected / actual

---

## Milestone 03-4：负向测试包

- [ ] bad_missing_manifest
- [ ] bad_schema
- [ ] bad_bit_depth
- [ ] bad_channel_order
- [ ] bad_polarity
- [ ] bad_print_value
- [ ] bad_empty_value
- [ ] bad_missing_layer
- [ ] bad_layer_size
- [ ] bad_samples_per_pixel
- [ ] bad_planar_config

---

## Milestone 03-5：统计字段规范

- [ ] 新增 printPixels
- [ ] 新增 fullPrintPixels
- [ ] 新增 partialPrintPixels
- [ ] 新增 emptyPixels
- [ ] 保留 nonZeroPixels 兼容
- [ ] 文档说明弃用 nonZeroPixels

---

## Milestone 03-6：回归脚本

- [ ] 新增 scripts/run_regression.ps1
- [ ] 正向 package 回归
- [ ] Relief package 回归
- [ ] Support package 回归
- [ ] Bad package 回归

---

## Milestone 03-7：状态报告

- [ ] 生成 REPORT_03_RGBWSV协议固化当前实现状态.md
- [ ] 记录协议字段
- [ ] 记录负向测试结果
- [ ] 记录仍未实现的 RIP 功能
