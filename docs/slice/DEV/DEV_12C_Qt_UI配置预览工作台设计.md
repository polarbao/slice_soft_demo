# DEV_12C_Qt_UI配置预览工作台设计

> 文档版本：v0.1
> 文档状态：DEV / Stage 12C
> 生成日期：2026-07-05
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

---

## 3. Profile 元数据

建议为配置增加轻量 metadata，不直接重命名大量 JSON：

```json
{
  "id": "material_rgb_white_varnish",
  "displayName": "彩色纹理甲片 - RGB + 白墨 + 光油",
  "category": "production",
  "visibility": "normal",
  "description": "用于标准 OBJ/MTL/PNG 彩色甲片模型。",
  "configPath": "samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json",
  "tags": ["obj", "texture", "rgb", "white", "varnish"]
}
```

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
