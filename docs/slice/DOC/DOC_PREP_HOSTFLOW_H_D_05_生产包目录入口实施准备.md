# DOC_PREP HOSTFLOW H-D-05 生产包目录入口实施准备

> 状态：**PREPARATION_GATE = PASS**  
> 日期：2026-08-09  
> 任务：`H-D-05`  
> 上游：`TASKS_HOSTFLOW_打印软件流程补齐任务清单.md`、`H-B-07`

## 1. 目标与边界

在参考宿主结果页增加“打开包目录”入口。入口只接受本次切片作业返回、并经
`HostPackageReviewController::Load()` 严格校验后的 `packageDir`，不得根据输出根目录、
Profile 名称、时间戳或包身份自行拼接路径。

本任务不修改切片、TIFF、manifest、RIP、SPI 版本、导出符号或能力集合，也不修改
`apps/slicer_debug_ui/**`。

## 2. A 级现状

生产路径身份链如下：

```text
HostSliceJobController::SigCompleted(packageDirectory)
  -> HostMainWindow::OnSliceJobCompleted
  -> HostMainWindow::LoadSliceResult(packageDirectory)
  -> HostPackageReviewController::Load(packageDirectory)
  -> hostpackagereview.packagedirectory
```

结果页当前已有生产包校验、摘要、层预览、通道图和命名报告，但没有打开目录入口。
`HostMainWindowResult.cpp` 已承载结果相关槽，是目录启动逻辑的既有归属；不得把逻辑继续
塞入接近行数上限的 `HostMainWindow.cpp`。

## 3. 实施合同

1. `HostPackageReviewPanel` 显示“打开包目录”按钮，初始禁用。
2. 只有 `review.valid == true` 且 `review.packagedirectory` 是当前存在目录时启用。
3. 点击时再次检查目录存在性；失败则禁用并显示明确原因。
4. Panel 只发出经校验的原始 `packagedirectory`，不规范化后替换身份、不拼接子路径。
5. `HostMainWindowResult.cpp` 再次核对请求路径与 Controller 当前 Review 完全一致，再调用：

```cpp
QDesktopServices::openUrl(QUrl::fromLocalFile(packageDirectory))
```

6. 作业失败、结果校验失败、空路径、目录被删除、系统拒绝打开均 fail-closed。

## 4. 文件所有权

| 文件 | 变更 |
|---|---|
| `apps/slicer_ui_host_sim/HostPackageReviewPanel.h/.cpp` | 按钮、状态和精确路径信号 |
| `apps/slicer_ui_host_sim/HostMainWindow.h` | 结果目录槽声明 |
| `apps/slicer_ui_host_sim/HostMainWindow.cpp` | 函数指针 connect |
| `apps/slicer_ui_host_sim/HostMainWindowResult.cpp` | 双重身份检查与系统目录启动 |
| `tests/hostflow/HostPackageDirectoryPanelTests.cpp` | 可用、空、缺失路径门禁 |
| `apps/slicer_ui_host_sim/CMakeLists.txt` | 独立 H-D-05 Debug/Release 测试目标 |

## 5. 验证计划

1. Debug/Release 构建 `hostflow_hd05_package_directory_tests` 与 `slicer_ui_host_sim`。
2. Debug/Release 运行 H-D-05 精确路径、缺失路径和初始禁用测试。
3. 回归 `hostflow_hb07_result_ui_smoke`、`hostflow_hb07_package_review`。
4. 回归 H-A/H-B 联合门禁、Qt 宿主边界和源码尺寸守卫。
5. `git diff --check`。

## 6. 准备结论

依赖 `H-B-07` 已完成，路径权威来源、UI 所有权、错误语义和验证入口均已明确。
`H-D-05` 可独立于 H-D-02/03/04 开发，准备 Gate 通过。
