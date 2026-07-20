# DOC_EXEC_12E-08C-R1-03 Generated Fixtures 与 Golden 结果

> 文档状态：COMPLETE
> 日期：2026-07-20

## 1. 实现结果

新增 `mesh_repair_fixture_golden_unit_tests`，覆盖 11 个 policy-contract fixture：

```text
clean_closed；
degenerate_face；
duplicate_same_attributes；
duplicate_uv_conflict；
opposite_duplicate；
winding_only；
simple_planar_boundary；
non_planar_boundary；
separable_edge_fan；
ambiguous_edge_fan；
self_intersection。
```

Golden schema 为 `slicesoft.mesh_repair_fixture_golden.12e_08c_r1.1`，冻结每个 case 的 report schema、status、
geometry/attribute SHA-256、issueCode、classification、reasonCode、affectedCount、`repairAttempted=false` 和
`productionOutputWritten=false`。

## 2. 资格证据边界

R1-03 为 policy-contract fixture。boundary planarity/budget 与 non-manifold fan 唯一性作为显式 evidence 输入，
用于冻结策略结果；本任务没有实现真实模型 boundary/fan classifier。真实模型集成基线由 R1-04 执行，复杂
non-manifold pattern classifier 仍属于 R3-01。

新增证据结论：

```text
simple boundary / uniquely separable fan：conditional；
non-planar boundary / ambiguous fan：manual_only；
self-intersection：fail_fast；
attribute conflict：manual_only；
clean：strict_pass_no_repair。
```

## 3. TDD 与验证

先添加使用尚不存在 boundary/fan evidence 枚举的 fixture 测试，定向构建按预期失败；补齐策略后使用空 golden
再次按预期失败并输出实际 projection，最后冻结 golden。实际执行：

```text
cmake --build build --config Debug --target mesh_repair_fixture_golden_unit_tests：PASS；
build/Debug/mesh_repair_fixture_golden_unit_tests.exe：PASS；
ctest --test-dir build -C Debug -R mesh_repair --output-on-failure：3/3 PASS；
cmake --build build --config Debug：PASS；
ctest --test-dir build -C Debug --output-on-failure：24/24 PASS；
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test：PASS。
```

本原子任务未重新运行 `run_ci_quick.ps1`；同一会话 R1-02 已确认该入口仍被既有
`material_process_top2 widthPx expected=48 actual=226` golden 差异阻断。

## 4. 安全边界

```text
不执行 Mesh Repair；
不创建 operation；
不写 production package/TIFF；
不修改 legacy 与 global pipeline；
不改变 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
OpenVDB 保持 optional/OFF。
```

## 5. 下一任务

`12E-08C-R1-04 真实模型 Pre-Repair Baseline` 已解除 R1-03 阻断。R1-04 完成后才能审查 R1 Gate 和 R2
保守修复范围。
