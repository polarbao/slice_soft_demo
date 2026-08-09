# REPORT R-B-05 顶点共享当前状态

> 状态：**COMPLETE**  
> 日期：2026-08-10  
> 前置：R-B-00 COMPLETE

## 1. 完成内容

- `SceneViewMeshBuilder` 已按最终 `float position + normal + uv` 精确键复用顶点；
- `+0/-0` 已规范为同一键，避免等值坐标生成重复顶点；
- 去重覆盖同一 mesh 的全部 submesh，但不改变各 submesh 的索引区间；
- 新增共面四边形正例，确认 6 个独立顶点压缩为 4 个共享顶点；
- 新增 UV 缝正例，确认同一几何位置因 UV 不同保留两个顶点；
- 未引入 epsilon 焊接、网格简化、拓扑修复或 `meshoptimizer`。

## 2. 验证结果

| 验证 | 配置 | 结果 |
|---|---|---|
| `textured_scene_viewdata_14b03a_unit_tests` | Debug | PASS |
| `hostflow_hd02_three_d_canvas` | Debug | PASS |
| `textured_scene_viewdata_14b03a_unit_tests` | Release | PASS |
| `hostflow_hd02_three_d_canvas` | Release | PASS |
| `slicer_stage14e04c_three_d_contract_test` | Release | PASS |
| `slicer_source_size_guard_self_test` | Release | PASS |

Debug/Release 目标均重新编译，验证不是复用旧二进制结论。

## 3. 边界与风险

当前 `SceneModel` 不保存 OBJ 顶点法向，ViewData 使用三角面法向。因此非共面相邻三角即使
位置和 UV 相同，也会因法向不同而保持分裂。这符合本卡的
`position + normal + uv` 安全合同，但真实曲面压缩率可能低于理论约 28 B/三角。
不得把理论值写成真实模型结论。

## 4. 下一步

执行 R-A-02：使用与 R-A-01 相同的 36 个真实资产，统计顶点共享后的实际 ViewMesh 字节、
新预算触发面和超限模型数。RD-B、R-B-01/02 和 `meshoptimizer` 是否需要启动，以该实测为准。
