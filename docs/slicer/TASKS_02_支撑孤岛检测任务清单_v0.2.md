# TASKS_02_支撑孤岛检测任务清单_v0.2

> 文档版本：v0.2  
> 文档状态：Draft / Codex 执行清单  
> 建议提交目录：`docs/slicer/`

---

## Milestone 02-0：阅读确认

- [x] 阅读 REPORT_01_Relief当前实现状态.md
- [x] 阅读 DOC_REVIEW_02_03_业务逻辑审查与修订结论.md
- [x] 阅读 PRD_02 / DEV_02 / DEMO_02 v0.2
- [x] 确认不做彩色纹理
- [x] 确认不改变 RGBWSV 协议
- [x] 确认 SupportType 不进入 TIFF 通道

---

## Milestone 02-1：Support 配置扩展

- [x] 支持 `support.mode = unsupported_only`
- [x] 支持 `support.mode = bottom_projection_plus_unsupported`
- [x] 支持 `support.mode = full_vertical_projection`，可先作为 debug 保留
- [x] 增加 `support.minOverlapRatio`
- [x] 增加 `support.minIslandAreaPx`
- [x] 增加 `support.connectivity`
- [x] 增加 `support.unsupportedProjection`
- [x] 保持 `bottom_projection` 兼容

---

## Milestone 02-2：Connected Component

- [x] 实现 4/8 邻域连通域
- [x] 输入 model mask layer
- [x] 输出 component id / area / pixel list
- [x] 增加小面积过滤能力

---

## Milestone 02-3：Island Detection

- [x] 计算 `previous_model OR previous_support`
- [x] 支持 optional xy dilation
- [x] 计算 overlap ratio
- [x] 判断 island
- [x] 统计 islandCount / islandPixels
- [x] 统计 filteredIslandCount / filteredIslandPixels

---

## Milestone 02-4：Unsupported Support Generation

- [x] 实现 `project_to_build_plate`
- [x] 对 island footprint 向下生成 S 支撑
- [x] 不覆盖 model mask
- [x] 写入 support type = unsupported_island
- [x] 保持 Model > Support

---

## Milestone 02-5：组合模式

- [x] 实现 bottom_projection_plus_unsupported
- [x] 先生成 bottom_projection
- [x] 再检测 unsupported island
- [x] 合并 support masks
- [x] 支持 supportTypeStats

---

## Milestone 02-6：Reports

- [x] support_report 增加 supportMode
- [x] support_report 增加 island stats
- [x] support_report 增加 supportTypeStats
- [x] slice_report 每层增加 island / unsupported 字段
- [x] 保留旧字段兼容

---

## Milestone 02-7：Preview / Debug

- [ ] 可选输出 island_mask
- [ ] 可选输出 unsupported_mask
- [ ] 可选输出 support_type
- [x] 如暂不输出图像，必须至少输出 report 统计

说明：本轮暂不新增 debug preview 图像文件，已按 DEMO_02 要求优先输出 report 统计。

---

## Milestone 02-8：样例与回归

- [x] 新增 samples/models/support
- [x] 新增 samples/configs/support
- [x] 新增 support_unsupported_only.json
- [x] 新增 support_bottom_plus_unsupported.json
- [x] 新增 support_island_filter.json
- [x] 原 P0 / Relief 样例回归通过

---

## Milestone 02-9：状态报告

- [x] 生成 REPORT_02_支撑与孤岛检测当前实现状态.md
- [x] 记录已完成 modes
- [x] 记录 supportTypeStats
- [x] 记录未实现复杂支撑能力
- [x] 建议是否进入 PRD_03
