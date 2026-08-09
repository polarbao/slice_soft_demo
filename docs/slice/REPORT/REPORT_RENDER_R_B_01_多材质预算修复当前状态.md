# REPORT R-B-01 多材质预算修复当前状态

> 状态：**COMPLETE**
> 日期：2026-08-10
> 前置：R-A-02 COMPLETE

## 1. 完成内容

- 移除跨材质组复用的全局 `stride`；
- 按全实例 `triangleLimit` 为非空材质组分配确定性比例配额；
- 预算足够时每个非空材质组至少保留一个三角，余量按最大余数分配；
- 每个材质组按其准确配额做均匀抽样，最终三角总数不再受分组取整误差影响；
- 预分配容量改用实际分配总数；
- 新增 12000/4000 双材质 fixture，lod2 精确输出 7500/2500，共 10000 三角。

## 2. 验证结果

| 验证 | 配置 | 结果 |
|---|---|---|
| `textured_scene_viewdata_14b03a_unit_tests` | Debug | PASS |
| `textured_scene_viewdata_14b03a_unit_tests` | Release | PASS |
| `hostflow_hd02_three_d_canvas` | Debug | PASS |
| `hostflow_hd02_three_d_canvas` | Release | PASS |
| `slicer_stage14e04c_three_d_contract_test` | Debug | PASS |
| `slicer_stage14e04c_three_d_contract_test` | Release | PASS |
| `slicer_source_size_guard_self_test` | Debug/Release | PASS |
| `hostflow_hd02_real_asset_matrix` | Release | PASS（104.07 s） |

22 个合同有效资产聚合复测结果：

```text
validAssets=22
sceneInstances=22
rendered=true
lod=lod2
triangles=210308
meshBytes=22692432
textureBytes=105380368
```

`210308` 与 22 个有效模型各自 `min(sourceTriangles, 10000)` 的求和完全一致；旧实现仅输出
`157405`，说明 R-B-01 已消除材质分组取整造成的配额欠用。

## 3. 边界与下一步

聚合场景仍为 `lod2`，且当前组内均匀抽样仍会破坏网格连通性。R-B-01 只闭合“输出数量
符合预算”这一问题，不闭合视觉质量。下一张算法卡 R-B-02 需要真正网格简化；推荐的
`meshoptimizer` 会修改第三方依赖，仍须取得 RD-B 明确授权后才能执行。
