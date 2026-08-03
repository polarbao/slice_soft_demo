# REPORT_12E-09D-04 Qt 生产控件当前状态

> 状态：COMPLETE
> 日期：2026-08-03

## 1. 已实现

右侧“切片设置”根据当前生产 Profile 显示互斥控件：

```text
Legacy：顶面纹理层数和等效 Z 厚度；
Global：partial_shell/all_texture、请求宽度、有效宽度和后端；
单材料浮雕：白墨 W 或光油 V；
不支持或锁定 Profile：只读并显示原因。
```

诊断宽度保留在“预检与诊断/纹理诊断”，没有写生产配置的接口。修改生产值后 package 标记 stale，保存、回读和 Profile 切换均保持各自身份。

## 2. 验证

```powershell
build-slicesoft/main/apps/slicer_debug_ui/Release/slicer_debug_ui.exe `
  --ui-smoke-test --case production-texture-controls --repo-root .
build-slicesoft/main/apps/slicer_debug_ui/Release/slicer_debug_ui.exe `
  --ui-smoke-test --case diagnostic-settings-controls --repo-root .
```

结果：PASS。

## 3. 边界

未改变 TIFF 协议、Legacy 默认路由、Global admission、支撑策略或冻结的 12G-TCWS。
