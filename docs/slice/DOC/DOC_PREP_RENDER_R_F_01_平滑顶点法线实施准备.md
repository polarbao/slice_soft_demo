# DOC_PREP R-F-01 平滑顶点法线实施准备

> 状态：PREPARATION GATE PASS  
> 日期：2026-08-11  
> 对应任务：`R-F-01`

## Implementation Plan

### Problem Type

ViewData 显示派生网格质量修复，同时改善由面法线导致的顶点重复。

### Layer(s) Involved

- `slicer_core/api/viewdata`：派生显示网格与法线；
- `tests/stage14b_03a`：平滑曲面、硬边和 UV seam 合同；
- `apps/slicer_ui_host_sim`：真实模型截图和 Release 字节基线。

### Official Documents

- `DOC_ANALYSIS_能力边界与UI融合_四问核实.md` §2；
- `TASKS_RENDER_模型显示与LOD修复补充任务清单.md` 的 `R-F-01/R-F-02`；
- `DOC_DECISION_16_00_Stage16准入Gate口径与R_F线排期裁定.md`。

### Historical Documents

- `REPORT_RENDER_R_A_02_顶点共享后真实资产重测.md`；
- `REPORT_RENDER_R_B_04_ViewData半精度传输当前状态.md`。

### AI Workspace Evidence

当前工作树已有 Stage 16 准入口径和任务状态更新，属于准备上下文；实施不得覆盖或回退。

### Current Code Reality

`SceneViewMeshBuilder` 为每个三角的三个角写入同一面法线，顶点键又包含法线，导致连续曲面刻面且相邻三角无法共享顶点。

### Current State

连续曲面使用面法线；UV seam 可保持，但真实资产平均网格曾达到 `105.15 B/triangle`。

### Target State

对共享几何边且二面角不超过阈值的相邻面，按角度加权形成一致顶点法线；超过阈值的边保持分裂。默认阈值冻结为 `40°`，允许内部构建策略显式覆盖，但不扩展公开 SPI DTO。

### Historical State

R-B-05 已按 `position + normal + uv` 安全键去重；本任务不改变该 seam-safe 键，只改善输入法线。

### Pending Confirmation

无。用户已授权完成 R-F 后进入 Stage 16 准入复核。

### Risk Points

- 跨 UV seam 应共享平滑法线，但仍因 UV 不同保持顶点分裂；
- 只在共享边处建立平滑关系，禁止仅接触同一点的独立壳层互相抹平；
- 材质组、底面和大角度边界不得被抹圆；
- `meshIdentity` 按内容变化是预期缓存失效，不修改 ABI、导出或生产 TIFF。

### Files To Change

- `src/slicer_core/api/viewdata/SceneViewMeshBuilder.h/.cpp`；
- `tests/stage14b_03a/PositiveCases.cpp`；
- R-F 状态报告和任务清单。

### Verification Plan

1. Debug/Release 构建 `textured_scene_viewdata_14b03a_unit_tests`；
2. 运行 provider、真实 fixture、宿主 3D 和 14E-04c 定向回归；
3. Release 真跑参考宿主并保存真实纹理截图；
4. R-F-02 使用 22 个有效资产聚合场景重测 mesh/texture/preview 字节与上传耗时；
5. 运行 `git diff --check`。

## 准备结论

`R-F-01` 的实现边界、默认阈值、硬边策略、测试入口和 R-F-02 连带基线均已冻结，可以进入开发。
