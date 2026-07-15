# DEMO_12C_Qt_UI配置预览验证方案

> 文档版本：v0.2
> 文档状态：DEMO / Stage 12C
> 生成日期：2026-07-05
> 更新日期：2026-07-10

---

## 1. 验证目标

验证 UI 是否能从“多个配置文件入口”收束为“Profile + 设置 + 统一预览工作区”。

---

## 2. 验证场景

### Case 12C-01 Profile 中文显示

步骤：

```text
1. 打开 slicer_debug_ui；
2. 展开场景/Profile；
3. 查看中文短名是否完整显示；
4. 鼠标悬停显示完整说明。
```

通过：

```text
下拉框无截断关键字；
长路径不直接挤占界面；
高级 fixture 默认不干扰普通 Profile。
```

### Case 12C-02 一键切片配置生成

步骤：

```text
1. 选择彩色纹理甲片 Profile；
2. 选择模型；
3. 设置模型填充材料；
4. 设置支撑 placement；
5. 运行切片。
```

通过：

```text
generated config 中包含 UI override；
输出 package 正常；
UI 显示配置摘要。
```

### Case 12C-03 统一预览工作区

步骤：

```text
1. 切换生产层检查；
2. 切换材料叠加；
3. 切换原始调试预览；
4. 使用像素探针检查同一 layer。
```

通过：

```text
三种模式共享 layerIndex；
图例显示 RGB/W/S/V；
原始调试预览明确标注不是生产真源。
```

### Case 12C-04 报告曲线诊断抽屉

步骤：

```text
1. 打开报告；
2. 打开曲线；
3. 折叠诊断区域；
4. 调整窗口大小。
```

通过：

```text
报告/曲线不遮挡主预览；
小窗口无控件重叠。
```

### Case 12C-05 OpenVDB 候选提示

步骤：

```text
1. 选择 OpenVDB candidate；
2. 对真实 OBJ 模型运行；
3. 观察成功或失败提示。
```

通过：

```text
显示候选/诊断标签；
失败时显示 failureReason；
提示可回退 legacy；
不把 non-production 输出标为 production。
```

---

## 3. 验证记录

每轮 UI 验证记录：

```text
build type；
Profile；
模型路径；
生成 config；
输出 package；
关键截图；
日志退出码；
手动检查结论。
```

---

## 4. R0 Fresh Build Gate

必须从新 build dir 配置和构建，不允许只运行历史 binary：

```text
fresh configure PASS；
slicer_debug_ui Debug build PASS；
--self-test PASS；
scenario-registry PASS；
layer-preview-load PASS；
overlay-load-real PASS。
```

若 Qt/MSVC 不兼容，记录 compiler、Qt 版本、错误位置和候选路线，R1/R2 保持未准入。

## 5. R1 Effective Config Gate

```text
选择 Profile 后修改模型填充、支撑 placement、表面/外侧光油；
不保存原 fixture JSON；
运行切片生成 session config；
generated config 包含所有 override；
原模板文件内容和 git status 不变化。
```

## 6. R2 Workspace Gate

三种预览模式必须在同一 `layerIndex` 上切换。验证窗口尺寸至少包含 1440x900、1280x720、1024x768；报告、曲线和日志折叠后不得遮挡主预览。

OpenVDB 显示必须包含 `utility/candidate`、`productionReplacementAllowed=false` 和 legacy fallback，不得显示为默认生产成功路径。

### 6.1 R2-02 图例与探针 Gate

```text
图例常驻显示 RGB 模型、W 白墨填充、S 支撑、V 光油/填充和真实空白；
协议说明同时包含 RGBWSV uint8、black_is_print、0=打印、255=不打印；
图例明确声明显示伪彩不等于 TIFF 生产值；
生产层点击探针显示 R/G/B/W/S/V、打印通道、材料语义和 layerIndex；
切换层或通道后旧探针被清除；
使用 output/UiSmokeLayerPreview 覆盖 RGB、W、S、V 和真实空白五类像素。
```

自动化命令：

```powershell
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-legend-probe-context --package output\UiSmokeLayerPreview
```

### 6.2 R2-03 DiagnosticsDock Gate

```text
启动后诊断区域默认隐藏，主预览不被日志永久压缩；
中央页签只保留“预览”和“配置”；
“视图/诊断区域”可以展开和收起底部 dock；
诊断区精确包含“报告”“曲线”“日志”；
三个既有 panel 各只有一个实例；
展开/收起不改变 PreviewWorkspace 当前真实 layerIndex；
输出包加载后曲线仍包含层统计。
```

自动化命令：

```powershell
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case diagnostics-collapse --package output\UiSmokeLayerPreview
```

### 6.3 R2-04 OpenVDB Utility 摘要 Gate

```text
有效 OFF 报告显示 OpenVDB 当前构建不可用，四项 utility 不得显示 pass；
有效 ON 报告显示壳层/拓扑 Utility 验证通过（非生产），不显示生产切片通过；
两类有效报告都显示 productionReplacementAllowed=false 和 Legacy 默认生产路径；
promote 只能翻译为推进辅助 Utility，不能翻译为替代生产引擎；
独立 JSON 可通过 ReportPanel 显式加载，源文件和当前 package 不改变；
未知 utility schema 显示不支持；
replacement=true 或安全 outputPolicy 非 false 时显示报告无效；
报告加载不改变 PreviewWorkspace 当前真实 layerIndex。
```

自动化命令：

```powershell
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case openvdb-utility-summary
```

Smoke 使用临时 ON/OFF/非法 JSON fixture，不依赖本机 OpenVDB ON 构建。真实 utility probe 由 `scripts/run_12b_r2_openvdb_sdf_utility.ps1` 单独验证。
