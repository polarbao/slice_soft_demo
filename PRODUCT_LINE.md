# SliceSoft 产品线边界

本分支是 `product/packaged-slicer`，只维护封装后的切片能力包产品线。

## 保留范围

- `slicer_module.dll` 公共 SPI 能力包；
- `slicer_worker.exe` 隔离执行进程；
- `slicer_ui_host_sim.exe` 新版参考宿主；
- `slicer_cli.exe` 与 `rip_reader_test.exe` 生产及协议工具；
- RGBWSV TIFF、Profile、报告、场景、几何与支撑核心实现。

## 剔除范围

- 旧版 `apps/slicer_debug_ui` Qt 工作台源码；
- 旧版 UI 的 VSCode 启动入口、UI smoke 和迁移期对比基准；
- 同时部署旧版与新版 UI 的混合运行时。

旧版源码和功能冻结在 `product/legacy-slicer`，基线为 `v0.1.0`。
拆分前完整状态保存在本地分支
`archive/pre-product-split-complete-20260813-182050`，并另有离线 Git bundle。

## 构建入口

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/PrepareSliceSoftRuntime.ps1 `
  -Config Release `
  -RuntimeDir runtime/slicesoft
```

运行入口：

```text
runtime/slicesoft/Release/slicer_ui_host_sim.exe
```

VSCode 使用 `SliceSoft Packaged:*` 入口。该分支不再生成
`slicer_debug_ui.exe`。
