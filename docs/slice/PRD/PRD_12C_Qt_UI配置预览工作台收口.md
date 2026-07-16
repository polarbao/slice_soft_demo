# PRD_12C_Qt_UI配置预览工作台收口

> 文档版本：v0.2
> 文档状态：PRD / Stage 12C
> 生成日期：2026-07-05
> 更新日期：2026-07-10
> 适用范围：Qt 调试 UI 的配置入口、Profile 管理、层预览、叠加预览、原始预览、报告曲线布局

---

## 1. 背景

当前 Qt UI 已能执行模型导入、一键切片、OpenVDB 诊断/候选切片、配置编辑、层预览、叠加预览、报告和曲线。但随着 00-11B 阶段累积，界面暴露了大量历史配置和调试入口：

```text
1. 配置文件数量多，普通用户难以判断该选哪个；
2. 部分配置只是阶段 fixture，不应长期出现在生产选择中；
3. 层预览、叠加预览、原始预览三类入口含义不够明确；
4. 报告和曲线占用主要工作区，和预览任务混在一起；
5. 配置界面说明不足，用户不知道选项会改变哪些通道；
6. 场景/Profile 下拉框显示不完整，影响使用。
```

12C 目标是把调试 UI 收束成“可用的软件工作台”，而不是继续让用户直接面对大量 JSON。

---

## 2. 产品目标

```text
1. 用 Profile/模板/设置面板替代普通用户直接选择大量配置文件；
2. 保留高级 fixture 和阶段验证配置，但默认隐藏或分组到高级模式；
3. 将层预览、叠加预览、原始预览整合为统一预览工作区；
4. 报告、曲线、诊断移入可折叠/可切换的辅助区域；
5. 每个关键设置提供中文短说明和文档链接；
6. UI 能支持 12A 的模型填充、支撑、光油壳层策略；
7. UI 能支持 12B 的 legacy/OpenVDB/benchmark 选择，但明确 OpenVDB candidate 属性。
```

---

## 3. 配置分层

### 3.1 普通用户层

只显示长期 Profile：

```text
彩色纹理甲片 - RGB + 白墨填充 + 下表面支撑
彩色纹理甲片 - 全实体 RGB + 下表面支撑（无白墨，兼容模式）
彩色纹理甲片 - RGB + 光油填充 + 下表面支撑
单材料浮雕 - 白墨/光油
调试 - 生产 RGB 检查
```

### 3.2 高级用户层

显示：

```text
支撑 placement；
internal void support；
outer varnish shell；
texture surface layers；
engine legacy/openvdb candidate；
preview mode；
report detail level。
```

### 3.3 测试 fixture 层

保留但默认隐藏：

```text
00-11B 阶段回归配置；
坏包负向测试；
OpenVDB non-production fixture；
协议兼容 fixture；
历史样例。
```

---

## 4. 统一预览工作区

预览不再按入口割裂，而是统一为一个工作区，内部切换模式：

```text
生产层检查：
  直接读取 RGBWSV TIFF，显示真实生产像素和六通道探针。

材料叠加：
  将 RGB/W/S/V 按可配置伪彩叠加，用于看支撑、白墨、光油关系。

原始预览：
  显示 slicer 生成的 preview 文件，作为调试输出，不作为生产真源。
```

UI 文案要求：

```text
层预览 => 生产层检查
叠加预览 => 材料叠加
原始预览 => 原始调试预览
```

---

## 5. 报告和曲线

报告和曲线不是主要切片操作入口，应调整为：

```text
1. 右侧或底部“诊断”抽屉；
2. 支持一键展开/收起；
3. 默认显示当前层关键统计；
4. 详细 JSON 和曲线在诊断页签中查看；
5. 不遮挡主预览。
```

---

## 6. 设置说明

每个设置至少包含：

```text
中文名称；
一句话说明；
影响通道；
默认值；
是否生产安全；
相关文档路径。
```

示例：

```text
模型填充材料：
用于模型内部非表面纹理区域。选择白墨会写 W 通道，选择光油会写 V 通道。生产 Profile 不允许选择空填充。
影响：W/V/RGB。
默认：白墨。
文档：PRD_12A。
```

---

## 7. OpenVDB UI 要求

OpenVDB 相关按钮和选项必须明确：

```text
1. OpenVDB 当前为候选/诊断能力；
2. 不等同于默认生产切片；
3. 失败时应显示失败原因和 fallback 建议；
4. 若 outputSemanticsComparable=false，不应提示为可替代输出；
5. benchmark 模式不写图片，避免用户误解耗时。
```

---

## 8. 成功标准

```text
1. 普通用户可通过“导入模型 -> 选择 Profile -> 设置材料/支撑/光油 -> 切片 -> 预览”完成流程；
2. 不需要手动理解 samples/configs 中全部 JSON；
3. 预览入口减少且含义清楚；
4. 报告/曲线不遮挡主操作；
5. 配置说明在 UI 和用户手册中一致；
6. 仍保留高级/测试入口给开发回归使用。
```

---

## 9. 当前基线与增量范围

当前已存在 ScenarioRegistry、QuickConfigPanel、LayerPreviewPanel、PreviewOverlayPanel、PreviewPanel、报告、曲线和日志 panel。12C 不从零实现这些能力。

12C 增量重点：

```text
1. 先恢复 fresh Qt UI build；
2. 把场景元数据收口为稳定 Profile，而不是继续增加普通场景；
3. 让 UI 修改通过 generated effective config 直接参与本次切片；
4. 统一三个预览模式的 layerIndex 和工作区；
5. 把报告、曲线、日志收进可折叠诊断区域；
6. 集中管理中文说明和文档链接；
7. 展示 12B-R2 utility role，但不提升为生产引擎。
```

## 10. 12C 非目标

```text
不修改切片算法；
不修改 RGBWSV 协议；
不删除 regression fixture；
不实现 12D material closure 算法；
不把 OpenVDB 设为默认 production engine；
不进行与工作台收口无关的全量 Qt 类重命名。
```

## 11. 阶段准入

12C-R0 必须先解决 Qt 5.15.2 / MSVC 19.51 fresh build blocker。只有 fresh UI build、自测和关键 smoke 可复现后，才允许进入 R1/R2 UI 结构调整。

## 12. 已冻结的产品默认值

以下默认行为以 `DOC_DECISION_12C_UI产品默认值与交互冻结.md` 为准：

```text
普通用户默认显示五类稳定 Profile；
运行切片时自动生成 session effective config，不覆盖原始 template/fixture；
生产 Profile 的模型内部填充不得为空，默认白墨；
DiagnosticsDock 默认位于底部并折叠；
12D 只允许后续只读展示，不阻断 12C-R0/R1；
OpenVDB 始终保持 utility/candidate 和默认关闭。
```
