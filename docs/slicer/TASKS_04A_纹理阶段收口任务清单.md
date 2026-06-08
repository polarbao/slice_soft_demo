# TASKS_04A_纹理阶段收口任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：04A  
> 建议提交目录：`docs/slicer/`

---

## Milestone 04A-0：阅读确认

- [x] 阅读 `REPORT_04_彩色纹理切片当前实现状态.md`
- [x] 阅读 `DOC_DECISION_04A_REPORT04后纹理阶段收口修复.md`
- [x] 阅读 `PRD_04A / DEV_04A / DEMO_04A`
- [x] 确认不进入 05 材料策略
- [x] 确认不改变 RGBWSV 协议

---

## Milestone 04A-1：重建 Missing Texture Fixture

- [x] 新增小型 `missing_texture_small.obj`
- [x] 新增小型 `missing_texture_small.mtl`
- [x] MTL 指向不存在的 map_Kd
- [x] 配置文件指向小型 fixture
- [x] 验证 missingTextures > 0
- [x] 验证 fallbackPixels > 0

---

## Milestone 04A-2：重建 No UV Fixture

- [x] 新增小型 `no_uv_small.obj`
- [x] 新增小型 `no_uv_small.mtl`
- [x] OBJ face 不包含 vt
- [x] 配置文件指向小型 fixture
- [x] 验证 facesWithUv = 0
- [x] 验证 facesWithoutUv > 0
- [x] 验证 fallbackPixels > 0

---

## Milestone 04A-3：Heavy Model 分流

- [x] 将 38MB 真实纹理模型标记为 heavy
- [x] heavy 配置默认不进入快速回归
- [x] 可选增加 `-SkipHeavyTexture`

---

## Milestone 04A-4：支撑连通性诊断

- [x] 对 support mask 做 connected component 统计
- [x] 输出 componentCount
- [x] 输出 largestComponentPixels
- [x] 输出 smallComponentCount
- [x] 输出 tinyComponentCount
- [x] 输出 bbox

---

## Milestone 04A-5：回归

- [x] TexturedReliefRgb pass
- [x] TexturedMissingTextureFallback pass
- [x] TexturedNoUvFallback pass
- [x] run_regression.ps1 pass
- [x] bad package pass
- [x] Support / Relief 原样例不破坏

---

## Milestone 04A-6：状态报告

- [x] 生成 `REPORT_04A_纹理阶段收口修复当前实现状态.md`
- [x] 明确是否建议进入 05 材料策略
