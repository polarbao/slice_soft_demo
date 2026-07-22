# REPORT 12E-09A-01 只读 Diagnostic Facade 与 UI DTO 当前状态

> 日期：2026-07-22
> 状态：COMPLETE / DIAGNOSTIC ONLY / PRODUCTION NOT ADMITTED
> 下一任务：12E-09A-02 Effective Config 事务与派生字段

## 1. 任务结论

12E-09A-01 已完成。当前代码具备从单个当前模型
`slicesoft.texture_fill_partition.12e.1` 报告生成只读 UI DTO 的能力，稳定区分：

```text
pending
unavailable
blocked
diagnostic
```

该能力没有文件输出接口，不会写 preview、TIFF 或 production package。12E-08D 仍为 NO-GO，
`global_surface_shell` 仍不得作为生产切片模式。

## 2. 实现内容

新增：

```text
src/slicer_core/pipeline/TextureFillPartitionDiagnosticFacade.h
src/slicer_core/pipeline/TextureFillPartitionDiagnosticFacade.cpp
tests/unit/texture_fill_partition_diagnostic_facade/Main.cpp
texture_fill_partition_diagnostic_facade_unit_tests
```

DTO 覆盖：

```text
status / availability / backend / backendRole / productionAcceptance；
issues[].code / severity / message / context；
widthMetrics；
partitionStats；
rasterMapping；
fullClosureLinkage；
performance；
productionOutputWritten 只读安全标志。
```

## 3. 安全语义

```text
1. 未执行的动态数值使用 std::optional，不把 null 或报告骨架中的 0 冒充测量结果；
2. blocked 报告保留 topology issue 及其 context；
3. 08C release matrix 和未知 schema 不能冒充当前模型报告；
4. 任一诊断 section 声明 productionOutputWritten=true 时强制 blocked；
5. status=pass 也只投影为 diagnostic，不在 09A 提前生成 admitted 状态；
6. 不修改 legacy slicer_cli、OpenVDB 默认开关或 RGBWSV 生产协议。
```

## 4. 定向验证

实际运行并通过：

```text
cmake --build build --config Debug --target texture_fill_partition_diagnostic_facade_unit_tests
.\build\Debug\texture_fill_partition_diagnostic_facade_unit_tests.exe
ctest --test-dir build -C Debug -R "texture_fill_partition_(diagnostic_facade|report)" --output-on-failure
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
cmake --build build --config Debug
git diff --check
```

结果：Facade 单测 PASS；Facade/Report 定向 CTest 2/2 PASS；Qt UI self-test 的 `startup` 与
`experimental-report-summary` PASS；默认 Debug 全量构建 PASS。

本原子任务未重新运行 Quick CI。R4-08 已记录当前 Quick CI 在既有
`material_process_top2 widthPx expected=48 actual=226` golden 基线处失败，未刷新 golden。

## 5. 后续边界

`12E-09A-02` 可以开始实现 session Effective Config 事务和派生字段，但必须继续满足：

```text
不覆盖 samples/configs fixture；
不开放 global production 按钮；
不静默 fallback 到 legacy；
不修改 p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print；
12E-09B 继续等待 12E-08D admission 与用户明确授权。
```
