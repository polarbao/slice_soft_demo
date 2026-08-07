# REPORT_14E-06 打印侧可移植模块清单当前状态

> 状态：**SLICER-SIDE COMPLETE / PRINT-SIDE ACK PENDING**
> 日期：2026-08-07
> 任务：14E-06

## 1. 交付结果

已完成打印侧文件级可移植清单：

- `contracts/slicer_ui_host_portability_manifest.json`：机器可读清单；
- `DOC_DELIVERY_14E_06_打印侧可移植模块文件清单.md`：人工可读的移植边界、顺序和风险；
- `ValidateUiHostPortabilityManifest.py`：验证参考宿主文件无遗漏、合同存在且没有切片内部源码泄漏。

当前清单覆盖 46 个宿主源/构建文件：42 个标记为 `direct_copy`，4 个标记为 `adapt_required`；另登记 11 个必须同步的公开合同输入和 4 类 14F 运行时交付物。

## 2. 冻结边界

- 打印侧只通过 `print_module_spi.h` 和运行时 `slicer_module.dll` 调用切片能力；
- 不复制 `slicer_core/base/engine/module/worker` 内部源码；
- `HostMainWindow`、`Main.cpp` 和 CMake 属于参考壳，必须接入打印软件既有架构；
- 交互、相机、ViewData、缓存和纹理 fail-closed 语义不得在移植时弱化；
- 本卡不修改 SPI v1、11 个导出、15 项能力、TIFF 或生产 Package。

## 3. 验证

```powershell
python tests/contracts/ValidateUiHostPortabilityManifest.py
python -m json.tool contracts/slicer_ui_host_portability_manifest.json
python tests/stage14e_02/ValidateQtHostBoundary.py --repo-root . --binary build-slicesoft/main/apps/slicer_ui_host_sim/Release/slicer_ui_host_sim.exe
```

清单自动验证用于防止后续新增参考宿主文件却未分类。打印侧书面确认仍是外部证据，因此 D14-E-07 保持 `NOT RUN`。

## 4. 后续

14E 开发任务已在切片侧收口。下一阶段是 14F 打包与外部联调；启动 14F-01 前需单独复核打包依赖、干净环境和外部测试窗口。
