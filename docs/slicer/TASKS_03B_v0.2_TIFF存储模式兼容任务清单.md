# TASKS_03B_v0.2_TIFF存储模式兼容任务清单

> 文档版本：v0.2  
> 文档状态：Codex Task List  
> 适用阶段：REPORT_05 之后 / 03B  
> 建议提交目录：`docs/slicer/`

---

## Milestone 03B-0：阅读确认

- [x] 阅读 `REPORT_05_材料策略当前实现状态.md`
- [x] 阅读 `DOC_DECISION_03B_v0.2_REPORT05后执行TIFF存储模式兼容改造.md`
- [x] 阅读 `PRD_03B / DEV_03B / DEMO_03B v0.2`
- [x] 确认本阶段不改 MaterialPolicy
- [x] 确认默认输出改为 stripped
- [x] 确认保留 tiled 兼容
- [x] 确认 MaterialPolicy 六个样例作为回归基线

---

## Milestone 03B-1：配置结构

- [x] 新增 `output.storageMode`
- [x] 新增 `output.rowsPerStrip`
- [x] 保留 `output.tileSize`
- [x] 兼容旧 `output.tiled`
- [x] 默认 storageMode = stripped
- [x] 默认 rowsPerStrip = 64

---

## Milestone 03B-2：TIFF Writer

- [x] 新增 stripped writer
- [x] 保留 tiled writer
- [x] writer 按 storageMode 分流
- [x] stripped writer 设置 ROWSPERSTRIP
- [x] tiled writer 继续 padding = 255

---

## Milestone 03B-3：Manifest

- [x] schema 升级到 `p0.rgbwsv.2`
- [x] 写入 `tiff.storageMode`
- [x] 写入 `tiff.tiled`
- [x] stripped 写入 `rowsPerStrip`
- [x] tiled 写入 `tileSize`
- [x] 保持 channelOrder / bitDepth / polarity 不变

---

## Milestone 03B-4：RIP Reader

- [x] 支持 p0.rgbwsv.1 legacy tiled
- [x] 支持 p0.rgbwsv.2 stripped
- [x] 支持 p0.rgbwsv.2 tiled
- [x] 根据 TIFFIsTiled 分流读取
- [x] 实现 read_scanline_tiff
- [x] 保留 read_tiled_tiff
- [x] 校验 manifest storageMode 与实际 TIFF 结构一致
- [x] 不使用 TIFFReadRGBAImage

---

## Milestone 03B-5：错误码与负向测试

- [x] 增加 StorageModeInvalid
- [x] 增加 TiffStorageMismatch
- [x] 增加 RowsPerStripInvalid
- [x] 增加 TileSizeInvalid
- [x] 新增 bad_storage_mode
- [x] 新增 bad_rows_per_strip
- [x] 新增 bad_tiff_storage_mismatch
- [x] 新增 bad_tile_size

---

## Milestone 03B-6：样例与回归

- [x] 新增 storage_stripped_default.json
- [x] 新增 storage_tiled_compat.json
- [x] 新增 material_policy_rgbwv_stripped 样例
- [x] 新增 material_policy_rgbwv_tiled 样例
- [x] run_regression.ps1 默认跑 stripped
- [x] run_regression.ps1 跑 MaterialPolicy 六个样例
- [x] run_regression.ps1 至少跑一个 tiled compatibility sample
- [x] 校验 MaterialPolicy top_n_layers 语义不变

---

## Milestone 03B-7：状态报告

- [x] 生成 `REPORT_03B_TIFF存储模式兼容当前实现状态.md`
- [x] 说明默认 storageMode
- [x] 说明 schema 兼容策略
- [x] 说明 reader 双模式支持
- [x] 说明 MaterialPolicy 六个样例回归结果
- [x] 说明 bad storage package 结果
