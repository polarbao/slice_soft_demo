# TASKS_12B_R2 OpenVDB SDF Utility 定位任务清单

> 文档状态：Current Task Plan
> 日期：2026-07-08
> 阶段：12B-R2
> 对应文档：
> - docs/slice/PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md
> - docs/slice/DEV/DEV_12B_R2_OpenVDB_SDFUtility评估设计.md
> - docs/slice/DEMO/DEMO_12B_R2_OpenVDB_SDFUtility验证方案.md
> - docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md

## 边界

R2 只做 OpenVDB SDF utility 定位、验证和报告工程化。

R2 不做：

```text
1. 不替代 legacy production slicer；
2. 不默认启用 OpenVDB；
3. 不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
4. 不从 diagnostic path 写 production RGBWSV TIFF；
5. 不把 warn_and_attempt 视为 production-safe；
6. 不实现 mesh repair / repair_then_strict；
7. 不承诺 OpenVDB 比 legacy 更快。
```

## Task 12B-R2-00 文档准入与阶段启动

状态：DONE

内容：

```text
补齐 R2 PRD / DEV / DEMO / TASKS / CODEX_PROMPT；
更新 docs/slice 与 docs/codex_task 入口；
生成 R2 启动状态报告；
判断 R0/R1 完成度和 R2 准入状态。
```

验证：

```powershell
占位标记扫描：目标为 R2 文档包和启动报告
git diff --check
```

## Task 12B-R2-01 当前 OpenVDB utility 代码盘点

状态：DONE

内容：

```text
盘点 OpenVDB 相关 apps/scripts/core APIs/samples/configs；
区分 production basis、experimental utility、prototype/demo only；
输出可复用能力、不可复用能力和 blocker。
```

输出：

```text
docs/slice/DOC/DOC_AUDIT_12B_R2_OpenVDB_SDFUtility代码盘点.md
```

验证：

```powershell
rg -n "OpenVDB|openvdb|SurfaceShell|SDF|outerVarnish|materialClosure" src apps scripts samples\configs
git diff --check
```

完成记录：

```text
已盘点 CMake USE_OPENVDB gate、OpenVdbSurfaceShell、SurfaceShellRealModelPrototype、
SurfaceShellTextureService、SurfaceShellRealModelReport、surface_shell demos、OpenVDB scripts 和 samples/configs/openvdb。

结论：
- outer varnish shell offset：已有 shell mask / SDF shell prototype，成熟度中；
- clearance distance：level set 基础存在，独立 utility 仍缺；
- topology diagnostic：已有 robustness/admission/report，成熟度高；
- material closure assist：12D 语义已有，OpenVDB assist DTO 尚缺。

下一步建议：
执行 R2-02 Utility Report Schema。
```

## Task 12B-R2-02 Utility Report Schema

状态：DONE

内容：

```text
定义 slicesoft.openvdb_sdf_utility.12b_r2.1；
明确 unavailable / blocked / skipped / executed / promoteDecision；
确保该 schema 不等于 production package schema。
```

输出：

```text
docs/slice/DOC/DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md
```

完成记录：

```text
已固化 schema=slicesoft.openvdb_sdf_utility.12b_r2.1；
已定义 outputPolicy、utilities、status、promoteDecision、decision、validation、issues；
已明确 productionReplacementAllowed=false，且禁止写 production package/TIFF；
已补充 OFF lane unavailable 示例和 reader 校验规则。
```

## Task 12B-R2-03 OpenVDB OFF 默认轨道保护

状态：DONE

内容：

```text
验证默认 USE_OPENVDB=OFF 构建和 UI 不受 R2 影响；
legacy benchmark 仍可运行；
OpenVDB utility 在 OFF 下输出 unavailable diagnostic。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_r2_off_guard.json
```

完成记录：

```text
build/CMakeCache.txt 确认为 USE_OPENVDB:BOOL=OFF；
cmake --build build --config Debug --target slicer_cli slicer_debug_ui 通过；
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test 通过；
scripts/run_12b_core_benchmark.ps1 legacy Release NoImageWrite 通过；
benchmark 输出 output/benchmarks/core_benchmark_12b_r2_off_guard.json；
benchmark schema=slicesoft.benchmark.12b.1；
engine=legacy available=true；
outputPolicy.writeTiff=false；
outputPolicy.writePreview=false；
legacy coreComputeMs=18.072；
scripts/run_09p_cli_experimental_tests.ps1 在 USE_OPENVDB=OFF 下通过，
现有 experimental diagnostic report 仍包含 OPENVDB_UNAVAILABLE 且不写 production package。
```

## Task 12B-R2-04 OpenVDB ON Smoke 与可用性报告

状态：DONE

内容：

```text
在已配置的 OpenVDB ON build 上运行 smoke；
若本机无 ON build，输出 blocker，不伪造验证通过。
```

验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
```

完成记录：

```text
build-openvdb-09p/CMakeCache.txt 确认为 USE_OPENVDB:BOOL=ON；
build-openvdb-09p 使用 CMAKE_TOOLCHAIN_FILE=D:\vcpkg-openvdb\scripts\buildsystems\vcpkg.cmake；
scripts/run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p 通过；
脚本增量构建 geometry_kernel_demo 成功；
输出 report=output/GeometryKernelOpenVdb/reports/geometry_kernel_report.json；
report.schema=p0.geometry_kernel_report.1；
report.openvdb.enabled=true；
report.openvdb.available=true；
report.openvdb.version=12.0.1；
report.openvdb.activeVoxels=27；
shellStats.shellPixels=884；
shellStats.interiorPixels=508；
shellStats.boundaryPixels=440。

说明：
该 report 是现有 geometry kernel smoke report；
R2 独立 slicesoft.openvdb_sdf_utility.12b_r2.1 report 原型仍属于 R2-06。
```

## Task 12B-R2-05 Utility Capability Matrix

状态：DONE

内容：

```text
评估 outer varnish shell offset；
评估 clearance / distance diagnostic；
评估 topology diagnostic；
评估 material closure gap analysis assist；
每项给出 promote / keep_experimental / reject。
```

输出：

```text
docs/slice/DOC/DOC_MATRIX_12B_R2_OpenVDBSdfUtilityCapability.md
```

完成记录：

```text
已输出 docs/slice/DOC/DOC_MATRIX_12B_R2_OpenVDBSdfUtilityCapability.md；
OuterVarnishShellOffset：promote，进入 R2-06 最小 utility report 原型；
ClearanceDistance：keep_experimental，现有 distanceStats 不足以作为 production-adjacent gate；
TopologyDiagnostic：promote，作为 report/gate utility 推进；
MaterialClosureAssist：keep_experimental，12D semantic masks 仍为生产真源。
```

## Task 12B-R2-06 最小 Utility Report 原型

状态：PENDING

内容：

```text
在不写 production TIFF 的前提下，生成最小 utility report；
复用已有 OpenVDB service/prototype 时必须保持 optional gate；
OFF lane 返回 unavailable；
ON lane 返回 smoke/capability 统计。
```

验证：

```text
报告 schema 正确；
decision.productionReplacementAllowed=false；
legacy package 不变。
```

## Task 12B-R2-07 R2 当前状态报告

状态：PENDING

内容：

```text
生成 REPORT_12B_R2_OpenVDB_SDFUtility当前状态.md；
明确 R2 是否完成、是否需要 follow-up、是否回到 legacy 优化主线。
```

完成标准：

```text
R2 报告包含 Current State / Target State / Historical State / Pending Confirmation；
包含 capability matrix；
包含验证命令和未运行验证原因；
明确 OpenVDB 是否 promote 为 utility。
```
