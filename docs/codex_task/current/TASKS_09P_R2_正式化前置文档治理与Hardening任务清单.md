# TASKS_09P_R2_正式化前置文档治理与Hardening任务清单

> 文档版本：v0.1
> 文档状态：Codex Task List / 09P-R2 Entry
> 生成日期：2026-06-30
> 当前阶段：09P-R1 已完成，09P-R2 hardening 准备阶段
> 适用分支：`spike/09P-openvdb-experimental-pipeline`

---

## 1. 总规则

每次只执行用户明确指定的一个任务。

每个任务开始前：

```powershell
git status --short
```

如果只有用户提供的 PDF 或明确输入资料未跟踪，可以在最终报告中说明并继续；其他未提交改动必须先询问。

每个任务完成前：

```powershell
git status --short
git diff --check
```

涉及代码或脚本时，根据任务运行指定验证命令。

生产安全规则：

```text
不默认启用 OpenVDB。
不让 OpenVDB 成为强制依赖。
不替代 legacy slicer_cli production path。
不从 experimental path 写真实 OBJ/3MF production RGBWSV TIFF。
不修改 p0.rgbwsv.2。
不修改 channelOrder = R G B W S V。
不修改 bitDepth = 8。
不修改 polarity = black_is_print。
warn_and_attempt 永远不得 productionAllowed。
confirmed self-intersection 必须 fail_fast。
non-manifold / duplicate / opposite duplicate / local winding 必须阻断 strict production admission。
```

---

## 2. 09P-R2 阶段目标

09P-R2 是 hardening 阶段，不是功能膨胀阶段。

目标：

```text
1. 修正文档当前状态；
2. 固化 experimental report schema；
3. 强化 topology admission gate；
4. 明确 mesh repair 前置判断；
5. 收敛 service data contract；
6. 设计 experimental golden / downstream output contract / texture fidelity compatibility；
7. Qt UI 读取 experimental report；
8. 建立 OpenVDB OFF / ON CI matrix；
9. 生成 REPORT_09P_R2。
```

非目标：

```text
production RGBWSV 输出真实 OBJ/3MF
自动 mesh repair 大实现
默认 OpenVDB
协议升级
compensated varnish
support clearance
RIP 半色调、设备通信、喷头 bitstream 与设备工艺联调
```

---

## 3. Task 09P-R2-0：同步文档当前状态

状态：已完成。提交：`945f097 docs(slice): 收口文档治理与AI协作配置`。

目标：

```text
把 README、AGENTS.md、.agents/docs、MASTER PRD/DEV、handoff、09P TASKS 中仍停留在 09B-R3 / 09P-R1 前的描述同步为：

当前阶段：09P-R1 已完成
下一阶段：09P-R2 hardening
```

允许修改：

```text
README.md
AGENTS.md
.agents/docs/project-profile.md
.agents/docs/build-and-test.md
docs/slice/README.md
docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md
docs/slice/PRD/PRD_FORMAL_SliceSoft_正式切片软件产品需求总览.md
docs/slice/DEV/DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md
docs/slice/ROADMAP/ROADMAP_FORMAL_SliceSoft_Demo到正式项目演进路线.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
```

禁止事项：

```text
不改源码。
不改 CMake。
不改测试。
不删除历史文档。
```

验证：

```powershell
git status --short
git diff --check
```

完成条件：

```text
所有主要入口都能指向 09P-R1 已完成和 09P-R2 hardening。
旧阶段仍保留为历史，不伪装成当前。
```

---

## 4. Task 09P-R2-1：新增 09P-R2 PRD / DEV / DEMO / CODEX_PROMPT

状态：已完成并已提交。主要提交：`1670110 docs(slice): 建立09P-R2正式文档与任务入口`，后续修正：`a6c5765 docs(slice): 修正文档治理文件EOF空行`。

目标：

新增阶段文档：

```text
docs/slice/PRD/PRD_09P_R2_OpenVDB实验生产管线Hardening.md
docs/slice/DEV/DEV_09P_R2_ReportSchema_AdmissionGate_CI_UI设计.md
docs/slice/DEMO/DEMO_09P_R2_OpenVDB实验生产管线Hardening验证方案.md
docs/codex_task/current/CODEX_PROMPT_09P_R2_OpenVDB实验生产管线Hardening执行指令.md
```

文档必须覆盖：

```text
report schema
productionAdmission
topology admission gate
mesh repair pre-check
service contract
Qt UI report integration
CI matrix
golden / downstream output contract / texture fidelity compatibility
```

验证：

```powershell
git status --short
git diff --check
```

---

## 5. Task 09P-R2-2：固化 experimental report schema

状态：已完成并已提交。提交：`aa5b3be feat(09P): 固化 experimental OpenVDB report schema`。

目标：

将当前 experimental report 字段整理为稳定 schema，并增加 schema validator 或脚本验证。

涉及对象：

```text
p0.experimental_openvdb_shell_cli_report.1
productionAdmission
diagnostics / ValidationIssue
openvdb status
legacyPathExecuted
productionPackageWritten
writeProductionRgbwsv
```

建议新增：

```text
docs/slice/DOC/DOC_SCHEMA_09P_R2_experimental_openvdb_shell_report.md
tests/golden/expected/09p_experimental_report_schema.json
scripts/run_09p_schema_tests.ps1
```

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_cli_experimental_tests.ps1
git diff --check
```

本轮已运行验证：

```powershell
cmake -S . -B build-09p-r2-nmake -G "NMake Makefiles" -DBUILD_SLICER_DEBUG_UI=OFF -DENABLE_GEOMETRY_KERNEL_DEMO=OFF
cmake --build build-09p-r2-nmake
ctest --test-dir build-09p-r2-nmake --output-on-failure
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_cli_experimental_tests.ps1 -BuildDir build-09p-r2-nmake -Config Debug
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_schema_tests.ps1 -BuildDir build-09p-r2-nmake -Config Debug
```

说明：

```text
默认 Visual Studio build 目录调用 HostX86\x64 cl.exe 时多次超时。
本轮改用 VsDevCmd + NMake Makefiles + HostX64 验证目录完成编译与测试。
build-09p-r2-nmake 为 .gitignore 覆盖的临时验证目录。
```

---

## 6. Task 09P-R2-3：强化 topology admission gate

状态：本轮完成，提交见 `feat(09P): 强化 topology admission gate`。

目标：

把 09B-R3 / 09P-R1 的 blocker 规则进一步文档化和测试化。

必须覆盖：

```text
MESH_BOUNDARY_EDGES
MESH_SELF_INTERSECTION_CONFIRMED
MESH_NON_MANIFOLD_EDGES
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_LOCAL_WINDING_INCONSISTENCY
OPENVDB_UNAVAILABLE
OPENVDB_LEVEL_SET_FAILED
```

输出：

```text
strict_closed gate matrix
warn_and_attempt matrix
diagnostic_only matrix
repair_then_strict placeholder matrix
```

新增正式矩阵文档：

```text
docs/slice/DOC/DOC_MATRIX_09P_R2_topology_admission_gate.md
```

验证：

```powershell
cmake --build build --config Debug --target production_admission_policy_unit_tests
.\build\Debug\production_admission_policy_unit_tests.exe
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

---

## 7. Task 09P-R2-4：定义 mesh repair 前置判断

状态：本轮完成，提交见 `docs(09P): 定义 mesh repair 前置判断`。

目标：

只定义 repair 前置判断，不实现大规模自动 repair。

文档必须回答：

```text
哪些 issue 可 repair？
哪些 issue 必须 reject？
repair 后是否必须重新诊断？
repair 前后 hash 如何记录？
repair_then_strict 何时允许 productionAllowed？
```

建议新增：

```text
docs/slice/DOC/DOC_DECISION_09P_R2_mesh_repair_admission_gate.md
```

已新增：

```text
docs/slice/DOC/DOC_DECISION_09P_R2_mesh_repair_admission_gate.md
```

禁止事项：

```text
不实现自动 repair。
不将 repair_then_strict 标记为 productionAllowed。
不放宽 strict_closed。
```

验证：

```powershell
git status --short
git diff --check
```

---

## 8. Task 09P-R2-5：收敛 service data contract

状态：本轮完成，提交见 `docs(09P): 收敛 service data contract`。

目标：

明确以下服务之间的数据契约：

```text
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer
ProductionAdmissionPolicy
ReportWriter
```

输出：

```text
输入 DTO
输出 DTO
ValidationIssue 传播规则
timing/memory/stat 字段
允许为空的字段
必须稳定的字段
```

建议新增：

```text
docs/slice/DEV/DEV_09P_R2_ServiceDataContract.md
```

已新增：

```text
docs/slice/DEV/DEV_09P_R2_ServiceDataContract.md
```

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

---

## 9. Task 09P-R2-6：设计 experimental golden / downstream output contract / texture fidelity compatibility

状态：本轮完成，提交见 `test(09P): 增加 experimental golden 验证契约`。

目标：

不要直接写真实 OBJ/3MF production TIFF，但要定义如何比较 experimental candidate 输出。

应明确：

```text
report golden
diagnostic golden
optional in-memory RGBWSV summary golden
downstream compatibility 不等于 production safe
哪些字段可比较
哪些字段只作趋势
```

建议新增：

```text
docs/slice/DEMO/DEMO_09P_R2_experimental_golden_rip_compatibility.md
scripts/run_09p_golden_tests.ps1
```

已新增：

```text
docs/slice/DEMO/DEMO_09P_R2_experimental_golden_rip_compatibility.md
scripts/run_09p_golden_tests.ps1
tests/golden/expected/09p_experimental_output_contract.json
```

验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1
git diff --check
```

---

## 10. Task 09P-R2-7：Qt Debug UI 读取 experimental report

状态：本轮完成，提交见 `feat(ui): 展示 experimental OpenVDB report 摘要`。

目标：

让 Qt Debug UI 能读取并展示 experimental OpenVDB report，而不是直接触发 production package。

最小 UI 能力：

```text
OpenVDB availability
productionAdmission.status
productionAllowed
nonProduction
blockerCodes
warningCodes
legacyPathExecuted
productionPackageWritten
```

禁止事项：

```text
UI 不直接依赖 OpenVDB 类型。
UI 不调用 OpenVDB 内部算法。
UI 不绕过 CLI/report。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

---

## 11. Task 09P-R2-8：建立 CI matrix

目标：

整理并实现分层 CI 脚本入口：

```text
OpenVDB OFF：build + ctest + run_ci_quick + experimental CLI unavailable smoke
OpenVDB ON：openvdb smoke + surface shell tests + experimental CLI smoke
Benchmark：Release optional/manual
```

建议新增或更新：

```text
scripts/run_09p_r2_ci_matrix.ps1
docs/slice/DEMO/DEMO_09P_R2_CI_Matrix验证方案.md
```

验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_r2_ci_matrix.ps1
git diff --check
```

OpenVDB ON 验证需本机环境：

```powershell
$env:VCPKG_ROOT='D:\vcpkg-openvdb'
```

---

## 12. Task 09P-R2-9：生成 REPORT_09P_R2

目标：

阶段完成后生成：

```text
docs/slice/REPORT/REPORT_09P_R2_OpenVDB实验生产管线Hardening当前状态.md
```

必须包含：

```text
当前分支
当前基线
已完成任务
新增/修改文件
验证命令
未运行验证及原因
production 禁止事项
仍不可 production-safe 的输入
是否进入 09P-R3
是否需要 mesh repair/admission gate 专项
```

验证：

```powershell
git status --short
git diff --check
```

---

## 13. 推荐执行顺序

```text
09P-R2-0 文档状态同步
09P-R2-1 阶段文档包
09P-R2-2 report schema
09P-R2-3 admission gate
09P-R2-4 mesh repair 前置判断
09P-R2-5 service data contract
09P-R2-6 golden / downstream output contract / texture fidelity compatibility
09P-R2-7 Qt UI report integration
09P-R2-8 CI matrix
09P-R2-9 REPORT_09P_R2
```

---

## 14. 给 Codex 的调用模板

```text
请阅读 AGENTS.md、docs/slice/README.md、
docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md、
docs/slice/PRD/PRD_FORMAL_SliceSoft_正式切片软件产品需求总览.md、
docs/slice/DEV/DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md、
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md。

现在只执行 Task 09P-R2-X：<任务标题>。

要求：
1. 只做这个任务，不执行下一个任务。
2. 开始前运行 git status --short。
3. 只修改任务允许范围内的文件。
4. 完成后运行任务指定验证命令。
5. 不修改 p0.rgbwsv.2。
6. 不默认启用 OpenVDB。
7. 不把 warn_and_attempt 视为 production-safe。
8. 不 push。
```

---

## 15. 结论

09P-R2 的第一目标不是“继续加功能”，而是把 09P-R1 的实验边界变成工程上可靠的 hardening 层。
只有当 report schema、admission gate、service contract、UI 展示和 CI matrix 都清楚之后，才能判断是否进入 09P-R3 或先拆 mesh repair/admission gate 专项。
