# DOC_PREP 13C-05 阶段收口准备

> 文档状态：READY FOR DEVELOPMENT
> 版本：v1.0
> 日期：2026-07-28
> 对应任务：13C-05
> 前置：13C-04 COMPLETE

## 1. 目标

对 13C-01..04 建立的 TIFF 数据源、缓存、材料合成、统一生产预览和 Preview IO 策略做阶段总
收口，形成 M13-4 TIFF 原生统一预览候选。

13C-05 不新增预览能力，重点是完整证据、兼容回归、文档同步和后续 Gate 解锁。

## 2. 收口矩阵

必须覆盖：

```text
TIFF stripped / tiled；
RGB、R/G/B、W/S/V、RGB+W/S/V、RGB+S+W+V、Occupancy、Empty；
600/600 和 635/600；
首层/中间层/末层的 layerIndex/zMm；
六通道探针；
快速滑层、cache、cancel、stale；
无 preview 目录；
显式诊断 preview；
坏 TIFF、缺失层、manifest layer list 错误；
生产/诊断两级入口；
RIP strict 和 p0.rgbwsv.2 固定协议。
```

## 3. 真实 package 与 fixture

至少使用：

```text
一个 stripped RGBWSV package；
一个 tiled RGBWSV package；
一个 635/600 非等方 DPI package；
一个无 preview 目录 package；
一个显式诊断 preview package；
一个包含 RGB/W/S/V 的共享 writer fixture。
```

fixture 必须由共享 writer 生成。不得手工伪造一个无法通过 RIP strict 的“演示 package”冒充
生产包。

## 4. 自动化与报告

必须运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case tiff-native-preview-all-materials --package <package>
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case tiff-native-preview-no-png --package <package>
.\build\Debug\rip_reader_test.exe --package <package> --summary
.\scripts\run_ci_quick.ps1
git diff --check
```

生成：

```text
docs/slice/REPORT/REPORT_13C_TIFF原生统一预览阶段收口.md；
必要的用户手册增量；
任务清单、依赖矩阵、ROADMAP、README 和 12X/13 总览状态同步。
```

## 5. 完成 Gate

通过标准：

```text
生产预览完全不依赖 preview PNG；
默认生产 package 无重复逐通道预览图；
显式诊断图仍可用；
所有模式和物理比例正确；
错误 fail closed，不跨层兜底；
RIP strict 和固定协议通过；
阶段文档中的命令、结果和未完成项均可复核。
```

完成后：

```text
M13-4 COMPLETE；
12E-09A-05 解锁；
13D-01 解除 13C-05 顺序等待；
12E-10A 仍需同时满足 09A-05。
```

非目标：

```text
Texture/Fill/Partition 语义推断；
Qt 工作台整体布局重排；
按需导出 UI；
修改生产 TIFF 协议；
设备级打印结论。
```

2026-07-28 Gate 更新：13C-04 已完成配置、Writer、Qt、无 PNG Smoke、RIP 和 IO 对比证据，
本任务顺序前置已关闭，可进入阶段总收口。
