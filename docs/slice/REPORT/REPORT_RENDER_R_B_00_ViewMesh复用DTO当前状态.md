# REPORT_RENDER_R_B_00 ViewMesh 复用 DTO 当前状态

> 状态：**COMPLETE**
> 日期：2026-08-10
> 合同：`slicer_capability_dtos` v1.8

## 1. 完成内容

- `SceneViewData` 新增顶层 `meshes`，实例保留 `mesh_identity` 引用。
- local 模式按 modelId 避免重复构建；world 模式按最终 mesh identity 去重。
- ViewData 预算只统计顶层 mesh 一次，闭合校验覆盖唯一性、引用、buffer 和 submesh。
- Adapter 每个 mesh 只存一份 blob，并输出复用同一 blobId 的 v1.7 兼容别名。
- 参考宿主优先解析、上传顶层 meshes，并保留旧内联 mesh 读取路径。
- `viewdataIdentity` 纳入顶层 mesh 集合，身份算法升级为内部 `scene_viewdata.2`。

## 2. 验证结果

| 验证 | Debug | Release |
|---|---:|---:|
| `textured_scene_viewdata_14b03a_unit_tests` | PASS | PASS |
| `hostflow_hd02_three_d_canvas` | PASS | PASS |
| `ValidateCapabilityDtos.py` | PASS | PASS（同一合同门禁） |
| `slicer_stage14e04c_three_d_contract_test` | 未重复运行 | PASS |
| `slicer_source_size_guard_self_test` | 未重复运行 | PASS |

Debug 两项 CTest 总耗时 14.64 秒；Release 两项 CTest 总耗时 1.90 秒。

## 3. 边界与风险

- 未引入 meshoptimizer，未改变 LOD 简化算法。
- 未改变 SPI v1、11 个导出、15 项能力、生产 TIFF/RGBWSV/RIP。
- `instances[].mesh` 仅为兼容别名；后续删除它必须走 major 合同修订。
- 当前只解决跨实例 mesh 重复构建和存储；每三角独立顶点仍由 R-B-05 处理。

## 4. 下一步

执行 R-B-05 顶点共享，再执行 R-A-02 真实资产重基准，最后裁决 RD-B。
