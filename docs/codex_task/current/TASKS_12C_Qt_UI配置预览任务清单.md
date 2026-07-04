# TASKS_12C_Qt_UI配置预览任务清单

> 文档版本：v0.1
> 文档状态：Task List / Stage 12C
> 生成日期：2026-07-05

---

## 任务边界

12C 处理 Qt UI 的配置、Profile、预览、报告曲线和说明文档。
不改变底层 RGBWSV 协议，不实现新切片算法。

---

## 原子任务

### Task 12C-01 ProfileCatalog 设计落地

状态：PENDING

内容：

```text
为现有配置建立中文 displayName/category/visibility/description。
```

验证：

```text
UI 下拉框按普通/高级/fixture 分组显示。
```

### Task 12C-02 设置面板替代常用 JSON 选择

状态：PENDING

内容：

```text
将材料填充、支撑、光油、预览、引擎选项放入 UI 设置页。
```

验证：

```text
generated config 正确包含 UI override。
```

### Task 12C-03 预览入口整合

状态：PENDING

内容：

```text
将层预览/叠加预览/原始预览统一到 PreviewWorkspace。
```

验证：

```text
三种模式共享 layerIndex，且文案清晰。
```

### Task 12C-04 报告曲线诊断抽屉

状态：PENDING

内容：

```text
报告、曲线、日志、benchmark 摘要移动到可折叠诊断区域。
```

验证：

```text
主预览不被遮挡，小窗口无重叠。
```

### Task 12C-05 配置项中文说明

状态：PENDING

内容：

```text
为关键配置项提供 tooltip、说明面板和文档路径。
```

验证：

```text
材料/支撑/光油/OpenVDB 选项均有说明。
```

### Task 12C-06 OpenVDB 候选状态提示

状态：PENDING

内容：

```text
UI 中明确 OpenVDB 是候选/诊断能力，并显示失败原因。
```

验证：

```text
OpenVDB 失败时不误导用户为生产失败，给出 fallback legacy 建议。
```

---

## 完成标准

```text
1. 普通用户能不接触大量 JSON 完成标准切片；
2. 高级/fixture 配置仍可访问；
3. 预览工作区语义清晰；
4. UI 说明与 user guide 一致。
```
