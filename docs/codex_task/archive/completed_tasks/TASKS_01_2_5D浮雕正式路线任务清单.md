# TASKS_01_2_5D浮雕正式路线任务清单

> 文档版本：v0.1  
> 文档状态：Draft / Codex 执行清单  
> 建议提交目录：`docs/slicer/`

---

## Milestone 01-0：阅读与确认

- [x] 阅读 REPORT_00_P0_Demo当前实现状态.md
- [x] 阅读 DOC_DECISION_01_00C完成后的阶段路线调整.md
- [x] 阅读 ROADMAP_v0.2_00C后续PRD_DEV文档生成计划.md
- [x] 阅读 PRD_01 / DEV_01 / DEMO_01
- [x] 确认当前阶段不做彩色纹理
- [x] 确认保持 RGBWSV / uint8 / black_is_print

---

## Milestone 01-1：目录与样例整理

- [x] 创建 `samples/models/relief/`
- [x] 创建 `samples/configs/relief/`
- [x] 将当前 0.3 浮雕样例整理为 relief nail 样例
- [x] 保留原 samples/configs/slice_config_relief_varnish.json 兼容入口
- [x] 新增正式 relief 配置文件

---

## Milestone 01-2：Relief Report 增强

- [x] 增加 totalColumns
- [x] 增加 hitColumns
- [x] 增加 emptyColumns
- [x] 增加 multiHitColumns
- [x] 增加 coverageRatio
- [x] 增加 thicknessMin / thicknessMax
- [x] 增加 support 子对象
- [x] 增加 warnings

---

## Milestone 01-3：通道像素统计标准化

- [x] 统计 V print pixels
- [x] 统计 W print pixels
- [x] 统计 RGB print pixels
- [x] 统计 S support print pixels
- [x] 逐步从 nonZeroPixels 过渡到 printPixels 命名

---

## Milestone 01-4：Relief 测试配置

新增至少：

- [x] relief_nail_varnish_support.json
- [x] relief_nail_white_support.json
- [x] relief_flat_varnish_no_support.json
- [x] relief_rgb_gray.json

---

## Milestone 01-5：回归验证

- [x] 普通 `slice_config.json` 通过
- [x] 当前 `slice_config_relief_varnish.json` 通过
- [x] 新 relief 样例全部通过
- [x] rip_reader_test 全部通过
- [x] preview 输出正确
- [x] reports 输出正确

---

## Milestone 01-6：状态报告

- [x] 新增或更新 `REPORT_01_Relief当前实现状态.md`
- [x] 记录 PRD_01 完成情况
- [x] 记录未实现能力
- [x] 记录下一步进入 PRD_02 / PRD_03 的建议
