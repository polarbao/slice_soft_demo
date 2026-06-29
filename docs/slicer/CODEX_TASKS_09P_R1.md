# CODEX_TASKS_09P_R1

本文件用于指导 Codex 在 `polarbao/slice_soft_demo` 仓库中执行 09P-R1 阶段任务。

当前基线：

```text
spike/09B-R3-shell-production-readiness
```

建议工作分支：

```text
spike/09P-openvdb-experimental-pipeline
```

阶段目标：

```text
09P-R1：OpenVDB 表面壳层纹理 experimental production pipeline 接入边界建立。
```

09P-R1 只做 feature flag / experimental path / diagnostic/report / service abstraction，不默认启用 OpenVDB，不替代 legacy production path，不直接写真实 OBJ/3MF 的 production RGBWSV TIFF。

---

## 总执行规则

1. 每次只执行用户明确指定的一个 Task。
2. 不要自动执行下一个 Task。
3. 每个 Task 开始前确认工作树干净。
4. 每个 Task 只修改本任务相关文件。
5. 每个最小任务完成后必须运行任务指定的验证命令。
6. 验证通过后必须立即提交。
7. 如果验证失败，先修复；无法修复则停止并报告失败原因，不要提交失败状态。
8. 不要提交 build、out、tmp、cache、IDE 索引、大体积生成文件。
9. 除非用户明确要求，不要 `git push`。
10. 不允许修改 `p0.rgbwsv.2`、RGBWSV 通道顺序、uint8 位深、`black_is_print` 极性。
11. 不允许默认启用 OpenVDB。
12. 不允许让 OpenVDB 成为所有开发环境的强制依赖。
13. 不允许替代 legacy `slicer_cli` production path。
14. 不允许把 `warn_and_attempt` 输出声明为 production-safe。
15. `confirmed self-intersection` 必须 fail fast。
16. `non-manifold`、`duplicate/opposite duplicate`、`local winding inconsistency` 在 strict production admission 下必须阻断生产准入。

每个 Task 开始前执行：

```powershell
git status --short
```

如果输出非空，先停止并询问用户如何处理，除非这些改动正是上一个失败验证留下且用户明确要求继续修复。

每个 Task 提交前执行：

```powershell
git status --short
git diff --check
```

每个 Task 提交后执行：

```powershell
git status --short
git log -1 --oneline
```

---

## 初始化建议

只在第一次开始 09P-R1 工作时执行：

```powershell
git fetch origin
git checkout spike/09B-R3-shell-production-readiness
git pull --ff-only

git checkout -b spike/09P-openvdb-experimental-pipeline
git status --short
```

如果分支已经存在：

```powershell
git fetch origin
git checkout spike/09P-openvdb-experimental-pipeline
git status --short
```

---

# Task 01：修正文档中的当前阶段基线

## 目标

把 README、MASTER PRD、MASTER DEV、handoff 类文档中仍然停留在 P0、R1、R2 的“当前阶段”描述更新为：

```text
当前最新阶段：09B-R3 已完成
当前工作分支基线：spike/09B-R3-shell-production-readiness
下一阶段：09P OpenVDB 表面壳层纹理实验生产管线接入
```

只做文档状态修正，不改代码。

文档中必须明确：

```text
09B-R3 没有接入 production slicer_cli。
09B-R3 没有写 production RGBWSV TIFF。
09B-R3 没有修改 p0.rgbwsv.2。
09B-R3 没有修改 RGBWSV 通道顺序、uint8 位深和 black_is_print 极性。
真实 OBJ/3MF 当前仍不得直接视为 production-safe。
下一阶段 09P-R1 只做 experimental path / feature flag / diagnostic/report。
```

## 允许修改

优先检查并修改：

```text
README.md
docs/slicer/PRD_MASTER_SliceSoft_正式切片软件产品需求总览.md
docs/slicer/DEV_MASTER_SliceSoft_正式切片软件总体架构与实现路线.md
docs/slicer/CODEX_HANDOFF_切片软件开发上下文.md
```

如果某个文件不存在，跳过并在最终回复中说明。

## 禁止事项

```text
不改 C++ 代码。
不改 CMake。
不改测试。
不新增 09P 设计文档。
```

## 验证命令

```powershell
git status --short
git diff --check
```

## 提交命令

```powershell
git add README.md docs/slicer
git commit -m "docs: align current phase with 09B-R3 readiness"
git status --short
git log -1 --oneline
```

---

# Task 02：新增 09P 阶段文档骨架

## 目标

新增 09P 文档集。09P 是 OpenVDB 表面壳层纹理 experimental production pipeline 接入设计与 implementation 准备阶段。

新增文件：

```text
docs/slicer/PRD_09P_OpenVDB表面壳层纹理实验生产管线接入.md
docs/slicer/DEV_09P_OpenVDB与LegacyPipeline融合设计.md
docs/slicer/TASKS_09P_OpenVDB生产Pipeline实验接入任务清单.md
docs/slicer/CODEX_PROMPT_09P_OpenVDB生产Pipeline实验接入执行指令.md
```

每个文档都必须写明：

```text
09P-R1 只做 feature flag / experimental path。
OpenVDB 默认关闭。
legacy slicer_cli 生产路径不得被替代。
warn_and_attempt 只能 nonProduction。
strict_closed 必须拒绝 non-manifold / duplicate / opposite duplicate / local winding。
confirmed self-intersection 必须 fail_fast。
production RGBWSV 协议不修改。
```

准入策略要写明 R3 结论：

```text
真实 OBJ/3MF 当前没有 confirmed self-intersection。
R2 的 AABB 自相交候选在 R3 narrow-phase 中主要被归类为 false positive。
真实模型 production blocker 已转移为 non-manifold、duplicate/opposite duplicate、local winding、multi-component admission。
真实 OBJ/3MF 当前不能直接 production RGBWSV 输出。
```

## 建议文档内容结构

`PRD_09P_*.md`：

```text
1. 背景
2. 目标
3. 非目标
4. 用户/工程场景
5. 功能范围
6. production safety rules
7. feature flag 策略
8. admission policy
9. 验收标准
10. 风险与限制
```

`DEV_09P_*.md`：

```text
1. 当前 legacy pipeline
2. 当前 OpenVDB/R3 experimental pipeline
3. 09P-R1 architecture
4. ProductionAdmissionPolicy
5. OpenVdbGeometryKernelService
6. SurfaceShellTextureService
7. MaterialChannelComposer bridge
8. slicer_cli experimental path
9. report/diagnostic schema
10. CMake / USE_OPENVDB strategy
11. Testing matrix
```

`TASKS_09P_*.md`：

```text
列出 Task 01 到 Task 12 的阶段计划、验收、验证命令和提交策略。
```

`CODEX_PROMPT_09P_*.md`：

```text
给 Codex 的执行提示，强调每次只做一个任务、每个任务后提交、不默认启用 OpenVDB、不改生产协议。
```

## 验证命令

```powershell
git status --short
git diff --check
```

## 提交命令

```powershell
git add docs/slicer/PRD_09P_OpenVDB表面壳层纹理实验生产管线接入.md `
        docs/slicer/DEV_09P_OpenVDB与LegacyPipeline融合设计.md `
        docs/slicer/TASKS_09P_OpenVDB生产Pipeline实验接入任务清单.md `
        docs/slicer/CODEX_PROMPT_09P_OpenVDB生产Pipeline实验接入执行指令.md

git commit -m "docs: add 09P experimental pipeline planning documents"
git status --short
git log -1 --oneline
```

---

# Task 03：新增 ProductionAdmissionPolicy 模块

## 目标

新增独立 production admission 策略模块，把 R3 stable issue code 转成生产准入决策。

建议新增：

```text
src/slicer_core/diagnostics/ProductionAdmissionPolicy.h
src/slicer_core/diagnostics/ProductionAdmissionPolicy.cpp
```

建议类型：

```cpp
enum class AdmissionMode {
    StrictClosed,
    WarnAndAttempt,
    DiagnosticOnly,
    RepairThenStrict
};

enum class AdmissionStatus {
    ProductionAllowed,
    NonProductionOnly,
    DiagnosticOnly,
    FailFast
};

struct ProductionAdmissionDecision {
    AdmissionStatus status;
    bool productionAllowed;
    bool nonProduction;
    std::vector<std::string> blockerCodes;
    std::vector<std::string> warningCodes;
    std::vector<std::string> suggestedActions;
};
```

可以根据仓库现有命名空间和 `ValidationIssue` 类型调整 API，但外部行为必须符合本任务规则。

## 策略规则

```text
StrictClosed:
  MESH_SELF_INTERSECTION_CONFIRMED => FailFast
  MESH_NON_MANIFOLD_EDGES => NonProductionOnly / blocker
  MESH_DUPLICATE_FACES => NonProductionOnly / blocker
  MESH_OPPOSITE_DUPLICATE_FACES => NonProductionOnly / blocker
  MESH_LOCAL_WINDING_INCONSISTENCY => NonProductionOnly / blocker
  no blocker => ProductionAllowed

WarnAndAttempt:
  永远不得 ProductionAllowed
  输出 NonProductionOnly
  保留 warning/error code
  productionAllowed=false
  nonProduction=true

DiagnosticOnly:
  只输出 DiagnosticOnly
  productionAllowed=false
  nonProduction=true 或按现有 report 约定设置，但不得 productionAllowed

RepairThenStrict:
  当前只保留 enum 和 placeholder
  未实现 repair 时不得 ProductionAllowed
```

## 建议 blocker code

至少包含：

```text
MESH_SELF_INTERSECTION_CONFIRMED
MESH_NON_MANIFOLD_EDGES
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_LOCAL_WINDING_INCONSISTENCY
```

可选 blocker：

```text
OPENVDB_LEVEL_SET_FAILED
OPENVDB_UNAVAILABLE
```

`OPENVDB_UNAVAILABLE` 在 production OpenVDB experimental path 中应阻断该 path，但不应破坏 legacy path。

## 测试要求

新增或复用 unit test target，覆盖：

```text
empty issues + StrictClosed => ProductionAllowed
MESH_SELF_INTERSECTION_CONFIRMED + StrictClosed => FailFast
MESH_NON_MANIFOLD_EDGES + StrictClosed => NonProductionOnly
MESH_DUPLICATE_FACES + StrictClosed => NonProductionOnly
MESH_OPPOSITE_DUPLICATE_FACES + StrictClosed => NonProductionOnly
MESH_LOCAL_WINDING_INCONSISTENCY + StrictClosed => NonProductionOnly
any issue + WarnAndAttempt => NonProductionOnly
any issue + DiagnosticOnly => DiagnosticOnly
RepairThenStrict placeholder => NonProductionOnly 或 DiagnosticOnly，但不得 ProductionAllowed
```

## 禁止事项

```text
不接入 slicer_cli。
不修改 production manifest schema。
不写 TIFF。
不改 OpenVDB demo 行为。
```

## 验证命令

Codex 先检查现有 CMake/test 命名，再选择正确 target。默认至少执行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

如果仓库当前没有可用 `ctest`，则执行新增的 test exe，并在最终回复中说明。

## 提交命令

```powershell
git add src/slicer_core/diagnostics CMakeLists.txt tests
git commit -m "09P: add production admission policy"
git status --short
git log -1 --oneline
```

---

# Task 04：把 09P admission policy 接入 R3 report/diagnostic 层，但不接 slicer_cli

## 目标

在现有 R3 surface shell report / real model report 生成链路中，加入 admission decision 字段，但不接入 production `slicer_cli`。

报告新增字段建议：

```json
{
  "productionAdmission": {
    "mode": "strict_closed",
    "status": "non_production_only",
    "productionAllowed": false,
    "nonProduction": true,
    "blockerCodes": [],
    "warningCodes": [],
    "suggestedActions": []
  }
}
```

字段名可根据仓库现有 JSON 命名风格调整，但必须 machine-readable。

## 要求

```text
1. 使用 Task 03 的 ProductionAdmissionPolicy。
2. OBJ/3MF 真实模型在 strict_closed 下不得 productionAllowed。
3. warn_and_attempt 只能 nonProduction。
4. confirmed self-intersection 必须 fail_fast。
5. 不修改 TIFF writer。
6. 不修改 production manifest schema。
7. 不修改 legacy RGBWSV package 输出。
```

## 测试要求

新增或更新 golden expected，覆盖：

```text
duplicate fixture => productionAdmission.status != production_allowed
local reversed fixture => productionAdmission.status != production_allowed
self intersect fixture => productionAdmission.status == fail_fast
real OBJ strict_closed => productionAllowed=false
real 3MF strict_closed => productionAllowed=false
warn_and_attempt => productionAllowed=false 且 nonProduction=true
```

如果 real model fixture 依赖本机路径或大文件不可用，则至少覆盖 synthetic fixtures，并在报告中说明 real model 验证由现有脚本覆盖。

## 验证命令

优先执行：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

如果本机有 OpenVDB/vcpkg 环境，再执行：

```powershell
$env:VCPKG_ROOT='D:\vcpkg-openvdb'
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09p -Triplet x64-windows
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09p
.\scripts\run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09p
```

## 提交命令

```powershell
git add src scripts tests docs/slicer
git commit -m "09P: attach admission decision to shell diagnostics"
git status --short
git log -1 --oneline
```

---

# Task 05：新增 09P experimental config 字段，默认关闭

## 目标

在现有配置解析/迁移逻辑中新增 experimental OpenVDB pipeline 配置字段，但所有新字段默认关闭。

建议配置结构：

```json
{
  "experimental": {
    "openvdbPipeline": {
      "enabled": false,
      "engine": "legacy",
      "admissionMode": "strict_closed",
      "failurePolicy": "fail_fast",
      "allowNonProductionOutput": false,
      "writeProductionRgbwsv": false
    }
  }
}
```

可以根据仓库现有配置结构调整命名，但默认行为必须保持安全。

## 要求

```text
1. 旧配置不变。
2. 缺省值必须可预测。
3. enabled 默认 false。
4. engine 默认 legacy。
5. admissionMode 默认 strict_closed。
6. failurePolicy 默认 fail_fast。
7. allowNonProductionOutput 默认 false。
8. writeProductionRgbwsv 默认 false。
9. OpenVDB 不可用时必须给出稳定 issue code 或清晰 diagnostic。
10. 不允许因为新增配置让 OpenVDB 成为默认依赖。
```

## 测试要求

新增配置单元测试：

```text
旧配置加载后 experimental.openvdbPipeline.enabled == false
空配置加载后 engine == legacy
空配置加载后 admissionMode == strict_closed
空配置加载后 writeProductionRgbwsv == false
显式 enabled true 但 USE_OPENVDB=OFF 时返回 OPENVDB_UNAVAILABLE 或明确 diagnostic
显式 writeProductionRgbwsv true 必须仍受 admission policy 限制
```

## 禁止事项

```text
不接入 production slicer_cli。
不改 production manifest schema。
不写 TIFF。
不默认启用 OpenVDB。
```

## 验证命令

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

## 提交命令

```powershell
git add src tests samples/configs docs/slicer
git commit -m "09P: add disabled experimental OpenVDB pipeline config"
git status --short
git log -1 --oneline
```

---

# Task 06：新增 OpenVdbGeometryKernelService 抽象层

## 目标

新增一个服务抽象层，不改变现有 OpenVDB demo / robustness 代码的外部行为。

建议新增：

```text
src/slicer_core/geometry/GeometryKernelService.h
src/slicer_core/geometry/OpenVdbGeometryKernelService.h
src/slicer_core/geometry/OpenVdbGeometryKernelService.cpp
```

也可以根据仓库现有目录结构放入更合适的模块，但名称和职责应清晰。

## 最低要求

```text
1. USE_OPENVDB=OFF 时必须能编译。
2. USE_OPENVDB=OFF 时调用 OpenVDB service 返回 OPENVDB_UNAVAILABLE，不崩溃。
3. USE_OPENVDB=ON 时可以复用现有 OpenVdbLevelSetBuilder / OpenVdbSurfaceShellClassifier。
4. 不接入 slicer_cli。
5. 不写 production RGBWSV。
6. 不改变现有 surface_shell_robustness_demo 行为。
```

## 建议接口

```cpp
struct GeometryKernelRequest {
    // mesh, voxel size, shell thickness, bounds, diagnostics options
};

struct GeometryKernelResult {
    bool ok;
    std::vector<ValidationIssue> issues;
    // shell stats, grid stats, timing stats, optional debug info
};

class GeometryKernelService {
public:
    virtual ~GeometryKernelService() = default;
    virtual GeometryKernelResult buildSurfaceShell(const GeometryKernelRequest& request) = 0;
};
```

实际字段可按仓库现有类型调整。

## 测试要求

新增最小 unit test：

```text
USE_OPENVDB=OFF:
  service reports unavailable
  issue code is OPENVDB_UNAVAILABLE
  no exception / no crash

USE_OPENVDB=ON:
  只在本机 OpenVDB 环境可用时测试
  能对简单 fixture 生成 geometry report 或 shell stats
```

## 验证命令

默认：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

OpenVDB 可用时追加：

```powershell
$env:VCPKG_ROOT='D:\vcpkg-openvdb'
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09p -Triplet x64-windows
cmake --build build-openvdb-09p --config Debug --target surface_shell_robustness_unit_tests
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
```

## 提交命令

```powershell
git add src/slicer_core/geometry src/slicer_core/diagnostics CMakeLists.txt tests
git commit -m "09P: introduce OpenVDB geometry kernel service"
git status --short
git log -1 --oneline
```

---

# Task 07：新增 SurfaceShellTextureService 抽象层

## 目标

把现有 R3 的 surface shell texture transfer 链路包装成 service，但不改变原 demo 行为，不接入 production `slicer_cli`。

建议新增：

```text
src/slicer_core/surface/SurfaceShellTextureService.h
src/slicer_core/surface/SurfaceShellTextureService.cpp
```

如果仓库已有更合适的 texture/surface 目录，可根据现有结构调整。

## 最低要求

```text
1. 输入：SceneModel / TriangleMeshData / texture config / shell result。
2. 输出：texture transfer stats、preview info、ValidationIssue[]。
3. 复用已有 NearestTriangleQuery、SurfaceTextureTransfer。
4. 保持 UV seam 策略：命中哪个 triangle，就使用该 triangle 的 UV，不跨 seam 平均。
5. 保持 material seam 策略：命中 triangle 的 material 是唯一来源，不自动跨 material seam 混色。
6. 不写 production TIFF。
7. 不修改 production manifest schema。
```

## 测试要求

覆盖：

```text
repeat texture fixture 仍然触发 TEXTURE_UV_OUT_OF_RANGE
repeat / clamp repeatedSampledVoxels 行为仍可区分
missing texture 返回 TEXTURE_MISSING
missing UV 返回 TEXTURE_UV_MISSING
UV seam 不跨 seam 平均
material seam 不跨 material 自动混色
```

## 验证命令

OpenVDB 环境可用时：

```powershell
cmake --build build-openvdb-09p --config Debug
.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09p
git diff --check
```

如果没有 OpenVDB 环境，至少运行非 OpenVDB unit tests：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

并在最终回复中说明 OpenVDB 脚本未运行。

## 提交命令

```powershell
git add src CMakeLists.txt tests scripts
git commit -m "09P: add surface shell texture service"
git status --short
git log -1 --oneline
```

---

# Task 08：新增 MaterialChannelComposer bridge 的最小实现

## 目标

新增一个 in-memory channel composition bridge，为后续 experimental pipeline 输出做准备。本任务不写 TIFF，不改 production package。

建议新增：

```text
src/slicer_core/material/MaterialChannelComposer.h
src/slicer_core/material/MaterialChannelComposer.cpp
```

## 最低能力

```text
1. 接收 model/interior/surface shell/support/varnish 的中间 layer buffer 或统计对象。
2. 输出 in-memory RGBWSV channel buffer 或 composition result。
3. 明确 priority resolver。
4. 不直接写 TIFF。
5. 不修改现有 TIFF writer。
6. 不修改 manifest schema。
```

## 建议优先级

先固化一个最小、可测试、可解释的优先级：

```text
Empty < Support < ModelBase/Interior < SurfaceShellRGB < WhiteInk < Varnish
```

如果现有项目已有不同策略，以现有策略为准，但必须在文档和测试中固定。

## 测试要求

最小测试：

```text
empty voxel => empty
model voxel => model wins
support-only voxel => support channel set
model + support conflict => 按明确策略输出
surface shell RGB 写入 RGB，但不影响 S/V
white ink 写入 W
varnish 写入 V
RGBWSV channel count/order 不变
```

## 禁止事项

```text
不写 TIFF。
不修改 TIFF writer。
不修改 manifest schema。
不替代 legacy channel composition。
```

## 验证命令

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

## 提交命令

```powershell
git add src/slicer_core/material CMakeLists.txt tests docs/slicer
git commit -m "09P: add material channel composer bridge"
git status --short
git log -1 --oneline
```

---

# Task 09：给 slicer_cli 增加 experimental flag，但只输出 diagnostic/report

## 目标

给 `slicer_cli` 增加显式实验开关，例如：

```text
--experimental-openvdb-shell
--admission-mode strict_closed|warn_and_attempt|diagnostic_only
--no-production-rgbwsv
```

参数名可按现有 CLI 风格调整，但必须显式、默认关闭、不可误触发。

## 本任务允许

```text
1. 解析 flag。
2. 打印或输出 diagnostic/report。
3. 当 OpenVDB 不可用时输出 OPENVDB_UNAVAILABLE。
4. 当 strict admission 失败时输出 blockerCodes。
5. 当 warn_and_attempt 时标记 nonProduction=true。
6. legacy 参数行为保持不变。
```

## 本任务不允许

```text
1. 默认启用 experimental path。
2. 替代 legacy path。
3. 对真实 OBJ/3MF 写 production RGBWSV TIFF。
4. 修改 p0.rgbwsv.2。
5. 改变 legacy slicer_cli 原有参数行为。
6. 把 warn_and_attempt 输出标记为 productionAllowed。
```

## 建议行为

```text
未传 --experimental-openvdb-shell:
  完全走 legacy path。

传 --experimental-openvdb-shell 但 USE_OPENVDB=OFF:
  输出 diagnostic/report。
  包含 OPENVDB_UNAVAILABLE。
  不写 production package。

传 --experimental-openvdb-shell --admission-mode diagnostic_only:
  只输出 report。
  不写 TIFF/package。

传 --experimental-openvdb-shell --admission-mode warn_and_attempt:
  可以跑实验诊断。
  输出 nonProduction=true。
  productionAllowed=false。

传 --experimental-openvdb-shell --admission-mode strict_closed:
  有 blocker 时 fail fast 或 nonProductionOnly。
  不写 production package。
```

## 测试要求

新增 CLI smoke 测试：

```text
slicer_cli legacy 参数仍通过
slicer_cli --experimental-openvdb-shell 在 USE_OPENVDB=OFF 时返回清晰 diagnostic
slicer_cli --experimental-openvdb-shell --admission-mode diagnostic_only 只输出 report，不写 TIFF
slicer_cli --experimental-openvdb-shell --admission-mode warn_and_attempt 输出 nonProduction=true
slicer_cli --experimental-openvdb-shell --admission-mode strict_closed 遇到 blocker 时不写 package
```

## 验证命令

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

OpenVDB 可用时追加：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
.\scripts\run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09p
```

## 提交命令

```powershell
git add apps src scripts tests docs/slicer
git commit -m "09P: add guarded experimental OpenVDB slicer CLI path"
git status --short
git log -1 --oneline
```

---

# Task 10：新增 09P 验证脚本

## 目标

新增一个 09P 专用脚本，集中跑当前 experimental pipeline 需要的最小验证。

建议新增：

```text
scripts/run_09p_experimental_pipeline_tests.ps1
```

## 脚本行为

默认跑 OpenVDB OFF 路径：

```text
1. cmake build Debug
2. admission policy unit tests
3. config default tests
4. slicer_cli legacy smoke
5. run_ci_quick.ps1
```

如果传入 `-OpenVdbBuildDir`，追加：

```text
1. run_openvdb_smoke.ps1
2. run_surface_shell_robustness_tests.ps1
3. run_surface_shell_real_model_tests.ps1
4. run_surface_shell_texture_tests.ps1
```

如果传入 `-RunBenchmarks`，追加：

```text
run_surface_shell_benchmarks.ps1 Release
```

脚本应复用已有脚本，不要复制大段已有逻辑。

## 建议参数

```powershell
param(
  [string]$BuildDir = "build",
  [string]$Config = "Debug",
  [string]$OpenVdbBuildDir = "",
  [switch]$RunBenchmarks
)
```

## 验证要求

```text
脚本失败时必须返回非零 exit code。
脚本必须打印每个阶段名称。
OpenVDB 未提供时不得失败，只跳过 OpenVDB-specific tests。
```

## 验证命令

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1
git diff --check
```

OpenVDB 可用时：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1 -OpenVdbBuildDir build-openvdb-09p
```

## 提交命令

```powershell
git add scripts/run_09p_experimental_pipeline_tests.ps1 docs/slicer
git commit -m "09P: add experimental pipeline validation script"
git status --short
git log -1 --oneline
```

---

# Task 11：新增 09P-R1 阶段报告

## 目标

新增阶段报告，总结 Task 01 到 Task 10 的实现结果、验证命令和限制。

新增文件：

```text
docs/slicer/REPORT_09P_R1_OpenVDB表面壳层纹理实验生产管线接入当前状态.md
```

## 报告必须包含

```text
1. 当前分支。
2. 当前基线。
3. 已完成任务列表。
4. 新增模块列表。
5. 新增配置字段。
6. 新增 CLI experimental flag。
7. admission policy 行为。
8. 已运行验证命令。
9. 未运行验证命令及原因。
10. production 禁止事项。
11. 当前仍不可 production-safe 的输入类型。
12. 下一阶段建议。
```

## 结论必须保守

报告结论必须包含：

```text
09P-R1 已建立 experimental OpenVDB pipeline 接入边界。
legacy production path 未被替代。
OpenVDB 默认仍关闭。
真实 OBJ/3MF 仍不得直接 production RGBWSV 输出。
warn_and_attempt 仍然只能 nonProduction。
下一阶段进入 09P-R2 hardening 或单独 mesh repair/admission gate 阶段。
```

## 禁止事项

```text
不改代码。
不改脚本。
不改测试。
只新增/更新阶段报告。
```

## 验证命令

```powershell
git status --short
git diff --check
```

## 提交命令

```powershell
git add docs/slicer/REPORT_09P_R1_OpenVDB表面壳层纹理实验生产管线接入当前状态.md
git commit -m "docs: add 09P-R1 experimental pipeline status report"
git status --short
git log -1 --oneline
```

---

# Task 12：最终本地总验证，不产生代码改动

## 目标

确认 Task 01 到 Task 11 的提交串起来后仍然可构建、可测试。

这个任务原则上不产生 commit。如果验证发现必须修改代码，则修复后作为单独最小修复提交。

## 默认验证命令

```powershell
git status --short
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
.\scripts\run_09p_experimental_pipeline_tests.ps1
git status --short
```

## OpenVDB 可用时追加

```powershell
$env:VCPKG_ROOT='D:\vcpkg-openvdb'
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09p -Triplet x64-windows
.\scripts\run_09p_experimental_pipeline_tests.ps1 -OpenVdbBuildDir build-openvdb-09p
```

如果需要 Release benchmark：

```powershell
.\scripts\run_surface_shell_benchmarks.ps1 -BuildDir build-openvdb-09p-release -Config Release
```

## 如果没有改动

```powershell
git status --short
git log --oneline -12
```

不要提交空 commit。

## 如果验证发现必须修改代码

只修复验证失败相关问题，然后执行：

```powershell
git status --short
git diff --check
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1

git add <fixed-files>
git commit -m "09P: fix experimental pipeline validation issues"
git status --short
git log -1 --oneline
```

---

# 建议提交序列

最终应形成类似这样的 commit 历史：

```text
docs: align current phase with 09B-R3 readiness
docs: add 09P experimental pipeline planning documents
09P: add production admission policy
09P: attach admission decision to shell diagnostics
09P: add disabled experimental OpenVDB pipeline config
09P: introduce OpenVDB geometry kernel service
09P: add surface shell texture service
09P: add material channel composer bridge
09P: add guarded experimental OpenVDB slicer CLI path
09P: add experimental pipeline validation script
docs: add 09P-R1 experimental pipeline status report
09P: fix experimental pipeline validation issues
```

Task 12 如果没有修复改动，则没有 commit。

---

# 给 Codex 的单任务调用模板

每次让 Codex 执行任务时，建议使用下面模板。

```text
请阅读 AGENTS.md 和 docs/slicer/CODEX_TASKS_09P_R1.md。

现在只执行 Task XX：<任务标题>。

要求：
1. 只做 Task XX。
2. 不执行 Task XX+1。
3. 开始前确认 git status --short 干净。
4. 完成后运行 Task XX 指定的验证命令。
5. 验证通过后只提交本任务相关文件。
6. commit message 使用任务文件中指定的提交信息。
7. 提交后输出 git status --short 和 git log -1 --oneline。
8. 不要 push。
```

示例：

```text
请阅读 AGENTS.md 和 docs/slicer/CODEX_TASKS_09P_R1.md。

现在只执行 Task 03：新增 ProductionAdmissionPolicy 模块。

要求：
1. 只做 Task 03。
2. 不执行 Task 04。
3. 开始前确认 git status --short 干净。
4. 完成后运行 Task 03 指定的验证命令。
5. 验证通过后只提交本任务相关文件。
6. commit message 使用：
   09P: add production admission policy
7. 提交后输出 git status --short 和 git log -1 --oneline。
8. 不要 push。
```