# DOC_PREP_12E-R0 Config、DTO 与启动准入准备

> 文档状态：PREPARED / 12E-01 READY FOR USER ADMISSION
> 日期：2026-07-16
> 覆盖任务：12E-01 Config 与 DTO 契约

## 1. 准备结论

12E 的产品语义、配置字段、静态校验、运行时准入、报告骨架和负向用例已经拆分到可执行粒度。`12E-01` 可以在用户明确指定后开始，但当前仍是准备状态，不修改 C++、Qt、CMake、配置 fixture 或生产 TIFF。

12E-01 只建立契约，不生成三维 mask，不接入 composer，不写 package。12D-R3 与 12E 是两条独立准入轨道：12D-07 已准备但未启动；12E-01 也已准备但未启动，任何一条都不能自动开始。

## 2. 当前代码事实

当前 A 级代码边界：

```text
TextureConfig 只有 enabled/apply_mode/top_surface_layers/sampler 等平面字段；
ModelFillConfig 已有 enabled/material/scope/value，但没有全局壳层补集 scope；
config.cpp 只接受现有 texture apply mode 和 modelFill scope；
slicer.cpp 仍按 layer/column 决定 texture 与 fill；
surface_shell_from_sdf 受 experimental OpenVDB 门禁约束；
当前没有 GlobalTextureFillPartitionOptions/Result；
当前没有 slicesoft.texture_fill_partition.12e.1 报告实现；
当前没有 global_surface_shell production backend。
```

因此，12E-01 不能把配置可解析描述成切片能力可用，也不能以旧的逐层逻辑静默代替全局三维分区。

## 3. 12E-01 配置契约

建议的唯一首版结构：

```json
{
  "texture": {
    "enabled": true,
    "applyMode": "global_surface_shell",
    "surfaceShell": {
      "geometryMode": "global_3d_distance",
      "widthMm": 0.10,
      "widthStepMm": 0.01,
      "minimumWidthPolicy": "two_cells_floor_0_10_mm",
      "surfaceScope": "all_closed_surfaces",
      "fullTextureAtModelLimit": true
    }
  },
  "modelFill": {
    "enabled": true,
    "material": "white",
    "scope": "complement_of_global_texture_shell",
    "value": 0,
    "emptyAllowedInProduction": false
  }
}
```

字段默认与约束：

| 字段 | 首版值/默认 | 静态规则 |
|---|---|---|
| `geometryMode` | `global_3d_distance` | 只接受该枚举 |
| `widthMm` | `0.10` | 有限且大于 0；有效最小值由运行时 preflight 校验 |
| `widthStepMm` | `0.01` | 首版必须精确为 0.01 mm |
| `minimumWidthPolicy` | `two_cells_floor_0_10_mm` | 只接受该枚举 |
| `surfaceScope` | `all_closed_surfaces` | 只接受该枚举 |
| `fullTextureAtModelLimit` | `true` | 首版必须为 true |
| `modelFill.scope` | `complement_of_global_texture_shell` | 必须与 `global_surface_shell` 成对 |

旧配置没有 `surfaceShell` 时保持当前行为；`top_surface_band`、`solid_volume_from_top_surface`、`top_surface_only` 和 `surface_shell_from_sdf` 不自动迁移。

## 4. DTO 边界

12E-01 建议新增纯核心 DTO，不暴露 Qt/OpenVDB 类型：

```text
TextureSurfaceShellConfig
  geometryMode
  widthMm
  widthStepMm
  minimumWidthPolicy
  surfaceScope
  fullTextureAtModelLimit

GlobalTextureFillPartitionOptions
  requestedWidthMm
  widthStepMm
  baseMinimumWidthMm
  surfaceScope

TextureFillPartitionReportData
  availability/status/productionAcceptance
  configSnapshot
  requested/effective/minimum/threshold 占位
  totals/layers/issues 占位
```

12E-01 的 report DTO 只允许输出 `unavailable` 或 `blocked` 骨架，不能输出虚假的 `pass`、coverage、阈值或性能数据。

## 5. 校验分层

配置加载阶段只校验不依赖模型和分类网格的静态规则：

```text
有限数值；
widthMm > 0；
widthStepMm = 0.01；
枚举受支持；
global_surface_shell 与 complement_of_global_texture_shell 成对；
modelFill.enabled=true；
fullTextureAtModelLimit=true。
```

模型 preflight/partition service 阶段才校验动态规则：

```text
effectiveMinimumWidthMm = max(0.10, 2 * classificationResolutionMm)；
requestedWidthMm >= effectiveMinimumWidthMm；
模型已完成最终变换；
拓扑通过对应准入；
backend capability 可用；
allTextureThresholdMm 可计算。
```

未实现 backend 时，结构正确的配置可以进入 DTO，但必须在切片或写包前以稳定错误显式阻断，不得 fallback 到逐层逻辑。

## 6. 稳定错误码

12E-01 应新增 `ValidationErrorCode` 或等价稳定错误标识：

```text
E_12E_SURFACE_SHELL_WIDTH_INVALID
E_12E_SURFACE_SHELL_STEP_UNSUPPORTED
E_12E_SURFACE_SHELL_GEOMETRY_MODE_UNSUPPORTED
E_12E_SURFACE_SHELL_MINIMUM_POLICY_UNSUPPORTED
E_12E_SURFACE_SCOPE_UNSUPPORTED
E_12E_TEXTURE_FILL_SCOPE_MISMATCH
E_12E_MODEL_FILL_REQUIRED
E_12E_PARTITION_BACKEND_UNAVAILABLE
```

CLI 文本可以包含详细原因，但测试应优先断言稳定错误标识，不依赖完整自然语言。

## 7. 文件边界

12E-01 允许修改：

```text
src/slicer_core/config.h
src/slicer_core/config.cpp
src/slicer_core/config/*（若按现有边界拆分）
src/slicer_core/texture_fill_partition_types.*（或等价纯 DTO 文件）
src/slicer_core/texture_fill_partition_report.*（仅 unavailable 骨架）
tests/unit/experimental_config_unit_tests.cpp
新的 config/report DTO unit tests
samples/configs/texture_fill_partition/* 最小 fixture
docs/slice 中对应 schema/状态文档
```

禁止修改：

```text
Qt UI；
OpenVDB 默认值或依赖；
三维 occupancy/distance backend；
composer 与 TIFF writer；
p0.rgbwsv.2、通道顺序、位深、极性；
12D repair 规则。
```

## 8. 12E-01 最小测试矩阵

| Case | 期望 |
|---|---|
| legacy config without `surfaceShell` | 解析与行为不变 |
| valid global shell pair | DTO 字段完整；backend preflight 明确 unavailable |
| width 非有限、0、负数 | 静态拒绝 |
| widthStep 非 0.01 | 静态拒绝 |
| unsupported geometry/policy/scope | 静态拒绝 |
| global shell + legacy fill scope | scope mismatch |
| complement scope + legacy texture mode | scope mismatch |
| modelFill disabled | required error |
| backend unavailable | 不写 package；稳定 unavailable error/report skeleton |

## 9. 验证计划

12E-01 实施时至少运行：

```powershell
cmake --build build --config Debug --target experimental_config_unit_tests
.\build\Debug\experimental_config_unit_tests.exe
ctest --test-dir build -C Debug -R "experimental_config|texture_fill_partition" --output-on-failure
git diff --check
```

若实际 target 路径与当前构建树不同，必须先确认 CMake 输出，不能把计划命令写成已运行结果。

## 10. 准入结论

```text
12E-R0 preparation：COMPLETE；
12E-01：PREPARED / READY FOR USER ADMISSION；
12E-02：BLOCKED BY 12E-01；
12E production：NOT ADMITTED；
12D-R3：保持独立 PREPARED / NOT STARTED。
```
