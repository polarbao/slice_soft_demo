# DOC_EXEC_12E-08C-R1-01 Mesh Repair Contract 结果

> 文档状态：COMPLETE
> 日期：2026-07-20

## 1. 实现结果

R1-01 已建立不修改网格的修复契约基础：

```text
MeshRepairOptions/Input/Hashes/Eligibility/Operation/Diagnostics/Result DTO；
MeshRepairStatus、EligibilityClass、OperationType、AttributeDecision；
E_12E_REPAIR_* 稳定错误码和 MeshRepairError；
mesh_repair_canonical.1 SHA-256 geometry/attribute/options/operation hash；
slicesoft.mesh_repair.12e_08c.1 report skeleton；
mesh_repair_contract_unit_tests 与 JSON golden。
```

## 2. 代码边界

新增代码位于：

```text
src/slicer_core/geometry/repair；
src/slicer_core/diagnostics/MeshRepairReport.*；
tests/unit/mesh_repair_contract；
tests/golden/expected/12e_mesh_repair_report_skeleton.json。
```

Geometry hash 与 attribute hash 独立；options 和 ordered operation list 分别散列。operation hash 排除
`durationMs`，避免非确定计时破坏重复性。浮点按有限 IEEE-754 bit pattern 编码并把负零规范为零。

## 3. 验证结果

实际执行：

```text
cmake --build build --config Debug --target mesh_repair_contract_unit_tests：PASS；
build/Debug/mesh_repair_contract_unit_tests.exe：PASS；
ctest --test-dir build -C Debug -R mesh_repair_contract --output-on-failure：1/1 PASS；
ctest --test-dir build -C Debug --output-on-failure：22/22 PASS；
cmake --build build --config Debug：PASS；
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test：PASS；
scripts/run_ci_quick.ps1：FAIL，现有 material_process_top2 golden 期望 widthPx=48、实际=226。
```

测试覆盖 SHA-256 已知向量、重复稳定性、geometry/attribute/options 独立变化、operation timing 排除、
非法索引稳定错误码和 report golden。

Quick CI 失败发生在本任务未修改的真实模型尺寸 golden；本次没有改动对应模型、配置或生产切片逻辑，且已
还原脚本重生成的 3MF fixture。该命令不记为通过，尺寸基线需在独立回归任务中重新确认。

## 4. 安全边界

```text
repairAttempted=false；
productionOutputWritten=false；
不实现 eligibility policy 或实际 repair；
不写 TIFF/package；
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
OpenVDB 保持 optional/OFF；
legacy production path 不变。
```

## 5. 下一任务

`12E-08C-R1-02 Eligibility Policy` 已具备准备文档，可在用户明确要求后执行。
