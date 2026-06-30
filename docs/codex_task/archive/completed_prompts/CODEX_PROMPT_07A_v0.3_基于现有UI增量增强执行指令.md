# CODEX_PROMPT_07A_v0.3_基于现有UI增量增强执行指令

> 文档版本：v0.3  
> 用途：复制给 VS Code Codex / Cursor / Copilot Chat 执行  
> 适用阶段：07A：Qt 参数编辑与 Profile 可视化增强  
> 前提：07 阶段代码已经上传，`apps/slicer_debug_ui` 已存在。  
> 核心原则：本阶段是基于现有 UI 的增量增强，不是重建 UI 工程。  
> 建议提交目录：`docs/slicer/`

---

## 0. 阶段边界

当前阶段是：

```text
07A：Qt 参数编辑与 Profile 可视化增强
```

你的任务不是重新创建 `slicer_debug_ui`，而是在现有 07 代码基础上增量增强。

必须保留当前已经存在的基础结构：

```text
apps/slicer_debug_ui/
  CMakeLists.txt
  main.cpp
  MainWindow.*
  services/
    PackageLoader.*
    ProcessRunner.*
    ReportLoader.*
    ToolPaths.*
  widgets/
    LogPanel.*
    MaterialProcessPanel.*
    PreviewPanel.*
    ReportPanel.*
```

不要删除、替换或重写现有基础功能。

---

## 1. 必须先阅读的文档顺序

请严格按以下顺序阅读：

```text
docs/slicer/REPORT_07_Qt调试UI当前实现状态.md
docs/slicer/DOC_REVIEW_07A_基于当前代码的文档修订判断.md
docs/slicer/DOC_DECISION_07A_v0.2_基于现有slicer_debug_ui增量增强.md
docs/slicer/ROADMAP_v1.4_REPORT07后续路线_Qt参数编辑与Profile可视化.md
docs/slicer/PRD_07A_Qt参数编辑与Profile可视化增强.md
docs/slicer/DEV_07A_Qt参数编辑与Profile可视化设计.md
docs/slicer/DEMO_07A_Qt参数编辑与Profile可视化验证方案.md
docs/slicer/TASKS_07A_v0.2_基于当前代码的修订任务清单.md
```

如果部分文件不存在，先不要改代码，先在 `docs/slicer/REPORT_07A_执行前检查.md` 中记录缺失文件列表，然后继续基于已有代码和本指令执行。

---

## 2. 必须先检查的代码路径

修改前请先检查这些文件：

```text
CMakeLists.txt
apps/slicer_debug_ui/CMakeLists.txt
apps/slicer_debug_ui/main.cpp
apps/slicer_debug_ui/MainWindow.h
apps/slicer_debug_ui/MainWindow.cpp

apps/slicer_debug_ui/services/PackageLoader.h
apps/slicer_debug_ui/services/PackageLoader.cpp
apps/slicer_debug_ui/services/ProcessRunner.h
apps/slicer_debug_ui/services/ProcessRunner.cpp
apps/slicer_debug_ui/services/ReportLoader.h
apps/slicer_debug_ui/services/ReportLoader.cpp
apps/slicer_debug_ui/services/ToolPaths.h
apps/slicer_debug_ui/services/ToolPaths.cpp

apps/slicer_debug_ui/widgets/LogPanel.h
apps/slicer_debug_ui/widgets/LogPanel.cpp
apps/slicer_debug_ui/widgets/MaterialProcessPanel.h
apps/slicer_debug_ui/widgets/MaterialProcessPanel.cpp
apps/slicer_debug_ui/widgets/PreviewPanel.h
apps/slicer_debug_ui/widgets/PreviewPanel.cpp
apps/slicer_debug_ui/widgets/ReportPanel.h
apps/slicer_debug_ui/widgets/ReportPanel.cpp
```

重点确认：

```text
1. MainWindow 当前 tab 布局；
2. ProcessRunner 当前 QProcess 执行方式；
3. PackageLoader 当前 package/report/preview 发现逻辑；
4. MaterialProcessPanel 当前只是 report viewer，不是 editor；
5. PreviewPanel 当前 PNG/PPM 查看逻辑；
6. ReportPanel 当前 raw JSON / warnings 显示逻辑；
7. CMake 当前 target 已存在。
```

---

## 3. 本阶段必须保持不变的内容

不得修改 slicer_core 的输出协议语义。

必须保持：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
MaterialRoleMapping 语义不变
MaterialPolicy 语义不变
MaterialProcessProfile 语义不变
S support 仍由 Support pipeline 独立生成
```

不得重写：

```text
slicer_core
slicer_cli
rip_reader_test
run_regression.ps1 的既有语义
compare_material_profiles.ps1 的既有语义
```

除非只是为了读取报告或 UI 展示补充字段。

---

## 4. 本阶段必须新增的代码能力

### 4.1 ConfigDocument

新增：

```text
apps/slicer_debug_ui/services/ConfigDocument.h
apps/slicer_debug_ui/services/ConfigDocument.cpp
```

职责：

```text
加载 JSON config
保留未知字段
提供 get/set helper
维护 dirty 状态
支持 Save
支持 Save As
支持 Revert
保存前做基础合法性检查
```

建议接口：

```cpp
class ConfigDocument : public QObject {
    Q_OBJECT
public:
    bool load(const QString& path);
    bool save(QWidget* parent = nullptr);
    bool saveAs(const QString& path, QWidget* parent = nullptr);
    QJsonValue value(const QStringList& path) const;
    void setValue(const QStringList& path, const QJsonValue& value);
    bool isDirty() const;
    QString path() const;
    QString errorString() const;

signals:
    void changed();
    void dirtyChanged(bool dirty);
    void validationChanged(QStringList warnings, QStringList errors);
};
```

### 4.2 ConfigValidator

新增：

```text
apps/slicer_debug_ui/services/ConfigValidator.h
apps/slicer_debug_ui/services/ConfigValidator.cpp
```

第一版只做轻量校验：

```text
input.modelPath 不为空
output.packageDir 不为空
output.storageMode in stripped/tiled
materialRoleMapping rule.role 合法
materialPolicy.varnish.topLayers >= 0
materialProcessProfile.varnish.topLayers >= 0
support.mode 合法
preview.interval > 0
```

非法 config 禁止保存，warning 允许保存但需显示。

---

## 5. 本阶段必须新增的 UI Widgets

### 5.1 ConfigEditorPanel

新增：

```text
apps/slicer_debug_ui/widgets/ConfigEditorPanel.h
apps/slicer_debug_ui/widgets/ConfigEditorPanel.cpp
```

职责：

```text
承载 config editor 总入口
显示当前 config path
显示 dirty 状态
Save / Save As / Revert / Validate
聚合各 editor widget
```

### 5.2 MaterialProcessProfileEditor

新增：

```text
apps/slicer_debug_ui/widgets/MaterialProcessProfileEditor.h
apps/slicer_debug_ui/widgets/MaterialProcessProfileEditor.cpp
```

必须支持编辑：

```text
materialProcessProfile.enabled
materialProcessProfile.name
materialProcessProfile.target
materialProcessProfile.rgb.enabled
materialProcessProfile.white.enabled
materialProcessProfile.white.coverage
materialProcessProfile.white.expandPx
materialProcessProfile.white.shrinkPx
materialProcessProfile.varnish.enabled
materialProcessProfile.varnish.topLayers
materialProcessProfile.support.expected
materialProcessProfile.validation.requireRgbPixels
materialProcessProfile.validation.requireWhitePixels
materialProcessProfile.validation.requireVarnishPixels
materialProcessProfile.validation.requireSupportPixels
```

### 5.3 MaterialPolicyEditor

新增：

```text
apps/slicer_debug_ui/widgets/MaterialPolicyEditor.h
apps/slicer_debug_ui/widgets/MaterialPolicyEditor.cpp
```

必须支持编辑：

```text
materialPolicy.enabled
materialPolicy.rgb.enabled
materialPolicy.rgb.source
materialPolicy.white.enabled
materialPolicy.white.mode
materialPolicy.white.layers
materialPolicy.white.value
materialPolicy.varnish.enabled
materialPolicy.varnish.mode
materialPolicy.varnish.topLayers
materialPolicy.varnish.value
materialPolicy.conflictPolicy
```

### 5.4 MaterialRoleMappingEditor

新增：

```text
apps/slicer_debug_ui/widgets/MaterialRoleMappingEditor.h
apps/slicer_debug_ui/widgets/MaterialRoleMappingEditor.cpp
```

必须支持：

```text
materialRoleMapping.enabled
materialRoleMapping.defaultRole
materialRoleMapping.allowInputSupportMaterial
materialRoleMapping.rules
```

rules 表格列：

```text
matchNameContains | role | delete
```

role 可选值：

```text
rgb
white
varnish
ignore
support_candidate
support
```

### 5.5 SupportEditor

新增：

```text
apps/slicer_debug_ui/widgets/SupportEditor.h
apps/slicer_debug_ui/widgets/SupportEditor.cpp
```

第一版支持：

```text
support.enabled
support.mode
support.minIslandAreaPx
support.xyDilationPx
support.connectivity
```

---

## 6. 图表与 Overlay

### 6.1 ChannelChartPanel

新增：

```text
apps/slicer_debug_ui/widgets/ChannelChartPanel.h
apps/slicer_debug_ui/widgets/ChannelChartPanel.cpp
```

读取：

```text
reports/material_process_report.json
```

显示：

```text
per-layer RGB printPixels
per-layer W printPixels
per-layer V printPixels
per-layer S printPixels
```

要求：

```text
1. 使用 QPainter 自绘，不强制引入 Qt Charts；
2. 支持 RGB/W/V/S channel checkbox；
3. 支持 hover layer index；
4. package reload 后刷新；
5. material_process_report 缺失时显示 warning，不崩溃。
```

### 6.2 PreviewOverlayPanel

新增：

```text
apps/slicer_debug_ui/widgets/PreviewOverlayPanel.h
apps/slicer_debug_ui/widgets/PreviewOverlayPanel.cpp
```

能力：

```text
single channel preview
RGB + W overlay
RGB + V overlay
RGB + S overlay
layer slider
zoom
fit to window
```

数据源策略：

```text
1. 优先读取 preview_report.json 中的 layer/channel metadata；
2. 如果 preview_report.json 不存在，则 fallback 到当前 preview 目录扫描与文件名 token 分类；
3. 不重新运行 slicer；
4. 只读取现有 preview 图像并做 QPainter alpha blend。
```

---

## 7. MainWindow 集成方式

不得重写 MainWindow。

在现有 MainWindow 上增量修改：

当前 center tabs 已有：

```text
Preview
Reports
```

新增：

```text
Config
Charts
Overlay
```

当前 right tabs 已有：

```text
Material
Warnings
Compare
```

可新增：

```text
Profile Editor
```

要求：

```text
1. 原有 Preview / Reports / Material / Warnings / Compare / Log 功能不丢失；
2. Load Package 后同时刷新 ReportPanel / PreviewPanel / MaterialProcessPanel / ChannelChartPanel / PreviewOverlayPanel；
3. Run Slicer 成功后继续自动 load package；
4. Compare Profiles 仍然可用。
```

---

## 8. CMake 操作要求

当前 target 已存在：

```text
apps/slicer_debug_ui/CMakeLists.txt
```

本阶段只允许增量添加源文件：

```text
services/ConfigDocument.*
services/ConfigValidator.*
widgets/ConfigEditorPanel.*
widgets/MaterialProcessProfileEditor.*
widgets/MaterialPolicyEditor.*
widgets/MaterialRoleMappingEditor.*
widgets/SupportEditor.*
widgets/ChannelChartPanel.*
widgets/PreviewOverlayPanel.*
```

不要新建第二个 UI target。

根 CMake 中的逻辑必须保持：

```text
BUILD_SLICER_DEBUG_UI
Qt5 Widgets not found 时跳过 UI，不影响 CLI 构建
```

---

## 9. 执行操作顺序

请按这个顺序执行，不要跳跃式大改：

```text
Step 1：确认现有 07 UI target 可构建。
Step 2：新增 ConfigDocument / ConfigValidator，先不接 UI。
Step 3：新增 ConfigEditorPanel，并能加载当前 config。
Step 4：新增 MaterialProcessProfileEditor。
Step 5：新增 MaterialPolicyEditor。
Step 6：新增 MaterialRoleMappingEditor。
Step 7：新增 SupportEditor。
Step 8：接入 Save / Save As / Revert / Validate。
Step 9：新增 ChannelChartPanel，读取 material_process_report。
Step 10：新增 PreviewOverlayPanel，读取 preview images。
Step 11：接入 MainWindow tabs。
Step 12：增强 --self-test。
Step 13：运行 UI demo 验证。
Step 14：运行 quick regression。
Step 15：生成 REPORT_07A。
```

---

## 10. 必须执行的验证命令

Windows / PowerShell 下执行：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_regression.ps1 -Mode quick
```

建议额外执行：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\nail_rgb_white_varnish_top2.json
.\build\Debug\rip_reader_test.exe --package output\NailRgbWhiteVarnishTop2 --summary
```

如本地实际路径为：

```text
build/Debug/slicer_debug_ui.exe
```

可在报告中说明实际生成路径，但文档默认按当前 CMake 子目录路径记录：

```text
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
```

---

## 11. 必须验证的 UI 行为

### 11.1 Config 编辑

```text
打开 samples/configs/material_process/nail_rgb_white_varnish_top2.json
修改 materialProcessProfile.varnish.topLayers = 3
Save As samples/configs/material_process/ui_generated_top3.json
运行 slicer
加载 output package
确认 V activeLayerIndices 数量变化
```

### 11.2 MaterialRoleMapping 编辑

```text
打开 OBJ/MTL material mapping 配置
新增 rule:
  matchNameContains = clear
  role = varnish
Save As
运行 slicer
确认 material_role_mapping_report.rules 包含 clear
```

### 11.3 Chart

```text
打开 output/NailRgbWhiteVarnishTop3
Charts tab 显示 RGB/W/V/S 曲线
```

### 11.4 Overlay

```text
打开 output/NailRgbWhiteVarnishTop3
Overlay tab 显示 RGB + V 或 RGB + S overlay
layer slider / zoom / fit 可用
```

### 11.5 原有能力回归

```text
Run Slicer 可用
Run RIP Summary 可用
Run Quick Regression 可用
Compare Profiles 可用
Preview tab 可用
Reports tab 可用
Material summary 可用
LogPanel 可用
```

---

## 12. 完成后必须生成的文件

完成实现后必须新增：

```text
docs/slicer/REPORT_07A_Qt参数编辑与Profile可视化当前实现状态.md
```

报告必须包含：

```text
1. 实现日期；
2. 阶段状态；
3. 新增 services 列表；
4. 新增 widgets 列表；
5. MainWindow 集成方式；
6. Config 编辑支持范围；
7. Save / Save As / Validate 行为；
8. ChannelChartPanel 支持范围；
9. PreviewOverlayPanel 支持范围；
10. self-test 结果；
11. UI demo 验证结果；
12. quick regression 结果；
13. 未实现项；
14. 下一阶段建议。
```

报告中必须明确：

```text
07A 未修改 slicer_core 输出协议。
07A 未修改 MaterialPolicy / MaterialRoleMapping / MaterialProcessProfile 执行语义。
07A 只是 UI 增强。
```

---

## 13. 禁止事项

禁止：

```text
1. 删除现有 MainWindow 功能；
2. 替换 ProcessRunner；
3. 重写 PackageLoader / ReportLoader；
4. 把 MaterialProcessPanel 改成编辑器；
5. 修改 slicer_core 输出协议；
6. 修改 p0.rgbwsv.2 语义；
7. 修改 MaterialPolicy 实际输出语义；
8. 引入设备通信；
9. 引入 RIP 半色调；
10. 引入 OpenVDB；
11. 引入生产级任务系统。
```

---

## 14. 可接受的阶段性降级

如果时间不足，优先保证：

```text
ConfigDocument
MaterialProcessProfileEditor
Save As
ChannelChartPanel
PreviewOverlayPanel
self-test
REPORT_07A
```

MaterialPolicyEditor / MaterialRoleMappingEditor / SupportEditor 可以先做基础字段，不必覆盖所有高级字段，但必须在 REPORT 中说明未覆盖范围。
