# TASKS_00C_单材料浮雕切片任务清单

> 文档版本：v0.1  
> 文档状态：Draft / Codex 执行清单  
> 建议提交目录：`docs/slicer/`

---

## Milestone 00C-0：阅读与确认

- [ ] 阅读 00C 全部文档
- [ ] 确认 00C 不做彩色纹理
- [ ] 确认 00C 不做完整光油覆盖策略
- [ ] 确认 00C 保持 00B 协议

## Milestone 00C-1：配置结构

- [ ] `SliceConfig` 增加 `slicingMode`
- [ ] `MaterialConfig` 增加 `materialChannel`
- [ ] `MaterialConfig` 增加 `applyMode`
- [ ] 增加 `ReliefConfig`
- [ ] 支持 `relief.fillMode`
- [ ] 支持 `relief.baseZMm`
- [ ] 更新配置校验

## Milestone 00C-2：样例配置

- [ ] 添加 `samples/configs/slice_config_relief_varnish.json`
- [ ] 推荐路径：`samples/models/relief/0.3.obj`
- [ ] 默认 `slicingMode = relief_heightfield`
- [ ] 默认 `materialChannel = V`
- [ ] 默认 `support.enabled = false`
- [ ] 默认 `relief.fillMode = surface_to_base`

## Milestone 00C-3：材料通道写入函数

- [ ] 抽出 `write_model_pixel(...)`
- [ ] 支持 `materialChannel = V`
- [ ] 支持 `materialChannel = W`
- [ ] 支持 `materialChannel = RGB`
- [ ] 支持 `materialChannel = auto`
- [ ] 保持 `0=打印，255=不打印`

## Milestone 00C-4：relief heightfield sampler

- [ ] 新增 `sample_relief_heightfield_masks(...)`
- [ ] 对每个 XY 计算垂直射线与三角面的交点
- [ ] 支持 `surface_to_base`
- [ ] 支持 `intersection_range`
- [ ] 输出 `model_masks`
- [ ] 统计 hit/empty/multiHit columns

## Milestone 00C-5：run_slicer 路径切换

- [ ] `closed_mesh_scanline` 继续走原 `sample_model_masks`
- [ ] `relief_heightfield` 走新 sampler
- [ ] 原样例配置不受影响
- [ ] 新 relief 配置可运行

## Milestone 00C-6：relief_report

- [ ] 输出 `reports/relief_report.json`
- [ ] 包含 `slicingMode`
- [ ] 包含 `fillMode`
- [ ] 包含 `baseZMm`
- [ ] 包含 column statistics
- [ ] 包含 warnings

## Milestone 00C-7：manifest 更新

- [ ] manifest 增加 `slicing.mode`
- [ ] manifest 增加 `slicing.reliefFillMode`
- [ ] manifest reports 增加 `relief`
- [ ] `rip_reader_test` 不被破坏

## Milestone 00C-8：测试

- [ ] 原 `slice_config.json` 仍可运行
- [ ] 新 `slice_config_relief_varnish.json` 可运行
- [ ] 输出 TIFF 为 uint8
- [ ] V 通道模型区域为 0
- [ ] 空白区域为 255
- [ ] 默认无 S 支撑
- [ ] Preview 可显示 V 通道
- [ ] `rip_reader_test` 通过

## Milestone 00C-9：状态报告

- [ ] 更新 `docs/slicer/REPORT_00_P0_Demo当前实现状态.md`
- [ ] 说明 00C 已实现内容
- [ ] 说明仍未实现的正式浮雕路线能力
- [ ] 说明彩色纹理阶段仍未开始
