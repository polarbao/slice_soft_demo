# DOC PREP 13D-04 响应式与阶段收口准备

> 文档状态：COMPLETE
> 版本：v1.0
> 日期：2026-07-29

## 1. 目标

在不改变业务能力的前提下完成工作台尺寸、持久化、中文文本、键盘顺序和阶段证据收口。

## 2. 布局状态合同

```text
QSettings schema：ui.layout.version=13d.1；
保存窗口 geometry、dock state、主 splitter 尺寸和最后检查器页；
旧版本、损坏 QByteArray、屏幕外尺寸回退安全默认值；
不得把布局状态写入切片 JSON；
不得用绝对屏幕坐标作为跨设备配置。
```

## 3. 验证矩阵

```text
1280x720 / 1440x900 / 1920x1080；
100% / 150% DPI；
空场景 / 单模型 / 三模型 / 22 实例摘要；
idle / blocked / slicing / completed / failed；
新安装 / 合法状态 / 旧版本 / 损坏状态。
```

## 4. 收口产物

新增 `workbench-layout-restore`、`workbench-1280x720` Smoke，更新用户手册、TASKS、阶段总览并生成
`REPORT_13D_Qt工作台布局收口当前状态.md`。完成后才恢复 12E-09A-03 推荐入口。

## 5. 准入结论

13D-03 已于 2026-07-29 通过编译、`workbench-project-diagnostics`、布局尺寸、诊断折叠、场景批量
导入和场景切片 Smoke。13D-04 可进入开发。

## 6. 实现结果

```text
ui.layout.version=13d.1 的 QSettings 保存/恢复已实现；
合法 geometry/dock/splitter/检查器状态可恢复；
旧 schema、损坏状态和屏幕外窗口回退安全默认布局；
项目区、上下文检查器和诊断区均有视图菜单入口与快捷键；
顶部主动作定义 Ctrl+O、Ctrl+S、Ctrl+Return、Esc 和稳定 tab order；
1280x720 的 100%/150% 字体尺度 Smoke 通过；
用户手册与 Stage 13/12X 总览已同步。
```

13D-01..04 已完成，阶段 Gate 关闭。
