# TASKS_05A_真实材料工艺参数验证任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：PRD_05A / DEV_05A  
> 建议提交目录：`docs/slicer/`

## Milestone 05A-0：阅读确认

- [x] 阅读 `REPORT_06B_3MF纹理与ColorGroup当前实现状态.md`
- [x] 阅读 05A 文档
- [x] 确认不改 p0.rgbwsv.2 输出协议
- [x] 确认不改 MaterialRoleMapping / MaterialPolicy 基础语义
- [x] 确认不做 OpenVDB / Qt / RIP 半色调

## Milestone 05A-1：MaterialProcessProfile 配置

- [x] 新增 materialProcessProfile 配置
- [x] 支持 profile name
- [x] 支持 target
- [x] 支持 rgb / white / varnish / support / validation 段
- [x] 默认 disabled，不破坏旧配置

## Milestone 05A-2：Report

- [x] 新增 material_process_report.json
- [x] 统计 RGB/W/V/S printPixels
- [x] 统计 per-layer RGB/W/V/S
- [x] 统计 varnish active layers
- [x] 统计 coverage ratio
- [x] 输出 validation.pass / failures / warnings

## Milestone 05A-3：Validation

- [x] requireRgbPixels
- [x] requireWhitePixels
- [x] requireVarnishPixels
- [x] requireSupportPixels
- [x] V top_n_layers 基础校验
- [x] W underbase 基础校验
- [x] Support S 独立性校验

## Milestone 05A-4：Profile Compare

- [x] 新增 compare_material_profiles.ps1
- [x] 比较两个 package 的 material_process_report
- [x] 输出 material_profile_compare_report.json
- [x] 校验 top1/top2/top3 差异

## Milestone 05A-5：样例

- [x] nail_rgb_white_varnish_top1.json
- [x] nail_rgb_white_varnish_top2.json
- [x] nail_rgb_white_varnish_top3.json
- [x] nail_white_underbase_only.json
- [x] nail_varnish_only.json
- [x] three_mf_texture_rgb_white_varnish.json
- [x] obj_mtl_texture_rgb_white_varnish.json

## Milestone 05A-6：回归

- [x] run_regression.ps1 增加 materialProcessCases
- [x] quick regression 增加轻量 profile cases
- [x] heavy regression 可保留真实模型 profile
- [x] rip_reader_test --summary 通过

## Milestone 05A-7：状态报告

- [x] 生成 `REPORT_05A_真实材料工艺参数验证当前实现状态.md`
- [x] 记录 profile 支持范围
- [x] 记录 topLayers 差异
- [x] 记录 underbase 覆盖
- [x] 记录是否建议进入 07 / 06C / 08
