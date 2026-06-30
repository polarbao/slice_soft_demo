# TASKS_05_材料策略任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：PRD_05 / DEV_05  
> 建议提交目录：`docs/slicer/`

---

## Milestone 05-0：阅读确认

- [x] 阅读 `REPORT_04A_纹理阶段收口修复当前实现状态.md`
- [x] 阅读 `DOC_DECISION_05_REPORT04A后进入材料策略基础阶段.md`
- [x] 阅读 `PRD_05 / DEV_05 / DEMO_05`
- [x] 确认不做 OpenVDB / 3MF / RIP / Qt
- [x] 确认不改变 RGBWSV 协议

---

## Milestone 05-1：配置结构

- [x] 新增 `materialPolicy.enabled`
- [x] 新增 `materialPolicy.rgb`
- [x] 新增 `materialPolicy.white`
- [x] 新增 `materialPolicy.varnish`
- [x] 新增 `conflictPolicy`
- [x] 保持旧 modelMaterial 兼容

---

## Milestone 05-2：Material Compose

- [x] 抽出 model material pixel compose
- [x] 支持 RGB texture source
- [x] 支持 W underbase
- [x] 支持 V all_model
- [x] 支持 V top_n_layers
- [x] 保持 S support 独立
- [x] 保持 Model > Support > Empty

---

## Milestone 05-3：Column Range

- [x] 复用或生成 lower_layer / upper_layer
- [x] 支持 top_n_layers 判定
- [x] relief_heightfield 路径优先实现
- [x] closed_mesh_scanline 可做 fallback

---

## Milestone 05-4：Reports

- [x] 新增 material_policy_report.json
- [x] 统计 RGB printPixels
- [x] 统计 W printPixels
- [x] 统计 V printPixels
- [x] 记录策略模式
- [x] slice_report 增加 materialPolicyApplied

---

## Milestone 05-5：样例

- [x] 新增 textured_rgb_only.json
- [x] 新增 textured_rgb_white_underbase.json
- [x] 新增 textured_rgb_varnish_top2.json
- [x] 新增 textured_rgb_white_varnish.json
- [x] 新增 varnish_only_all_model.json
- [x] 新增 white_only_all_model.json

---

## Milestone 05-6：回归

- [x] P0 通过
- [x] Relief 通过
- [x] Support 通过
- [x] Texture 通过
- [x] 04A fallback 通过
- [x] MaterialPolicy 样例通过
- [x] Bad package 通过

---

## Milestone 05-7：状态报告

- [x] 生成 `REPORT_05_材料策略当前实现状态.md`
- [x] 说明已实现策略
- [x] 说明未实现材料能力
- [x] 建议是否进入 06 3MF / 多材料
