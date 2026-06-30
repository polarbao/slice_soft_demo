# TASKS_R0_架构审查与重构设计任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：R0  
> 建议提交目录：`docs/slicer/`

---

## Milestone R0-0：阅读确认

- [x] 阅读 `REPORT_07B_R1_UI真实OverlaySmoke与配置编辑器小收口当前实现状态.md`
- [x] 阅读 `PRE_R0_DECISION_纹理壳层与光油几何策略约束.md`
- [x] 阅读 R0 架构文档
- [x] 确认进入 Feature Freeze
- [x] 确认 R0 不做大规模代码移动

---

## Milestone R0-1：代码资产盘点

- [x] 盘点 `model.cpp`
- [x] 盘点 `slicer.cpp`
- [x] 盘点 `config.*`
- [x] 盘点 report writers
- [x] 盘点 UI debug app
- [x] 盘点 scripts/regression
- [x] 形成 `ARCH_REVIEW_current_code_inventory.md`

---

## Milestone R0-2：模块边界设计

- [x] scene model
- [x] importers
- [x] texture
- [x] materials
- [x] support
- [x] raster
- [x] output
- [x] reports
- [x] tools
- [x] UI

---

## Milestone R0-3：策略对象设计

- [x] TextureApplicationPolicy
- [x] VarnishGeometryPolicy
- [x] SupportPolicy
- [x] MaterialRoleMapping
- [x] MaterialPolicy
- [x] MaterialProcessProfile

---

## Milestone R0-4：Pipeline 设计

- [x] 定义 pipeline steps
- [x] 定义 step input/output
- [x] 定义 diagnostics/timing
- [x] 定义策略插入点

---

## Milestone R0-5：Config / Report / Test 设计

- [x] config schema
- [x] config migration
- [x] report schema
- [x] diagnostics
- [x] test layering
- [x] CI entry

---

## Milestone R0-6：R1/R2 规划

- [x] 输出 R1 重构边界
- [x] 输出 R2 工程化边界
- [x] 暂不细化到每个源码文件 patch
