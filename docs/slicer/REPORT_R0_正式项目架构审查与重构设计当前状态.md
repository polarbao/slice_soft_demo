# REPORT_R0_正式项目架构审查与重构设计当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态 / 架构审查输出  
> 生成日期：2026-06-10  
> 适用阶段：R0

---

## 1. R0 阶段结论

根据 07B-R1 报告和 R0 系列文档，当前项目应进入：

```text
P0 Demo Feature Freeze
R0：正式项目架构审查与重构设计
```

R0 本次执行的是架构审查与设计输出，不做大规模源码移动，不重写 `slicer_core`，不实现 `surface_shell_texture`，不实现 `compensated_varnish`，不引入 OpenVDB，不接入设备通信，也不修改 RGBWSV 生产输出协议。

本次新增架构审查产物：

```text
docs/slicer/ARCH_REVIEW_current_code_inventory.md
docs/slicer/REPORT_R0_正式项目架构审查与重构设计当前状态.md
```

---

## 2. 当前代码资产盘点

详细盘点见：

```text
docs/slicer/ARCH_REVIEW_current_code_inventory.md
```

当前核心资产：

```text
slicer_core
  config.*
  model.*
  slicer.*
  rip_reader.*
  tiff_io.*
  texture_image.*
  third_party/miniz

apps
  slicer_cli
  rip_reader_test
  slicer_debug_ui

scripts
  run_regression.ps1
  make_3mf_samples.ps1
  make_bad_3mf_packages.ps1
  run_3mf_negative_tests.ps1
  make_bad_packages.ps1
  compare_material_profiles.ps1
```

当前 CMake target：

```text
slicer_core
slicer_cli
rip_reader_test
slicer_debug_ui
```

当前主要代码集中度：

```text
model.cpp 约 1662 行
slicer.cpp 约 2877 行
config.cpp 约 598 行
rip_reader.cpp 约 448 行
tiff_io.cpp 约 622 行
texture_image.cpp 约 211 行
```

这说明 P0 Demo 的功能闭环已经完整，但 `model.cpp` 和 `slicer.cpp` 的职责集中度已经不适合继续堆叠功能。

---

## 3. 模块保留 / 重构清单

### 3.1 建议保留的功能资产

```text
OBJ / MTL / Texture 输入链路
STL ASCII / Binary 基础输入链路
3MF stored / deflate 输入链路
3MF BaseMaterial / ColorGroup / Texture2DGroup
3MF bad package validation
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
Support / SupportType / island diagnostics
Relief heightfield
RGBWSV TIFF writer
RIP reader strict validation
preview report / preview PNG
Qt Debug UI
UI self-test / overlay-load-real smoke test
quick/full/heavy regression 脚本分层
```

### 3.2 必须重构的职责

```text
model.cpp:
  拆出 scene model、OBJ importer、MTL importer、3MF importer、texture resource 解析。

slicer.cpp:
  拆出 pipeline orchestration、raster、relief、support、materials、output、reports。

config.cpp:
  拆出 schema、migration、模块级 config parser、统一 diagnostics。

reports:
  从 slicer 主流程中抽出 writer/schema 层，形成统一 report 基础字段。

tests:
  从脚本集合升级为 unit/golden/schema/regression/ui smoke 的分层入口。
```

### 3.3 暂不重构的边界

```text
不改 p0.rgbwsv.2
不改 R G B W S V 通道顺序
不改 8-bit / black_is_print 极性
不改当前 CLI 基本调用方式
不让 Qt UI 直接依赖 slicer_core 内部算法
```

---

## 4. 正式目录结构建议

建议 R1/R2 逐步收敛到以下结构：

```text
src/
  core/
    config/
    pipeline/
    scene/
    diagnostics/

  importers/
    stl/
    obj/
    mtl/
    three_mf/

  texture/
    image/
    sampler/
    sources/

  materials/
    role_mapping/
    material_policy/
    process_profile/
    texture_application/
    varnish_geometry/

  support/
    generation/
    diagnostics/

  raster/
    scanline/
    relief/

  output/
    rgbwsv/
    tiff/
    manifest/

  reports/
    writers/
    schema/

apps/
  slicer_cli/
  rip_reader_test/
  slicer_debug_ui/

tests/
  unit/
  golden/
  schema/
  packages/
  ui_smoke/
```

依赖方向建议：

```text
importers -> core/scene
core/pipeline -> scene + texture + materials + support + raster + output + reports
apps -> tools/services/package/report
reports -> diagnostics/config snapshot/stats
output/rgbwsv -> output/tiff + output/manifest
```

禁止方向：

```text
core 不依赖 Qt UI
importer 不直接写 TIFF
material policy 不直接读文件系统
support generation 不直接写 report 文件
UI 不访问 slicer.cpp 内部临时结构
```

---

## 5. Pipeline step 设计

正式 pipeline 建议定义为：

```text
LoadConfig
ValidateConfig
MigrateConfig
LoadInputScene
NormalizeScene
ResolveMaterials
PrepareTextureSources
ApplyTextureApplicationPolicy
PrepareVarnishGeometryPolicy
SliceGeometry
GenerateSupport
ComposeMaterialChannels
WriteRGBWSVPackage
WriteReports
ValidatePackage
```

每个 step 应具备：

```text
Input
Output
Config
Diagnostics
Warnings
Errors
Timing
```

建议接口方向：

```cpp
struct PipelineStepResult {
    bool ok;
    Diagnostics diagnostics;
    std::vector<Warning> warnings;
    std::vector<Error> errors;
};
```

R1 实施原则：

```text
wrap first
move later
rewrite last
```

也就是先将当前 `slicer.cpp` 内的阶段逻辑包成 step，再移动文件，最后才考虑算法替换。

---

## 6. TextureApplicationPolicy 设计

R0 必须把当前纹理写入行为上升为正式策略对象。

建议模型：

```cpp
enum class TextureApplicationMode {
    FullVolume,
    SurfaceShell,
    TopSurfaceOnly,
    OuterSurfaceShell
};

struct TextureApplicationPolicy {
    TextureApplicationMode mode;
    int shell_thickness_px;
    double shell_thickness_mm;
    std::string fill_role;
    std::string shell_region;
};
```

短期策略：

```text
FullVolume:
  保留当前 Demo 行为，作为 R1/R2 的兼容默认。

SurfaceShell:
  R0 只设计接口，不实现。
  R2 可做 2D per-layer 近似。
  09 后再考虑 3D SDF / OpenVDB 精确版本。
```

Pipeline 插入点：

```text
PrepareTextureSources 之后
ComposeMaterialChannels 之前
```

---

## 7. VarnishGeometryPolicy 设计

R0 必须把光油行为从 `top_n_layers` 类临时分支上升为正式策略对象。

建议模型：

```cpp
enum class VarnishGeometryMode {
    InPlaceTopLayers,
    AdditiveGrow,
    CompensatedShrink
};

struct VarnishGeometryPolicy {
    VarnishGeometryMode mode;
    int thickness_layers;
    double thickness_mm;
    std::string compensation_method;
};
```

短期策略：

```text
InPlaceTopLayers / AdditiveGrow:
  保留当前 top_n_layers 或外加语义，作为短期默认方向。

CompensatedShrink:
  R0 只设计接口。
  R1 不强制实现。
  09 或几何内核成熟后再实现。
```

Pipeline 插入点：

```text
NormalizeScene / SliceGeometry / ComposeMaterialChannels 之间
```

---

## 8. Config schema 设计

正式配置建议引入：

```text
schema = slicer.config.1
```

建议顶层：

```json
{
  "schema": "slicer.config.1",
  "input": {},
  "output": {},
  "pipeline": {},
  "geometry": {},
  "texture": {},
  "materials": {},
  "support": {},
  "preview": {},
  "diagnostics": {}
}
```

R1/R2 迁移原则：

```text
现有 P0 配置继续兼容；
新增 schema/migration 层；
legacy SliceConfig 可作为迁移后的内部 DTO 短期保留；
错误信息迁移到统一 diagnostics/error code；
UI 配置编辑器跟随 schema 展示，不继续硬编码所有字段。
```

新增策略配置建议：

```json
{
  "materials": {
    "textureApplication": {
      "mode": "full_volume",
      "shellThicknessPx": 3,
      "shellThicknessMm": 0.05,
      "fillRole": "base"
    },
    "varnishGeometry": {
      "mode": "additive",
      "thicknessLayers": 2,
      "thicknessMm": 0.02
    }
  }
}
```

---

## 9. Report schema 设计

统一 report 基础字段建议：

```json
{
  "schema": "p0.report.xxx.1",
  "source": {},
  "configSnapshot": {},
  "stats": {},
  "warnings": [],
  "errors": [],
  "timings": {}
}
```

应统一的 report：

```text
input_report
geometry_report
material_report
texture_report
support_report
process_report
diagnostics_report
preview_report
package_report
three_mf_report
obj_mtl_material_report
```

R1/R2 建议：

- R1 先将 report 写出包装到 `reports/writers`。
- R2 再统一 schema 字段、版本、configSnapshot 和 timings。
- 保持现有 report 文件名兼容 UI 和 regression。

---

## 10. Test / CI 设计

当前已有：

```text
scripts/run_regression.ps1 -Mode quick/full/heavy
scripts/run_3mf_negative_tests.ps1
rip_reader_test --expect-error --expect-code
slicer_debug_ui --self-test
slicer_debug_ui --ui-smoke-test --case overlay-load-real
tests/packages/bad
tests/packages/bad_3mf
tests/packages/legacy
```

建议正式分层：

```text
unit_tests
schema_tests
golden_tests
regression_quick
regression_full
regression_heavy
negative_package_tests
ui_smoke_tests
```

R2 CI 入口建议至少包含：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

---

## 11. R1/R2 是否可以进入

可以进入 R1，但前提是 R1 严格遵守以下边界：

```text
1. 只做模块边界重构，不新增大型功能；
2. 保持 slicer_cli / rip_reader_test / slicer_debug_ui target 可构建；
3. 保持 p0.rgbwsv.2 输出协议不变；
4. 每次拆分后运行 quick regression；
5. 先 wrapper 化 step，再移动代码，最后才优化算法；
6. R1 不实现 surface_shell_texture / compensated_varnish；
7. R2 再处理 config/report/test/CI 的 schema 工程化固化。
```

不建议现在直接进入 08/09/10 的原因：

```text
继续增加支撑形态、OpenVDB 或设备链路会进一步放大 model.cpp / slicer.cpp 的职责集中问题。
正式项目应先通过 R1/R2 建立稳定模块边界和测试守门。
```

---

## 12. 本阶段验证说明

本次 R0 执行只新增文档并更新任务清单，不修改源码、不修改 CMake、不修改配置、不修改样例数据。

因此本次未重新执行完整 quick regression。当前约束仍以 07B-R1 报告中的通过结果作为进入 R0 的前置证据；R1 开始实际移动代码后，必须将 `run_regression.ps1 -Mode quick` 作为每个可提交拆分点的守门验证。
