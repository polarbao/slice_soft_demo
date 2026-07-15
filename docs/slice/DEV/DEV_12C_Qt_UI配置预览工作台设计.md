# DEV_12C_Qt_UI配置预览工作台设计

> 文档版本：v0.2
> 文档状态：DEV / Stage 12C
> 生成日期：2026-07-05
> 更新日期：2026-07-10
> 前置文档：PRD_12C_Qt_UI配置预览工作台收口.md

---

## 1. 设计目标

将当前 Qt 调试 UI 从“配置文件执行器”升级为“切片参数工作台”：

```text
1. ProfileCatalog 管理显示名、分类、可见性和配置模板；
2. SettingsPanel 管理普通用户可理解的材料/支撑/光油/引擎选项；
3. PreviewWorkspace 整合生产层检查、材料叠加、原始调试预览；
4. DiagnosticsDock 承载报告、曲线、日志和 benchmark 信息；
5. HelpTextProvider 为配置项提供中文说明。
```

---

## 2. 建议模块

```text
ProfileCatalog
  读取 samples/configs 与 profile metadata，输出 UI 可见 Profile。

SliceSettingsModel
  保存 UI 设置状态，并可生成实际 slice_config JSON。

SliceSettingsPanel
  普通用户设置页：材料、支撑、光油、预览、引擎。

AdvancedFixturePanel
  高级/测试配置入口，默认折叠或隐藏。

PreviewWorkspace
  统一承载 ProductionLayerView / MaterialOverlayView / RawPreviewView。

DiagnosticsDock
  报告、曲线、日志、benchmark 摘要。

HelpTextProvider
  根据 key 提供中文说明、默认值、影响通道、文档链接。
```

### 2.1 SliceSettingsModel DTO 边界

`SliceSettingsModel` 位于 `apps/slicer_debug_ui/services`，使用值语义保存以下状态：

```text
profileid / modelpath / outputdirectory；
layerthicknessmm；
modelfillmaterial = White | Varnish；
support.enabled / placement / internalvoidenabled / internalvoidminareapx；
surfacevarnish.enabled / thicknesspx；
outervarnish.enabled / thicknessmm / pixelpitchum；
preview.enabled / interval；
enginerole = LegacyProduction | OpenVdbUtilityCandidate。
```

边界约束：

```text
不继承 QObject，不持有 QWidget；
不依赖 slicer_core 内部对象；
外侧光油默认关闭且 thicknessmm=0；
生产 Profile 模型填充无 Empty 枚举；
OpenVDB 只能形成 non-blocking candidate 警告，不能成为默认生产引擎；
R1-03 负责将 DTO 映射到 generated effective config，本类不直接写文件或运行 CLI。
```

---

## 3. Profile 元数据

场景索引从 `slice_soft.scenarios.2` 起提供轻量 metadata，不直接重命名大量模板 JSON：

```json
{
  "id": "material_rgb_white_varnish",
  "displayName": "彩色纹理甲片 - RGB + 白墨 + 光油",
  "category": "production",
  "visibility": "normal",
  "description": "用于标准 OBJ/MTL/PNG 彩色甲片模型。",
  "configPath": "samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json",
  "inputFormats": ["obj"],
  "materialCapabilities": ["rgb_surface", "white_model_fill", "lower_support"],
  "productionSafety": "production",
  "docPath": "docs/slice/PRD/PRD_12C_Qt_UI配置预览工作台收口.md"
}
```

字段约束：

| 字段 | 含义 | 稳定 Profile 要求 |
|---|---|---|
| `id` | 不随中文文案变化的唯一标识 | 必填 |
| `displayName` | UI 中文短名 | 必填 |
| `category` | UI 分组 | 必填 |
| `visibility` | 普通/高级/夹具/隐藏分层 | 必填 |
| `inputFormats` | 可接受的模型格式 | 非空 |
| `materialCapabilities` | RGB、模型填充、支撑、光油等能力标签 | 非空 |
| `productionSafety` | 当前用途安全级别 | 必填 |
| `docPath` | 仓库内说明文档 | 必填且文件存在 |

`productionSafety` 使用以下稳定值：

```text
production：可作为 legacy 生产设置模板；
diagnostic：只用于生产数据检查，不代表新的生产算法；
development_only：高级开发样例；
fixture_only：自动化或回归夹具；
experimental_only：实验能力，禁止默认进入生产流程。
```

旧 `name` 字段只作为 advanced/fixture 的兼容回退；普通稳定 Profile 必须显式提供 `displayName` 和完整元数据。

可见性：

```text
normal：默认显示；
advanced：高级模式显示；
fixture：测试夹具模式显示；
hidden：不在 UI 选择器显示，但可手动打开。
```

---

## 4. 设置到配置的映射

UI 设置不直接等于单个 JSON 文件，而是模板 + override：

```text
模板：提供稳定默认值；
UI override：模型路径、输出目录、材料填充、支撑、光油、引擎；
高级 override：texture layers、support thresholds、preview colors；
```

生成流程：

```text
1. 用户选择 Profile；
2. UI 读取模板配置；
3. 用户修改设置；
4. SliceSettingsModel 合成 generated config；
5. slicer_cli 使用 generated config 切片；
6. UI 加载 package/manifest/report/preview。
```

### 4.1 R1-03 生效配置映射（已实现）

R1-03 使用 `EffectiveConfigGenerator` 承担生成编排，输入优先级固定为：

```text
磁盘 Profile template
< 稳定 Profile 默认 SliceSettingsState
< 当前内存 ConfigDocument dirty override
< 本次运行模型/输出目录 override
```

生成与运行顺序固定为：

```text
读取只读 template/original document
-> 将 Profile 默认值应用为内存 override
-> 合成 SliceSettingsState
-> 写入 input/output/modelFill/support/surfaceVarnish/outerVarnish/preview/experimental
-> SliceSettingsModel::Validate
-> ConfigValidator::validate
-> QSaveFile 原子写入 output/ui_sessions/<session>/slice_config.generated.json
-> slicer_cli --config <generated config>
```

字段映射：

| UI 状态 | generated config |
|---|---|
| 模型/输出/层高 | `input.modelPath` / `output.packageDir` / `output.layerThicknessMm` |
| 模型内部填充 | `modelFill.enabled/material/scope/value/emptyAllowedInProduction/legacyRgbFallback` |
| 支撑位置 | `support.enabled/mode/placement/upper.enabled` |
| 内部镂空 | `support.internalVoid.enabled/minAreaPx/fillRule` |
| 表面光油 | `surfaceVarnish.enabled/thicknessPx/source` |
| 外侧光油 | `outerVarnish.enabled/thicknessMm/pixelPitchUm/conflictPolicy` |
| 预览 | `preview.enabled/interval` |
| legacy | `experimental.openvdbPipeline.enabled=false`、`writeProductionRgbwsv=false` |
| OpenVDB utility/candidate | 仅诊断配置，`admissionMode=diagnostic_only`、`writeProductionRgbwsv=false` |

相对 `input.modelPath` 在 generated config 移入 session 目录前按原模板目录解析为绝对路径，避免路径基准改变。原模板不写回；校验失败不创建 generated config，也不启动 CLI。UI “生效配置”页显示摘要、告警、错误和 template 到 effective config 的全字段差异。

固定协议校验继续阻断 `bitDepth != 8`、`channelOrder != R G B W S V` 和 `background.value != 255`；本阶段不修改 `p0.rgbwsv.2`、uint8 或 `black_is_print`。

---

## 5. 预览整合设计

### 5.1 ProductionLayerView

职责：

```text
读取 layers/layer_xxxxxx.tiff；
显示 RGB/W/S/V 单通道或组合；
提供六通道像素探针；
显示 layerIndex/zMm。
```

### 5.2 MaterialOverlayView

职责：

```text
按配置伪彩合成 RGB/W/S/V；
支持 RGB+S、RGB+W、RGB+V、RGB+W+S+V；
图例可见；
不把黑色生产值直接当 UI 背景。
```

### 5.3 RawPreviewView

职责：

```text
读取 preview 目录；
用于调试生成器输出；
明确标注“原始调试预览，不代表生产 TIFF 全部通道”。
```

### 5.4 R2-01 PreviewWorkspace 与共享层状态（已实现）

`PreviewWorkspace` 位于 `apps/slicer_debug_ui/widgets`，只负责模式承载和真实层号同步，不接管三个既有 panel 的渲染、TIFF 读取、伪彩、缩放或像素探针。

模式映射：

```text
ProductionLayer -> LayerPreviewPanel；
MaterialOverlay -> PreviewOverlayPanel；
RawPreview -> PreviewPanel。
```

共享契约：

```text
LayerPreviewPanel::LayerIndices 优先作为规范层范围；
三个 panel 都提供 CurrentLayerIndex / SelectLayer / SigLayerIndexChanged；
PreviewWorkspace 使用 m_currentLayerIndex 保存唯一共享状态；
同步期间使用 m_syncing 防止信号循环；
模式切换只切换 QStackedWidget，不修改共享层号；
生产层不存在时才使用 overlay/raw 层号并集作为后备范围。
```

稀疏 preview 处理：

```text
PreviewPanel 不再以当前通道的图片序号作为层号；
当前通道按真实 layerIndex 建立唯一层列表；
目标层缺图时保留 requested layerIndex 并显示“未跨层兜底”；
PreviewOverlayPanel 对同层材料缺失使用相同规则；
禁止最近层、后续层和其他通道层的隐式替代。
```

`MainWindow` 当前只保留一个顶级“预览”页签。报告、曲线、配置仍是顶级页签，待 R2-03 DiagnosticsDock 再调整；R2-01 不提前改变诊断布局。

### 5.5 R2-02 图例与像素探针收口（已实现）

`PreviewWorkspace` 在三种预览模式上方常驻显示统一材料图例、生产协议提示和当前像素探针上下文。图例不是新的材料判定源，W/S/V/空白色块读取 `LayerPreviewDataProvider` 已解析的 `pseudoColors`；RGB 使用真彩通道提示。

生产值与显示值契约：

```text
生产数据：RGBWSV、uint8、black_is_print、0=打印、255=不打印；
显示数据：RGB 真彩色或 W/S/V configurable pseudo color；
显示色只用于人工识别，不写回 TIFF，也不能作为材料语义真源；
真实空白必须由 R=G=B=W=S=V=255 判定，不能仅凭界面白色判定。
```

探针链路：

```text
LayerPreviewPanel 继续直接读取当前 layer 的生产 TIFF；
点击显示坐标后转换为切片原始 y 坐标并读取六通道值；
InterpretPixel 根据各通道是否小于 255 形成打印通道和中文材料语义；
SigPixelProbeChanged 将上下文同步到 PreviewWorkspace；
切换 layer 或 channel 时清空旧探针，避免把上一层结果误认为当前层；
保留 semantic/sourcePolicy 技术字段供 smoke 与工程诊断使用。
```

本任务没有修改 TIFF reader、production package、伪彩生成算法或材料冲突策略。

### 5.6 R2-03 DiagnosticsDock（已实现）

`DiagnosticsDock` 是 `QDockWidget` 的 UI 封装，只负责报告、曲线和日志的承载与折叠，不引入报告解析或业务判断。

所有权：

```text
MainWindow
  DiagnosticsDock (bottom only, default hidden)
    diagnosticsTabs
      ReportPanel
      ChannelChartPanel
      LogPanel
```

交互与加载链路：

```text
MainWindow 中央 mainWorkspaceTabs 只保留 PreviewWorkspace 和 ConfigEditorPanel；
视图菜单复用 QDockWidget::toggleViewAction 显示“诊断区域”；
关闭 dock 只隐藏，不销毁 panel 或清空日志；
DiagnosticsDock::LoadPackage 转发 package 给 ReportPanel 和 ChannelChartPanel；
ProcessRunner 继续向同一个 LogPanel 写 stdout/stderr/exit result；
ReportPanel::warningsChanged 继续回写右侧 warnings_view。
```

R2-03 不读取 OpenVDB utility report，不移动右侧材料工艺/警告区域，也不承担 R2-05 多尺寸最终截图验收。

---

## 6. 布局建议

```text
左侧：模型/Profile/主要操作按钮；
中间：PreviewWorkspace；
右侧：当前层参数、像素探针、图例；
底部：可折叠日志；
右下或底部抽屉：报告、曲线、benchmark、诊断。
```

要求：

```text
1. 场景/Profile 下拉框最小宽度足够显示中文短名；
2. 长路径使用 elide + tooltip；
3. 主操作按钮不被配置路径挤压；
4. 报告/曲线默认不抢占预览空间；
5. 小窗口下允许滚动，不允许控件遮挡。
```

---

## 7. 帮助文本

HelpTextProvider 数据结构：

```json
{
  "modelFill.material": {
    "title": "模型填充材料",
    "description": "用于模型内部非表面纹理区域。",
    "affects": ["RGB", "W", "V"],
    "default": "白墨",
    "doc": "docs/slice/PRD/PRD_12A_彩色纹理材料填充支撑光油策略.md"
  }
}
```

UI 展示：

```text
短说明：tooltip；
详细说明：右侧说明面板；
文档路径：可复制或打开。
```

### 7.1 R1-04 集中帮助元数据（已实现）

`HelpTextProvider` 位于 `apps/slicer_debug_ui/services`，使用只读 C++ 元数据表作为 UI 帮助的单一数据源。当前不增加第二份运行时 JSON，避免帮助文件缺失导致工作台启动失败，也不把 UI 文案引入 `slicer_core`。

每个条目固定提供：

```text
key；
title；
description；
affects；
defaultValue；
productionSafety；
docPath。
```

当前登记 21 个条目，覆盖输入输出、层高、纹理、模型内部填充、白墨、顶部/表面/外侧光油、支撑 placement/internal void、preview、Legacy 生产引擎和 OpenVDB 候选/诊断引擎。

复用关系：

```text
QuickConfigPanel 控件 tooltip <- HelpTextProvider::ToolTip；
SettingHelpPanel 详细说明 <- SettingHelpMetadata::DetailText；
ConfigEditorPanel “设置说明”页签承载 SettingHelpPanel；
UiSmokeTestRunner setting-help-metadata 校验字段、文档和绑定。
```

OpenVDB 条目必须同时显示：默认关闭、非生产、`productionReplacementAllowed=false`、不写生产 RGBWSV。Legacy 条目明确为当前默认生产路径，但仍需通过配置、几何和输出校验。

---

## 8. 风险

| 风险 | 缓解 |
|---|---|
| 隐藏 fixture 后开发者找不到回归配置 | 提供“显示高级/测试配置”开关 |
| UI override 与模板 JSON 冲突 | 生成 config 时写入 effectiveConfigReport |
| 预览整合影响已有功能 | 保留三种模式，但入口统一 |
| OpenVDB 被误认为生产引擎 | UI 标签显示“候选/诊断”，失败时显示 fallback |

---

## 9. 验证

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\Debug\slicer_debug_ui.exe
```

UI smoke：

```text
1. Profile 中文显示完整；
2. 选择模型后一键切片；
3. 修改模型填充材料生成不同 config；
4. 生产层检查/材料叠加/原始调试预览可切换；
5. 报告/曲线抽屉可展开收起；
6. OpenVDB candidate 失败显示明确原因。
```

---

## 10. 增量实现原则

```text
ScenarioRegistry：保留并扩展，不平行重写；
ConfigDocument/QuickConfigPanel：保留编辑能力，新增 SliceSettingsModel/effective config orchestration；
LayerPreviewPanel/PreviewOverlayPanel/PreviewPanel：保留渲染能力，由 PreviewWorkspace 统一状态；
ReportPanel/ChannelChartPanel/LogPanel：保留内容，由 DiagnosticsDock 调整承载位置；
UiSmokeTestRunner：扩展 case，不另建测试程序。
```

## 11. 分阶段技术顺序

### 11.1 R0 Build Lane

先比较并固化兼容工具链、最小 compatibility shim、Qt patch/LTS 升级三条路线。未经决策不得修改第三方 Qt 头文件或直接升级依赖。

### 11.2 R1 Settings Pipeline

```text
Profile template -> SliceSettingsModel -> overrides -> generated config -> ConfigValidator -> slicer_cli
```

运行切片不得再忽略 dirty UI 设置。generated config 必须写入 session 目录并可在 UI 查看 effective summary。

### 11.3 R2 Workspace

PreviewWorkspace 只协调模式、layerIndex、zoom intent 和当前 probe context；各 panel 继续负责各自数据源。DiagnosticsDock 只负责布局，不把业务决策移入 report/view 层。

## 12. 测试边界

R0 使用 fresh build 证明工具链；R1 增加 Profile 和 generated config smoke；R2 增加 shared-layer、diagnostics-collapse 和多尺寸布局 smoke。旧 binary 只可作为历史基线，不能作为 R0 完成证据。

## 13. 冻结实现约束

`DOC_DECISION_12C_UI产品默认值与交互冻结.md` 已关闭初始审查中的产品交互开放项：

```text
ProfileCatalog 在 ScenarioRegistry 上演进，普通层默认四类稳定 Profile；
SliceSettingsModel 必须在运行前生成 session effective config；
生产模型填充不允许 empty；
DiagnosticsDock 默认底部折叠，右侧保留图例和像素探针；
12D 业务算法不得进入 UI；
OpenVDB utility report 只读展示 productionReplacementAllowed=false。
```

具体模板路径和字段映射可在 R1 原子任务内根据当前代码校正，但不得改变上述语义。
