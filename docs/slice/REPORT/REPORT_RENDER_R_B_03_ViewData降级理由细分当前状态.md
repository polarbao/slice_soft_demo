# REPORT R-B-03 ViewData 降级理由细分当前状态

> 状态：**COMPLETE**
> 日期：2026-08-10
> 合同：`slicer_capability_dtos` v1.9

## 1. 完成内容

原 `mesh_lod_reduced_for_max_bytes` 已停止产生。当前 Provider 在 `lod=auto` 且输出三角数确实少于
源网格时，返回：

```text
mesh_simplified_lod1_for_max_bytes
mesh_simplified_lod2_for_max_bytes
```

历史抽稀理由 `mesh_decimated_lod1_for_max_bytes` / `mesh_decimated_lod2_for_max_bytes` 已登记为合同
保留字，但当前 Provider 明确禁止产生。无法安全简化时继续 fail-closed，不以 decimation 兜底。

## 2. 修正的误报

旧实现只要 auto 尝试 lod1/lod2 就预先标记几何降级。对于三角数很少、真正减少的是纹理分辨率的模型，
这会错误显示 mesh 已被简化。当前理由在 candidate 完成后依据源/输出三角数生成，因此该场景只返回
`texture_resolution_reduced_for_max_bytes`。

## 3. 合同影响

- DTO 字段形状、C ABI 和导出数量不变；
- 合同版本从 v1.8 升到 v1.9，仅冻结 `truncationReason` 字符串分类；
- 多个原因仍用 `;` 连接，且相同理由不会重复追加；
- `viewdataIdentity` 继续包含完整理由，缓存身份确定性不变；
- 宿主可以对 `mesh_simplified_*` 显示普通质量提示，对 `mesh_decimated_*` 显示强告警。

## 4. 实际验证

Debug 与 Release 均执行：

```text
slicer_capability_dto_contract_test
slicer_stage14b_target_graph_test
slicer_source_size_guard_self_test
textured_scene_viewdata_14b03a_unit_tests
textured_scene_viewdata_14b03a_real_fixture_tests
```

结果均为 `5/5 PASS`。新增门禁验证 auto 16000 三角 fixture 在 384 KiB 预算下选择 lod2 并只返回
`mesh_simplified_lod2_for_max_bytes`；小 mesh 只降纹理时不得出现任何 mesh 理由。

## 5. 边界

本任务未修改 mesh 二进制格式、meshoptimizer 参数、Qt 宿主、生产 TIFF、OpenVDB 或 R-B-04
顶点量化。R-B-04 涉及 buffer format 与宿主解码，必须先完成独立准备和受控合同修订。
