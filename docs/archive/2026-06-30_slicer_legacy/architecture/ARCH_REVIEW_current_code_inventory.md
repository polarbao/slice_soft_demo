# ARCH_REVIEW_current_code_inventory

> 文档版本：v0.1  
> 文档状态：R0 代码资产盘点  
> 生成日期：2026-06-10  
> 适用阶段：R0

---

## 1. 盘点范围

本盘点基于当前仓库实际文件、CMake target、脚本入口和 R0 文档约束。

重点范围：

```text
src/slicer_core/
apps/slicer_cli/
apps/rip_reader_test/
apps/slicer_debug_ui/
scripts/
tests/packages/
samples/configs/
```

R0 只做架构审查与重构设计，不移动源码、不重写算法、不改变 `p0.rgbwsv.2` 输出协议。

---

## 2. CMake target 资产

当前顶层 target：

```text
slicer_core
slicer_cli
rip_reader_test
slicer_debug_ui
```

说明：

- `slicer_core` 是核心静态/对象库入口，包含 config、model、slicer、rip_reader、texture_image、tiff_io 和 miniz。
- `slicer_cli` 链接 `slicer_core`，负责命令行切片、模型检查和 preview-only 运行。
- `rip_reader_test` 链接 `slicer_core`，负责 RGBWSV package 校验和负向错误码验证。
- `slicer_debug_ui` 是 Qt5 Widgets 应用，目前不直接链接 `slicer_core`，而是通过 CLI 工具、package、report 文件进行调试闭环。

R1 建议：

- 保留现有 target 名称，先在 `slicer_core` 内部拆分模块。
- UI 继续避免直接依赖核心内部算法，优先依赖稳定的 package/report/service API。
- 在 R2 再考虑将 tests/schema/golden 分层 target 化。

---

## 3. `src/slicer_core/model.*`

当前职责：

```text
基础几何类型：Vec3 / TexCoord / BoundingBox / Triangle
模型报告：ModelReport / MaterialInfo / ThreeMfReportInfo
ASCII/Binary STL 读取
OBJ/MTL 解析
3MF ZIP 解包与安全检查
3MF restricted XML 读取
3MF unit / object / component / transform 处理
3MF BaseMaterial / ColorGroup / Texture2DGroup 解析
纹理资源缓存与路径解析
自动旋转和 bbox 统计
```

代码体量：

```text
model.cpp 约 1662 行
model.h 公开了导入结果、几何、材质、3MF 报告和 load_model_report()
```

可保留资产：

- `ModelReport` 作为输入场景诊断资产可保留，但 R1 应拆成 scene/importer/report 的更清晰类型。
- STL/OBJ/MTL/3MF 的已验证解析能力可保留。
- 3MF deflate、path traversal、受限 XML、负向测试覆盖可保留。

需要重构的职责：

- 3MF ZIP/XML 逻辑应拆到 `importers/three_mf`。
- OBJ/MTL 逻辑应拆到 `importers/obj` 和 `importers/mtl`。
- 纹理资源发现与缓存应拆到 `texture/image` 或 `texture/sources`。
- 几何场景模型应沉淀到 `core/scene`，避免 importer 结果直接携带过多 report 细节。

---

## 4. `src/slicer_core/slicer.*`

当前职责：

```text
run_slicer() 总入口
网格尺寸计算
闭合网格 scanline raster
relief heightfield 采样
支撑 bottom_projection / unsupported_only / full_vertical_projection
SupportType 与孤岛诊断
材质角色映射
纹理采样与 fallback 统计
RGB/W/V/S 通道合成
TIFF layer 写出
preview PNG/PPM 写出
manifest 写出
多类 report JSON 写出
materialProcessProfile 验证统计
```

代码体量：

```text
slicer.cpp 约 2877 行
slicer.h 仅公开 SliceRunResult / SliceRunOptions / run_slicer()
```

可保留资产：

- `run_slicer()` 可以作为短期 facade 继续稳定 CLI。
- 当前 RGBWSV 8-bit、`black_is_print`、`R G B W S V` 通道顺序应冻结。
- 当前 report 输出资产、preview 输出资产和 support diagnostics 资产应保留。

需要重构的职责：

- pipeline orchestration 应从算法细节中拆出。
- raster/relief/support/material/output/report 写出需要拆成独立模块。
- `TextureApplicationPolicy`、`VarnishGeometryPolicy`、`SupportPolicy` 应成为显式策略对象，替代继续扩展 if/else。
- report 写文件不应散落在 slicer 主流程末端，R1/R2 应沉淀为 `reports/writers`。

---

## 5. `src/slicer_core/config.*`

当前职责：

```text
读取 JSON 配置
解析 input/output/transform/autoOrient/material/texture/materialPolicy/materialProcessProfile/materialRoleMapping/support/preview/relief
执行字段合法性校验
保留多个阶段的历史字段与限制提示
```

代码体量：

```text
config.cpp 约 598 行
config.h 公开 SliceConfig 及所有子配置结构
```

可保留资产：

- `SliceConfig` 可作为 legacy config DTO 保留。
- 现有字段覆盖了 P0 Demo 全链路。
- 校验错误能阻止尚未实现策略误用。

需要重构的职责：

- 引入正式 `schema = slicer.config.1`。
- 增加 config migration 层，兼容现有 P0 配置。
- 将 texture/material/support/preview 等配置解析拆分到各模块。
- 将阶段性错误文案从配置模型中剥离，改为统一 diagnostics/error code。

---

## 6. 输出与 RIP 校验资产

当前文件：

```text
src/slicer_core/tiff_io.*
src/slicer_core/rip_reader.*
apps/rip_reader_test/main.cpp
```

当前能力：

- 写入/读取 TIFF 基础能力。
- 支持 `p0.rgbwsv.1` legacy 与 `p0.rgbwsv.2` 当前 schema。
- 严格校验 bitDepth、channelOrder、polarity、printValue、emptyValue、layer list、storageMode、tile/strip 参数。
- `rip_reader_test` 支持 `--expect-error` 和 `--expect-code`，可用于负向测试。

R1/R2 建议：

- 将 TIFF writer/reader 拆到 `output/tiff`。
- 将 RGBWSV package manifest 写出与读取拆到 `output/rgbwsv` / `output/manifest`。
- 将 `ValidationErrorCode` 升级为跨 package/schema 的稳定错误码体系。

---

## 7. 纹理与材料资产

当前文件：

```text
src/slicer_core/texture_image.*
src/slicer_core/config.*
src/slicer_core/model.*
src/slicer_core/slicer.*
```

当前能力：

- 文件系统贴图与 3MF 内部贴图加载。
- UV nearest/bilinear 采样。
- missing texture fallback。
- OBJ/MTL 与 3MF 材料角色映射。
- MaterialPolicy / MaterialProcessProfile 可驱动 RGB/W/V 输出组合。

R1/R2 建议：

- `texture_image` 保留为 image codec/bitmap 基础设施。
- 将 UV 采样、texture source、fallback diagnostics 拆到 `texture/sampler`。
- 新增 `materials/texture_application` 承载 full volume / surface shell 策略接口。
- 新增 `materials/varnish_geometry` 承载 additive / compensated 策略接口。

---

## 8. 支撑与几何采样资产

当前能力集中在 `slicer.cpp`：

```text
bottom_projection
unsupported_only
bottom_projection_plus_unsupported
full_vertical_projection
SupportType debug
island diagnostics
xy dilation
relief heightfield surface_to_base
```

可保留资产：

- 当前支撑结果和诊断 report 对 Demo 验证有价值。
- 当前 relief heightfield 是单材料浮雕和彩色浮雕链路的基础资产。

需要重构的职责：

- 支撑生成拆到 `support/generation`。
- 支撑连通性、孤岛、SupportType 统计拆到 `support/diagnostics`。
- relief heightfield 拆到 `raster/relief`。
- scanline closed mesh raster 拆到 `raster/scanline`。

---

## 9. Qt Debug UI 资产

当前目录：

```text
apps/slicer_debug_ui/
```

当前能力：

- 配置选择、编辑、校验和 diff。
- CLI 构建/运行入口。
- report 加载与摘要展示。
- material policy / process profile / role mapping / support 参数编辑。
- preview 和 overlay panel。
- self-test 与 `overlay-load-real` UI smoke test。

可保留资产：

- UI 作为调试工具保留。
- UI 不直接链接 `slicer_core` 的方式有利于隔离算法和 Qt。
- Report/Preview 的轻量 service 可以在 R1/R2 继续发展为工具层。

需要重构的职责：

- UI 配置编辑器应跟随 `slicer.config.1` schema/migration 更新。
- UI smoke test 应逐步拆成 widget/service smoke 与端到端工具 smoke。
- ConfigDiff 的 Copy Path / Export Diff JSON / 根节点过滤仍是未完成易用性项。

---

## 10. 脚本与测试资产

当前脚本：

```text
scripts/run_regression.ps1
scripts/make_3mf_samples.ps1
scripts/make_bad_3mf_packages.ps1
scripts/run_3mf_negative_tests.ps1
scripts/make_bad_packages.ps1
scripts/compare_material_profiles.ps1
```

当前测试资产：

```text
tests/packages/bad/
tests/packages/bad_3mf/
tests/packages/legacy/
samples/configs/
```

当前分层状态：

- 已有 quick/full/heavy 的脚本分层。
- 已有 RIP bad package 负向测试。
- 已有 3MF bad package 负向测试。
- 单元测试、schema test、golden test 还未正式 target 化。

R2 建议：

- 保留现有脚本作为回归入口。
- 将 schema/golden/unit/ui smoke 分类固化。
- 增加 CI 入口脚本，至少覆盖 build、quick regression、bad package tests、UI self-test。

---

## 11. R0 结论

当前 Demo 资产足以进入正式项目重构设计阶段。

R1 不应直接重写算法，而应按以下顺序推进：

```text
wrap first
move later
rewrite last
```

优先拆分：

```text
model.cpp -> scene/importers/texture resource
slicer.cpp -> pipeline/raster/support/materials/output/reports
config.cpp -> schema/migration/module parsers
```

协议边界保持：

```text
p0.rgbwsv.2 不变
RGBWSV 通道顺序不变
8-bit / black_is_print 不变
现有 quick regression 作为 R1/R2 守门
```
