# TASKS_R1_核心模块边界重构任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List  
> 适用阶段：R1  
> 建议提交目录：`docs/slicer/`

---

## Milestone R1-0：阅读确认

- [x] 阅读 `REPORT_R0_正式项目架构审查与重构设计当前状态.md`
- [x] 阅读 `DOC_DECISION_R1_R0后进入核心模块边界重构阶段.md`
- [x] 阅读 `DEV_R1_核心模块边界重构设计.md`
- [x] 确认 R1 不新增大型功能
- [x] 确认 R1 不实现 surface_shell_texture / compensated_varnish
- [x] 确认 R1 不改 p0.rgbwsv.2 输出协议

---

## Milestone R1-1：建立目录与空模块

- [x] 新建 `src/slicer_core/scene/`
- [x] 新建 `src/slicer_core/importers/obj/`
- [x] 新建 `src/slicer_core/importers/mtl/`
- [x] 新建 `src/slicer_core/importers/three_mf/`
- [x] 新建 `src/slicer_core/pipeline/`
- [x] 新建 `src/slicer_core/materials/`
- [x] 新建 `src/slicer_core/support/`
- [x] 新建 `src/slicer_core/raster/`
- [x] 新建 `src/slicer_core/output/rgbwsv/`
- [x] 新建 `src/slicer_core/reports/`
- [x] 新建 `src/slicer_core/diagnostics/`
- [x] CMake 增量加入空模块源文件

---

## Milestone R1-2：Scene Model 边界

- [x] 新增 `scene/SceneModel.*`
- [x] 梳理 Model / Mesh / MaterialInfo / TextureInfo / TriangleTextureInfo
- [x] 只做类型迁移或 alias，避免算法重写
- [x] 保持现有 importer 可编译
- [x] quick regression 通过

---

## Milestone R1-3：Importer 边界

- [x] 新增 `ObjImporter.*`
- [x] 新增 `MtlImporter.*`
- [x] 新增 `ThreeMfImporter.*`
- [x] public API 成型
- [x] 内部允许调用 legacy helper
- [x] 不重写 parser
- [x] quick regression 通过

---

## Milestone R1-4：Pipeline Wrapper

- [x] 新增 `PipelineContext.*`
- [x] 新增 `PipelineStepResult.*`
- [x] 新增 `SlicePipeline.*`
- [x] 定义 LoadInputScene / ResolveMaterials / SliceGeometry 等 step
- [x] 第一版 wrapper 可调用 legacy `run_slicing`
- [x] quick regression 通过

---

## Milestone R1-5：Materials 模块边界

- [x] 新增 `MaterialRoleMapping.*`
- [x] 新增 `MaterialPolicy.*`
- [x] 新增 `MaterialProcessProfile.*`
- [x] 新增 `TextureApplicationPolicy.*`
- [x] 新增 `VarnishGeometryPolicy.*`
- [x] 只建立策略对象和配置映射
- [x] 不实现 surface shell
- [x] 不实现 compensated varnish
- [x] quick regression 通过

---

## Milestone R1-6：Support / Raster / Output / Reports 边界

- [x] 新增 support wrapper
- [x] 新增 raster wrapper
- [x] 新增 rgbwsv output wrapper
- [x] 新增 report writer wrapper
- [x] 内部允许 legacy 调用
- [x] quick regression 通过

---

## Milestone R1-7：Legacy 收口

- [x] 标记 legacy 函数
- [x] 记录仍留在 model.cpp 的职责
- [x] 记录仍留在 slicer.cpp 的职责
- [x] 不强行一次性清空 legacy 文件
- [x] 生成 R1 状态报告

---

## Milestone R1-8：最终验证

- [x] `cmake --build build --config Debug`
- [x] `run_regression.ps1 -Mode quick`
- [x] `slicer_debug_ui --self-test`
- [x] `overlay-load-real`
- [x] 生成 `REPORT_R1_核心模块边界重构当前状态.md`
