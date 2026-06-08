# TASKS_03_v0.4_RGBWSV协议固化任务清单

> 文档版本：v0.4  
> 文档状态：Codex Task List  
> 适用阶段：PRD_03  
> 建议提交目录：`docs/slicer/`

---

## Milestone 03-0：阅读确认

- [x] 阅读 `REPORT_02_支撑与孤岛检测当前实现状态.md`
- [x] 阅读 `DOC_DECISION_03_REPORT02后进入协议固化阶段.md`
- [x] 阅读 `PRD_03_v0.4_RGBWSV协议固化与负向测试.md`
- [x] 阅读 `DEV_03_v0.4_TIFFWriter_RIPReader协议固化设计.md`
- [x] 确认不改变 RGBWSV 协议
- [x] 确认不做彩色纹理
- [x] 确认不做 RIP 半色调

---

## Milestone 03-1：Manifest Schema

- [x] manifest 增加 `schema = p0.rgbwsv.1`
- [x] manifest 确认写入 `tiff.channelCount = 6`
- [x] manifest 确认写入 `tiff.bitDepth = 8`
- [x] manifest 确认写入 `tiff.polarity = black_is_print`
- [x] manifest 确认写入 `printValue = 0`
- [x] manifest 确认写入 `emptyValue = 255`
- [x] manifest 增加 layer list 或强化现有 layer 记录

---

## Milestone 03-2：Reader 校验增强

- [x] 校验 schema
- [x] 校验 channelOrder
- [x] 校验 channelCount
- [x] 校验 bitDepth
- [x] 校验 polarity
- [x] 校验 printValue / emptyValue
- [x] 校验 grid fields
- [x] 校验 layer list
- [x] 校验 layer file exists
- [x] 校验 TIFF SamplesPerPixel
- [x] 校验 TIFF BitsPerSample
- [x] 校验 TIFF PlanarConfig
- [x] 校验 TIFF dimensions

---

## Milestone 03-3：错误码

- [x] 定义 ValidationErrorCode
- [x] 错误信息包含 code
- [x] 错误信息包含 field path
- [x] 错误信息包含 expected / actual
- [x] `rip_reader_test --expect-message` 继续兼容
- [x] 可选实现 `--expect-code`

---

## Milestone 03-4：Bad Package 生成

- [x] 生成 bad_missing_manifest
- [x] 生成 bad_schema
- [x] 生成 bad_bit_depth
- [x] 生成 bad_channel_order
- [x] 生成 bad_polarity
- [x] 生成 bad_print_value
- [x] 生成 bad_empty_value
- [x] 生成 bad_missing_layer
- [x] 生成 bad_layer_size
- [x] 生成 bad_samples_per_pixel
- [x] 生成 bad_planar_config

推荐通过脚本生成：

```text
scripts/make_bad_packages.ps1
```

---

## Milestone 03-5：统计字段

- [x] 新增 printPixels
- [x] 新增 fullPrintPixels
- [x] 新增 partialPrintPixels
- [x] 新增 emptyPixels
- [x] 保留 nonZeroPixels 兼容但标记 deprecated
- [x] 更新 reports 文档

---

## Milestone 03-6：回归脚本

- [x] 新增 `scripts/run_regression.ps1`
- [x] 覆盖 ordinary P0
- [x] 覆盖 Relief package
- [x] 覆盖 Support package
- [x] 覆盖 Bad package
- [x] 输出汇总结果

---

## Milestone 03-7：回归确认

必须确认：

- [x] `samples/configs/slice_config.json` 通过
- [x] `relief_nail_varnish_support.json` 通过
- [x] `relief_nail_white_support.json` 通过
- [x] `support_bottom_projection.json` 通过
- [x] `support_unsupported_only.json` 通过
- [x] `support_bottom_plus_unsupported.json` 通过
- [x] `support_island_filter.json` 通过
- [x] bad packages 按预期失败

---

## Milestone 03-8：状态报告

- [x] 生成 `REPORT_03_RGBWSV协议固化当前实现状态.md`
- [x] 记录 manifest schema
- [x] 记录 reader 校验项
- [x] 记录 bad package 测试结果
- [x] 记录 regression 结果
- [x] 记录仍未实现的 RIP 功能
