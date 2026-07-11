# DOC_AUDIT_12C-R0-03 现有 Qt 布局与组件复用基线

> 文档状态：Implemented Baseline / 12C-R0-03
> 日期：2026-07-12

## 1. 审查目标

在进入 Profile/Settings 与统一预览改造前，固定现有 `MainWindow`、预览、配置、报告和日志组件的职责，记录多尺寸布局缺口，避免 R1/R2 重写已有能力。

## 2. 当前布局

```text
MainWindow
  vertical splitter
    horizontal splitter
      left: project/profile/path/run panel
      center: layer/report/chart/config/overlay/raw-preview tabs
      right: material parameters/warnings/compare tabs
    bottom: LogPanel
```

当前静态约束：

```text
left minimum width = 320 px；
center minimum width = 620 px；
right minimum width = 360 px；
log minimum height = 140 px；
left panel 未放入 QScrollArea；
主窗口初始 resize = 1440 x 900。
```

三列最小宽度在不计 splitter handle 和 layout margin 前已达到 1300 px。左侧长路径、说明和十个操作按钮共同决定较大的垂直 minimum size。

## 3. 组件职责与复用边界

| 组件 | 当前职责 | 后续处理 |
|---|---|---|
| `MainWindow` | ToolPaths、进程生命周期、场景选择、package 装载与总体编排 | 保留编排职责；不继续堆积材料算法 |
| `ScenarioRegistry` | 场景索引、visibility、中文显示信息 | R1 直接扩展为 Profile metadata 真源，不平行重写 |
| `ConfigDocument` | JSON 载入、修改、保存、dirty 状态 | R1 复用；由设置模型生成 effective config |
| `ConfigEditorPanel/QuickConfigPanel` | 配置编辑和常用参数入口 | 保留编辑器；后续由 Settings DTO 协调 |
| `LayerPreviewPanel` | 生产 TIFF、RGBWSV 通道、像素探针 | R2 作为 ProductionLayerView 复用，不重写解析/渲染 |
| `PreviewOverlayPanel` | 同层 RGB/W/S/V 伪彩合成 | R2 作为 MaterialOverlayView 复用 |
| `PreviewPanel` | preview 目录原始调试图浏览 | R2 作为 RawPreviewView 复用，并保持非生产真源说明 |
| `ReportPanel` | manifest/report 摘要和原始 JSON | 移入 DiagnosticsDock，不重写 ReportLoader |
| `ChannelChartPanel` | 分层通道统计曲线 | 移入 DiagnosticsDock，保留绘制逻辑 |
| `MaterialProcessPanel` | 材料工艺摘要 | 保留右侧上下文或进入诊断区，不承担业务决策 |
| `LogPanel` | 命令、stdout/stderr、退出码 | R2 改为可折叠诊断区域，不重写日志语义 |

## 4. 多尺寸基线

使用 `build-12c-ui` fresh binary，在双屏 Windows 环境中请求 1440x900、1280x720、1024x768，并通过 `PrintWindow` 采集窗口内容。截图保存在本地忽略目录 `output/12c_r0_03_layout_print`，不作为仓库生产资产提交。

| 请求尺寸 | 实际物理窗口 | 观察 |
|---|---|---|
| 1440x900 | 1363x1214 | 受 DPI 与 minimum size 共同影响，目标高度不能满足，右侧空间紧张 |
| 1280x720 | 1203x1214 | 高度被强制抬升，右侧诊断区仅部分可见 |
| 1024x768 | 1097x1214 | 三列无法同时容纳，路径文本截断，右侧诊断区明显裁切 |

物理宽度受两块显示器 DPI 缩放影响，不应用作固定 UI 常量；三次采样均无法降低到目标高度，且窄窗口右侧裁切，是可重复的结构性结论。

## 5. 已确认缺口

```text
1. 左侧项目与运行面板无滚动容器，内容高度直接抬高主窗口 minimum size；
2. 320 + 620 + 360 的三列最小宽度使 1280 和 1024 宽度无法成立；
3. 报告、曲线、配置、叠加、原始预览全部位于中心顶级 tab，主工作流入口过多；
4. LogPanel 常驻且 minimumHeight=140，小高度窗口无法折叠释放空间；
5. 右侧参数/诊断/工艺对比 minimumWidth=360，窄窗口下被裁切；
6. 长本地路径依赖换行占用大量垂直空间，尚未统一 elide + tooltip。
```

## 6. 后续修复边界

R1 只处理 Profile、Settings、generated effective config 和帮助元数据，不在 R1 重写 preview 或报告组件。

R2 通过 `PreviewWorkspace` 协调三个已有 preview panel，通过可折叠 `DiagnosticsDock` 承载报告、曲线和日志；同时为左侧长内容提供滚动/摘要方案，并加入真正的多尺寸验收 smoke。

## 7. 准入结论

```text
12C-R0-03：COMPLETE
12C-R0：COMPLETE
现有组件复用边界：FROZEN
当前布局小尺寸验收：KNOWN FAIL / 待 R2 修复
下一任务：12C-R1-01 Profile Metadata 收口
```
