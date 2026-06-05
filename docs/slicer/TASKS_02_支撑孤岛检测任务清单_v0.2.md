# TASKS_02_支撑孤岛检测任务清单_v0.2

> 文档版本：v0.2  
> 文档状态：Draft / Codex 执行清单  
> 建议提交目录：`docs/slicer/`

---

## Milestone 02-0：阅读确认

- [ ] 阅读 REPORT_01_Relief当前实现状态.md
- [ ] 阅读 DOC_REVIEW_02_03_业务逻辑审查与修订结论.md
- [ ] 阅读 PRD_02 / DEV_02 / DEMO_02 v0.2
- [ ] 确认不做彩色纹理
- [ ] 确认不改变 RGBWSV 协议
- [ ] 确认 SupportType 不进入 TIFF 通道

---

## Milestone 02-1：Support 配置扩展

- [ ] 支持 `support.mode = unsupported_only`
- [ ] 支持 `support.mode = bottom_projection_plus_unsupported`
- [ ] 支持 `support.mode = full_vertical_projection`，可先作为 debug 保留
- [ ] 增加 `support.minOverlapRatio`
- [ ] 增加 `support.minIslandAreaPx`
- [ ] 增加 `support.connectivity`
- [ ] 增加 `support.unsupportedProjection`
- [ ] 保持 `bottom_projection` 兼容

---

## Milestone 02-2：Connected Component

- [ ] 实现 4/8 邻域连通域
- [ ] 输入 model mask layer
- [ ] 输出 component id / area / pixel list
- [ ] 增加小面积过滤能力

---

## Milestone 02-3：Island Detection

- [ ] 计算 `previous_model OR previous_support`
- [ ] 支持 optional xy dilation
- [ ] 计算 overlap ratio
- [ ] 判断 island
- [ ] 统计 islandCount / islandPixels
- [ ] 统计 filteredIslandCount / filteredIslandPixels

---

## Milestone 02-4：Unsupported Support Generation

- [ ] 实现 `project_to_build_plate`
- [ ] 对 island footprint 向下生成 S 支撑
- [ ] 不覆盖 model mask
- [ ] 写入 support type = unsupported_island
- [ ] 保持 Model > Support

---

## Milestone 02-5：组合模式

- [ ] 实现 bottom_projection_plus_unsupported
- [ ] 先生成 bottom_projection
- [ ] 再检测 unsupported island
- [ ] 合并 support masks
- [ ] 支持 supportTypeStats

---

## Milestone 02-6：Reports

- [ ] support_report 增加 supportMode
- [ ] support_report 增加 island stats
- [ ] support_report 增加 supportTypeStats
- [ ] slice_report 每层增加 island / unsupported 字段
- [ ] 保留旧字段兼容

---

## Milestone 02-7：Preview / Debug

- [ ] 可选输出 island_mask
- [ ] 可选输出 unsupported_mask
- [ ] 可选输出 support_type
- [ ] 如暂不输出图像，必须至少输出 report 统计

---

## Milestone 02-8：样例与回归

- [ ] 新增 samples/models/support
- [ ] 新增 samples/configs/support
- [ ] 新增 support_unsupported_only.json
- [ ] 新增 support_bottom_plus_unsupported.json
- [ ] 新增 support_island_filter.json
- [ ] 原 P0 / Relief 样例回归通过

---

## Milestone 02-9：状态报告

- [ ] 生成 REPORT_02_支撑与孤岛检测当前实现状态.md
- [ ] 记录已完成 modes
- [ ] 记录 supportTypeStats
- [ ] 记录未实现复杂支撑能力
- [ ] 建议是否进入 PRD_03
