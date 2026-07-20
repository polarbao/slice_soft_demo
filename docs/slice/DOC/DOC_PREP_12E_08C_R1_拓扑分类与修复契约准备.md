# DOC_PREP_12E-08C-R1 拓扑分类与修复契约准备

> 文档状态：R1-01 COMPLETE / R1-02 READY
> 日期：2026-07-20
> 当前范围：只准备 R1；不实现 Mesh Repair，不写 production package

## 1. 准备结论

R1 的代码所有权、DTO、Schema、错误码、输入模型、测试边界和退出标准已明确。下一原子任务为
`12E-08C-R1-01 MeshRepair DTO、错误码、canonical hash 与 report skeleton` 已完成；下一任务为
`12E-08C-R1-02 Eligibility Policy`。

## 2. 当前代码事实

当前可复用：

```text
SceneModelTriangleMeshAdapter；
MeshTopologyDiagnostics；
MeshRobustnessDiagnostics；
ValidationIssue；
ProductionAdmissionPolicy；
12E Release benchmark 与真实模型 config/evidence 脚本。
```

当前缺失：

```text
MeshRepairOptions/Result；
repair eligibility policy；
pre/post geometry/attribute hash；
operation list；
attribute preservation validator；
mesh_repair_report；
实际 repair_then_strict 实现。
```

## 3. R1 输入边界

R1 输入必须是最终 transform/autoOrient 后、进入 12E partition 前的 SceneModel mesh。不能对磁盘 OBJ 文本直接做
字符串修补，也不能绕过 importer 资源和 attribute mapping。

## 4. R1 输出边界

R1 只输出：

```text
pre diagnostics；
eligibility；
canonical hash；
report skeleton；
stable issues；
manual repair suggestion。
```

R1 必须满足：

```text
repairAttempted=false；
productionOutputWritten=false；
legacy output unchanged；
OpenVDB OFF build independent。
```

## 5. R1-01 预计文件

```text
src/slicer_core/geometry/repair/MeshRepairTypes.*
src/slicer_core/geometry/repair/MeshRepairHash.*
src/slicer_core/diagnostics/MeshRepairReport.*
tests/unit/mesh_repair_contract_unit_tests.cpp
tests/golden/mesh_repair_report_schema.json
CMakeLists.txt / tests CMake target registration
```

实际修改前必须重新检查当前 CMake target 组织，不按文档路径盲建目录。

## 6. R1-01 完成标准

```text
DTO 不包含 Qt/OpenVDB 类型；
stable status/error code 完整；
canonical serialization 有版本；
相同 mesh 的 hash 重复稳定；
geometry 改变只改变 geometry hash；
attribute 改变只改变 attribute hash；
report skeleton 符合 slicesoft.mesh_repair.12e_08c.1；
不执行修复、不写 package。
```

## 7. 后续 R1 准备

R1-02 使用现有 topology/robustness diagnostics 生成 eligibility，不重复实现拓扑统计。R1-03 建立 generated fixtures；
R1-04 复用 12E-08C 三个真实 OBJ 配置，生成 pre-repair baseline。

## 8. 验证计划

目标创建后至少运行：

```powershell
cmake --build build --config Debug --target mesh_repair_contract_unit_tests
.\build\Debug\mesh_repair_contract_unit_tests.exe
ctest --test-dir build -C Debug -R "mesh_repair" --output-on-failure
git diff --check
```

若 target 路径不同，以 CMake 实际输出为准。本准备文档不把这些计划命令声明为已运行。

## 9. R1 Gate

R1-01 已通过定向测试和默认 Debug CTest。R1-02 可开始。R2 仍未获得代码实施准入；必须先完成 R1 全部
原子任务并审查真实模型 baseline。

## 10. R1-01 实际文件与验证

```text
src/slicer_core/geometry/repair/MeshRepairTypes.*；
src/slicer_core/geometry/repair/MeshRepairHash.*；
src/slicer_core/diagnostics/MeshRepairReport.*；
tests/unit/mesh_repair_contract/main.cpp；
tests/golden/expected/12e_mesh_repair_report_skeleton.json。
```

实际验证：target build PASS、测试可执行文件 PASS、定向 CTest 1/1 PASS、全量 Debug CTest 22/22 PASS。
