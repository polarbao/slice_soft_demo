# TASKS_04_彩色纹理切片任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 建议提交目录：`docs/slicer/`

## Milestone 04-0：阅读确认

- [x] 阅读 `REPORT_03_RGBWSV协议固化当前实现状态.md`
- [x] 阅读 `DOC_DECISION_04_REPORT03后进入彩色纹理基础阶段.md`
- [x] 阅读 `PRD_04 / DEV_04 / DEMO_04`
- [x] 确认不改变 RGBWSV 协议
- [x] 确认不做 RIP / OpenVDB / Qt

## Milestone 04-1：OBJ Loader 增强

- [x] 解析 `vt`
- [x] 支持 face `v/vt`
- [x] 支持 face `v/vt/vn`
- [x] 记录 face material name
- [x] 统计 facesWithUv / facesWithoutUv

## Milestone 04-2：MTL Loader

- [x] 解析 `newmtl`
- [x] 解析 `Kd`
- [x] 解析 `map_Kd`
- [x] 解析相对路径
- [x] 记录 missing texture warnings

## Milestone 04-3：Texture Loader

- [x] 引入或实现基础 image loader
- [x] 支持 PNG
- [x] 支持 JPG
- [x] 支持 BMP
- [x] 不依赖 Qt

## Milestone 04-4：Texture Sampler

- [x] 支持 nearest
- [x] 支持 bilinear
- [x] 支持 clamp
- [x] 支持 repeat
- [x] 支持 flipV
- [x] 支持 fallbackRgb

## Milestone 04-5：Relief Top Surface Sampling

- [x] relief column 记录 top hit triangle
- [x] 记录 barycentric coordinate
- [x] 插值 UV
- [x] 采样 texture RGB
- [x] 支持 fallback

## Milestone 04-6：Layer Compose

- [x] `texture.enabled = true` 时写 sampled RGB
- [x] support 仍写 S
- [x] 空白仍为 255
- [x] 保持 Model > Support > Empty
- [x] 保持 schema p0.rgbwsv.1

## Milestone 04-7：Reports

- [x] 新增 `texture_report.json`
- [x] 记录 materials / textures
- [x] 记录 sampledPixels
- [x] 记录 fallbackPixels
- [x] 记录 uvOutOfRangePixels
- [x] slice_report 增加 texture 统计

## Milestone 04-8：样例与回归

- [x] 新增 samples/models/textured
- [x] 新增 samples/configs/textured
- [x] 新增 checker / gradient 样例
- [x] 新增 missing texture fallback 样例
- [x] 新增 no UV fallback 样例
- [x] `scripts/run_regression.ps1` 增加 texture 正向用例

## Milestone 04-9：状态报告

- [x] 生成 `REPORT_04_彩色纹理切片当前实现状态.md`
- [x] 记录实现范围
- [x] 记录已通过样例
- [x] 记录未实现的完整全彩能力
- [x] 建议是否进入 05 材料策略
