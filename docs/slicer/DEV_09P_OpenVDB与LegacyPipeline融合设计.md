# DEV_09P_OpenVDB与LegacyPipeline融合设计

> 阶段：09P  
> 子阶段：09P-R1  
> 文档类型：DEV  
> 当前基线：`spike/09B-R3-shell-production-readiness`

## 1. 当前 Legacy Pipeline

legacy production path 仍是默认路径：

```text
SliceConfig
→ model import / report
→ legacy geometry / layer mask
→ material policy
→ RGBWSV package
→ TIFF writer
→ manifest
→ RIP reader / report
```

09P-R1 不替代 legacy path，不修改 production `slicer_cli` 默认行为，不修改 `p0.rgbwsv.2`。

## 2. 当前 OpenVDB/R3 Experimental Pipeline

R3 experimental path 已具备：

```text
OpenVDB level set
surface shell / interior classification
nearest triangle query
surface texture transfer
topology robustness diagnostics
stable issue code
repeat/clamp texture fixture
process peak working set
surface shell report / benchmark report
```

R3 结果只用于 report、preview、benchmark 和实验诊断，未写 production RGBWSV TIFF。

## 3. 09P-R1 Architecture

09P-R1 引入 service boundary，而不是直接把 R3 demo 代码接到 production output：

```text
Config / CLI feature flag
→ ProductionAdmissionPolicy
→ OpenVdbGeometryKernelService
→ SurfaceShellTextureService
→ MaterialChannelComposer bridge
→ diagnostic/report
```

production package 写入仍保持 legacy path 管控。experimental path 默认不写 production RGBWSV。

## 4. ProductionAdmissionPolicy

职责：

```text
读取 ValidationIssue / stable issue code
根据 AdmissionMode 输出 AdmissionDecision
决定 productionAllowed / nonProduction / fail_fast / diagnostic_only
输出 blockerCodes / warningCodes / suggestedActions
```

最低规则：

```text
StrictClosed + MESH_SELF_INTERSECTION_CONFIRMED => FailFast
StrictClosed + MESH_NON_MANIFOLD_EDGES => NonProductionOnly
StrictClosed + MESH_DUPLICATE_FACES => NonProductionOnly
StrictClosed + MESH_OPPOSITE_DUPLICATE_FACES => NonProductionOnly
StrictClosed + MESH_LOCAL_WINDING_INCONSISTENCY => NonProductionOnly
WarnAndAttempt => NonProductionOnly
DiagnosticOnly => DiagnosticOnly
RepairThenStrict => 09P-R1 placeholder, 不得 ProductionAllowed
```

## 5. OpenVdbGeometryKernelService

职责：

```text
封装 OpenVDB level set / shell classification
输出 geometry stats 与 ValidationIssue
在 USE_OPENVDB=OFF 时返回 OPENVDB_UNAVAILABLE
保持 OpenVDB 不是默认强制依赖
```

禁止：

```text
不写 TIFF
不决定材料通道
不替代 legacy geometry 默认路径
```

## 6. SurfaceShellTextureService

职责：

```text
封装 R3 SurfaceTextureTransfer
输入 shell/interior/mesh/texture config
输出 texture transfer stats、preview metadata、ValidationIssue
保持 UV seam 策略：命中哪个 triangle，就使用该 triangle 的 UV
保持 material seam 策略：命中 triangle 的 material 是唯一来源
```

必须保留：

```text
TEXTURE_MISSING
TEXTURE_UV_MISSING
TEXTURE_UV_OUT_OF_RANGE
repeat/clamp 可区分统计
```

## 7. MaterialChannelComposer Bridge

职责：

```text
把 model/interior/surface shell/support/white/varnish 的中间结果组合成 in-memory RGBWSV buffer 或 composition result
固化 priority resolver
保持 channel order = R G B W S V
```

09P-R1 只建立 bridge，不直接写 production TIFF。

## 8. slicer_cli Experimental Path

建议新增显式 flag：

```text
--experimental-openvdb-shell
--admission-mode strict_closed|warn_and_attempt|diagnostic_only
--no-production-rgbwsv
```

行为：

```text
未传 experimental flag => 完全走 legacy path
OpenVDB 不可用 => 输出 OPENVDB_UNAVAILABLE diagnostic，不影响 legacy path
diagnostic_only => 只输出 report
warn_and_attempt => productionAllowed=false, nonProduction=true
strict_closed 有 blocker => 不写 production package
```

## 9. Report / Diagnostic Schema

09P-R1 report 建议新增：

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

字段必须 machine-readable，不能只靠文字 message。

## 10. CMake / USE_OPENVDB Strategy

构建策略：

```text
USE_OPENVDB=OFF 默认可编译
USE_OPENVDB=ON 显式开启
OpenVDB service 在 OFF 时返回 OPENVDB_UNAVAILABLE
不让 OpenVDB 成为所有开发环境的强制依赖
```

OpenVDB 真实验证可使用：

```powershell
$env:VCPKG_ROOT='D:\vcpkg-openvdb'
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09p -Triplet x64-windows
```

## 11. Testing Matrix

默认验证：

```text
USE_OPENVDB=OFF Debug build
ctest
run_ci_quick.ps1
legacy slicer_cli smoke
admission policy unit tests
config default tests
```

OpenVDB 可用时追加：

```text
run_openvdb_smoke.ps1
run_surface_shell_robustness_tests.ps1
run_surface_shell_real_model_tests.ps1
run_surface_shell_texture_tests.ps1
Release benchmark 可作为 nightly / manual
```

09P-R1 不要求真实 OBJ/3MF production RGBWSV 输出。
