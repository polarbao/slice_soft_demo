# DOC_EXEC_12E-08D-03 共享 Writer、Package 与 RIP 结果

> 文档状态：COMPLETE
> 完成日期：2026-07-23
> 下一原子任务：12E-08D-04 显式 Profile、真实模型 Release matrix 与 GO/NO-GO

## 1. 任务目标

12E-08D-03 在不改变生产协议的前提下，把 Legacy 和 Global Surface Shell 的最终内存层统一到同一套
RGBWSV TIFF/package 后处理边界，并验证：

```text
p0.rgbwsv.2；
R G B W S V；
uint8；
black_is_print；
printValue=0；
emptyValue=255；
完整且连续的 TIFF layer list；
preview/report 与生产 layerIndex 对齐；
RIP strict PASS；
Global blocker 不回退到 Legacy，也不发布残缺 package。
```

## 2. 实现结果

新增共享输出模块：

```text
src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.h
src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.cpp
```

该模块提供：

```text
WriteRgbwsvProductionLayerTiff：
  Legacy 与 Global 共用的逐层 RGBWSV TIFF 入口；

WriteRgbwsvProductionPackage：
  admitted layer list 的校验、TIFF、manifest、preview/report、
  RIP strict、staging 和原子发布；

RgbwsvProductionPackageWriteRequest：
  requested/effective mode、admission、grid、storage、preview 和最终层；

RgbwsvProductionPackageWriteResult：
  productionOutputWritten、fallbackApplied、layerCount 和发布目录。
```

原有 `tiff_io` 仍是唯一 TIFF 编码实现。本任务只把写入参数改为
`std::span<const std::uint8_t>`，避免共享入口为大层缓冲区额外复制。Legacy 主路径已改为调用
`WriteRgbwsvProductionLayerTiff`，没有新增第二套 TIFF 编码器。

新增 Global 桥接模块：

```text
src/slicer_core/pipeline/GlobalSurfaceShellProductionPackage.h
src/slicer_core/pipeline/GlobalSurfaceShellProductionPackage.cpp
```

它只接受 08D-02 产生的 `ready_for_writer + fullClosurePass` Adapter 结果，并把层移动到共享 package
writer。Adapter blocked、模式不匹配、尺寸/层数不一致时 fail closed，发布目录保持不存在。

## 3. Package 合同

共享 writer 写出的 manifest 固定包含：

```text
requestedPipelineMode；
effectivePipelineMode；
productionAcceptance=admitted；
productionOutputWritten=true；
fallbackApplied=false；
grid；
tiff protocol/storage/layers；
root layers；
reports；
preview。
```

写包顺序为：

```text
完整请求预校验
-> sibling staging
-> TIFF / preview / report / manifest
-> validate_slice_package(staging)
-> 原子替换正式 package
```

任何校验、写入或 RIP 错误都会删除 staging；已有 package 只在新 staging 通过 RIP 后才被替换。

## 4. Preview 与 Report

共享 package writer 输出 `RGB/W/S/V` 四类显示图。生产值不会直接当作伪彩图颜色：

```text
RGB：生产 RGB 值；
W：青色伪彩；
S：绿色伪彩；
V：rgb(127,127,127)；
Empty：白色。
```

preview 是 display-only，不改变 TIFF。`slice_report.json` 记录逐层和汇总
`printPixels/emptyPixels`；`preview_report.json` 记录格式、间隔和生成文件。

## 5. 自动验证

新增测试目标：

```text
rgbwsv_production_package_writer_unit_tests
```

覆盖 11 个场景：

```text
admitted Global package + RIP；
stripped/tiled 同协议；
Legacy 使用同一 package writer 合同；
08D-02 Adapter 经 Global 桥接写包；
blocked Adapter 不写包；
Adapter 协议不匹配不写包；
PNG preview；
非 admitted 不写包；
requested/effective mode 不一致不写包；
层号断裂不写包；
已有 package 原子替换。
```

本任务实际执行并通过：

```text
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
```

结果为 `48/48 CTest PASS`、`CI quick complete`；共享 writer 测试内部调用现有
`validate_slice_package` 对 stripped/tiled 和完整层列表执行 RIP strict。

## 6. 当前边界

08D-03 证明了共享 Writer/Package/RIP 边界可用，但不等于 Global 已向普通生产入口开放：

```text
Legacy 仍是默认生产模式；
Global Router 仍保持 production unavailable；
没有静默回退；
没有新增 UI 生产选择器；
尚未执行 08D-04 的显式 Profile、真实模型 Release matrix 和最终 GO/NO-GO。
```

因此本任务完成时状态为：`12E-08D-03 COMPLETE`，`12E-08D-04 READY / NEXT`。
后续 08D-04 已于 2026-07-23 完成，当前结论见
`DOC_EXEC_12E_08D_04_显式Profile与ReleaseMatrix结果.md`。
