# REPORT HOSTFLOW H-D-05 生产包目录入口当前状态

> 状态：**COMPLETE / PASS**  
> 日期：2026-08-09  
> 任务：`H-D-05`

## 1. 完成内容

参考宿主结果页已增加“打开包目录”入口。按钮的路径身份只来自：

```text
作业完成 packageDirectory
  -> HostPackageReviewController 严格加载
  -> hostpackagereview.packagedirectory
  -> HostPackageReviewPanel 精确路径信号
  -> HostMainWindowResult 二次身份检查
  -> QDesktopServices::openUrl(QUrl::fromLocalFile(...))
```

未从输出根目录、Profile、包身份或时间戳推导路径。作业失败、严格校验失败、空路径、
目录缺失或系统拒绝打开时均 fail-closed。

## 2. 代码变更

| 模块 | 结果 |
|---|---|
| `HostPackageReviewPanel` | 增加按钮、可用性提示、点击时目录复检和精确路径信号 |
| `HostMainWindowResult` | 校验请求路径等于当前 Review 路径后调用系统文件管理器 |
| `HostMainWindow` | 使用函数指针语法接线，不增加 ABI/DLL 调用 |
| `HostPackageDirectoryPanelTests` | 覆盖初始禁用、有效路径、缺失路径和校验失败 |
| `Main.cpp` | H-B-07 UI smoke 增加入口存在且初始禁用检查 |

## 3. 验证证据

Debug 与 Release 均实际执行：

```text
hostflow_hd05_package_directory       PASS
hostflow_hb07_result_ui_smoke         PASS
hostflow_hb07_package_review          PASS
H-A/H-B + Qt host boundary + size     20/20 PASS
```

构建目标：

```text
hostflow_hd05_package_directory_tests PASS
slicer_ui_host_sim                     PASS
```

`HostMainWindow.cpp` 当前 491 行，仍低于 500 行门禁；目录打开实现位于
`HostMainWindowResult.cpp`，未扩大主窗口实现文件。

## 4. 固定边界

- 未修改 `PM_SPI_VERSION=1`、11 个导出或 15 项能力。
- 未修改生产 RGBWSV TIFF、manifest、Reader 或 RIP。
- 未修改 `apps/slicer_debug_ui/**`。
- 未把参考宿主加入源码尺寸白名单。
- 自动化没有真正启动系统文件管理器；它验证 UI 精确路径身份与 fail-closed，系统启动调用由
  `HostMainWindowResult.cpp` 的直接实现和人工 H-D-06 验收覆盖。

## 5. 后续状态

H-D-01、H-D-05 已完成。H-D-02 的 3D 接线会让现有 LOD 跳采样破面直接可见，必须先完成
R-B 方案裁决或明确临时禁用破坏性降级；H-D-03 依赖 H-D-02，H-D-06 依赖 H-D-01..05。
