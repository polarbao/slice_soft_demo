# DOC PREP R-B-03 降级理由细分实施准备

> 状态：**PREPARATION GATE PASS**
> 日期：2026-08-10
> 前置：R-B-02 COMPLETE

## 1. 当前事实

当前 Provider 在尝试 lod1/lod2 时预先写入 `mesh_lod_reduced_for_max_bytes`，尚未检查网格三角数是否
真的减少。小型 mesh 因纹理预算进入后续尝试时也会被误标为几何降级。

## 2. 最小实施

- 不给公开 DTO 新增字段；
- 在 candidate 实际构建 mesh 后比较源/输出三角数；
- auto 模式发生真实简化时追加 `mesh_simplified_lodN_for_max_bytes`；
- 当前代码删除 generic reason，禁止生成 `mesh_decimated_*`；
- 纹理、top preview 原有理由保持不变；
- 合同升 v1.9 并增加机器校验。

## 3. 文件边界

```text
src/slicer_core/api/viewdata/SceneViewCandidateBuilder.cpp
src/slicer_core/api/viewdata/TexturedSceneViewDataProvider.cpp
tests/stage14b_03a/PositiveCases.cpp
tests/stage14b_03a/SimplificationCases.cpp
contracts/slicer_capability_dtos.json/.md
tests/contracts/ValidateCapabilityDtos.py
docs/slice/DOC/DOC_SCHEMA_14_SceneViewData网格DTO规格.md
```

不修改 Qt、SPI、module 导出、RGBWSV、OpenVDB、mesh 二进制布局或 meshoptimizer 参数。

## 4. 验证门

Debug/Release 均运行 ViewData 单测、真实 fixture、DTO contract、目标图和源码大小门禁。实现与
验收边界明确，R-B-03 可以进入开发。
