# DEV_12E-08C-R4 ModelPreflight 与 RepairAssetAdmission 设计

> 文档版本：v0.1
> 文档状态：DEV / PREPARED
> 日期：2026-07-21
> 对应 PRD：PRD_12E_08C_R4_模型导入预检与修复资产准入.md

## 1. 架构目标

将“诊断事实”“模式准入”“UI 展示”和“修复资产审计”分离。core 不依赖 Qt，UI 不直接拼装拓扑规则，
report writer 不拥有业务决策。

## 2. 推荐模块

```text
ModelPreflightService
  -> FastImportValidator
  -> TransformedGeometryDiagnostics
  -> MeshRepairEligibilityPolicy
  -> BackendCapabilityProbe

SliceAdmissionPolicy
  -> legacy admission
  -> global_surface_shell admission

ModelPreflightCache
  -> key(source/resource/transform/options/algorithmVersion)

RepairAssetAdmissionService
  -> required identity registry
  -> provenance/attribute diff
  -> post-strict evidence validator

Qt ModelPreflightController
  -> async request/cancel/lifetime
  -> Chinese DTO presentation
  -> one-click action gate
```

依赖方向：

```text
apps/slicer_debug_ui -> core facade/DTO
pipeline -> SliceAdmissionPolicy
reports -> immutable result DTO
output writer <- admitted composed layers only
```

## 3. DTO 草案

```cpp
enum class ModelPreflightStatus
{
    Pending,
    Running,
    Passed,
    Warning,
    Blocked,
    Stale
};

struct ModelPreflightIssue
{
    std::string code;
    std::string category;
    std::string severity;
    std::uint64_t count{0U};
    std::string summary;
    std::string recommendation;
};

struct ModeAdmissionResult
{
    std::string mode;
    std::string status;
    std::vector<std::string> blockerCodes;
    std::vector<std::string> warningCodes;
};

struct ModelPreflightResult
{
    std::string schema;
    std::string cacheKey;
    std::string sourceHash;
    std::string resourceHash;
    std::string transformHash;
    ModelPreflightStatus status{ModelPreflightStatus::Pending};
    ModeAdmissionResult legacyAdmission;
    ModeAdmissionResult globalAdmission;
    std::vector<ModelPreflightIssue> issues;
    bool productionOutputWritten{false};
};
```

Public 接口实现时遵循项目 PascalCase/Doxygen/Allman 规范；以上仅为合同草案，不表示已实现。

## 4. 缓存与执行顺序

缓存键必须包含：

```text
模型字节 hash；MTL/贴图/3MF resource hash；
最终 transform、autoOrient、单位和缩放；
preflight options；selected pipeline mode；
diagnostic algorithm/schema version。
```

执行顺序：

```text
Import -> Fast Check -> transform/autoOrient -> Full Preflight -> Mode Admission
       -> passed/warning: selected pipeline
       -> blocked: report/UI only, no selected pipeline start
```

模式切换可以复用诊断事实，但必须重新计算 `ModeAdmissionResult`。几何或资源变化必须重跑完整诊断。

## 5. 严重级别与错误码

建议稳定码：

```text
E_12E_PREFLIGHT_NOT_RUN
E_12E_PREFLIGHT_STALE
E_12E_PREFLIGHT_IMPORT_INVALID
E_12E_PREFLIGHT_RESOURCE_MISSING
E_12E_PREFLIGHT_NON_FINITE_GEOMETRY
E_12E_PREFLIGHT_AUDIT_INCOMPLETE
E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED
E_12E_PREFLIGHT_BACKEND_UNAVAILABLE
E_12E_REPAIR_ASSET_IDENTITY_MISMATCH
E_12E_REPAIR_ASSET_ATTRIBUTE_MISMATCH
E_12E_REPAIR_ASSET_POST_STRICT_FAILED
```

诊断 code 与 UI 中文文案分离；core 只输出稳定 code/参数，Qt `HelpTextProvider` 或 presenter 负责中文。

## 6. Model Fill 材料解析

几何分区只输出 `modelFillMask`，材料解析在 material policy/composer：

```text
white -> W；
varnish -> V；
rgb/custom -> RGB 配置值；
profile_default/material_role -> MaterialProcessProfile resolved RGB/W/V values；
C/M/Y/K -> material_role 预设，不新增 C/M/Y/K TIFF channel。
```

UI 选择 C/M/Y/K 时写入稳定 role id；effective config 必须记录 `requestedRole`、`resolvedChannels`、
`profileId` 和不可用原因。实际通道值由工艺 Profile/标定决定，DEV 不硬编码未经确认的物理墨量。

## 7. Texture Surface Width

配置仍使用 `texture.surfaceShell.widthMm`：

```text
baseMinimumWidthMm = 0.10；
widthStepMm = 0.01；
effectiveMinimumWidthMm = max(baseMinimum, 2 * maxClassificationResolutionMm)；
maximum = allTextureThresholdMm；
requested 值越界时 clamp 到 effective 值并记录原因。
```

预检只负责模型/模式可用性和动态分析前置；width sweep 与互补 mask 继续复用现有 12E service。

## 8. 修复资产准入

`RepairAssetAdmissionService` 输入：原 case identity、原 source hash、修复模型、修复说明、工具/版本和可选
人工审计记录。输出必须包括：

```text
new source/resource/geometry/attribute hash；
bounds/scale/component/triangle count delta；
material assignment 与 per-corner UV diff；
texture resource provenance；
完整 self-intersection status；
post-strict status；
admittedForGlobal。
```

任何未知属性映射、尺寸超阈值变化或 strict blocker 都返回 blocked，不覆盖原始模型和历史证据。

## 9. UI 集成

一键动作统一调用：

```text
OnImportAndSlice -> EnsureFreshPreflight(legacy) -> StartLegacy or ShowBlocker
OnImportAndGlobalSlice -> EnsureFreshPreflight(global) -> StartGlobalDiagnostic/Production or ShowBlocker
```

不得维护两套独立检测实现。异步 worker 需要取消、关闭窗口生命周期和 stale result 防护。

## 10. 测试策略

```text
unit：状态机、cache key、mode admission、error ordering；
fixture：invalid/open/non-manifold/self-intersection/clean textured OBJ/Texture2D 3MF；
UI smoke：检测中、通过、警告、阻断、模式切换、重新检测；
negative：stale result、backend unavailable、resource changed、attribute mismatch；
Release：检测时间、global core time、peak working set 分开统计；
regression：legacy repair OFF TIFF invariant、RIP strict、no silent fallback。
```

## 11. 风险与回滚

| 风险 | 缓解 |
|---|---|
| legacy 被新 strict 规则意外阻断 | mode-aware policy；旧配置默认 legacy 兼容级别 |
| 预检重复导致 UI 变慢 | hash cache、异步执行、阶段化 fast/full check |
| stale 结果错误放行 | generation id + cache key 双校验 |
| 修复后纹理错位 | attribute diff 与 closest-surface/UV fixture |
| C/M/Y/K 被误当协议通道 | 只作为 material role，经 Profile 解析到 RGBWSV |

回滚时可关闭 UI preflight feature，但 global strict admission 不得回滚；legacy 默认路径和生产 writer 保持不变。

