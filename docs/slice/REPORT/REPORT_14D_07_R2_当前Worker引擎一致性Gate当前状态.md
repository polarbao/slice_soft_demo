# REPORT_14D-07-R2 当前 Worker 引擎一致性 Gate 当前状态

> 日期：2026-08-07
>
> 状态：`COMPLETE / DEBUG+RELEASE PASS`

## 1. 任务结论

当前 `slicer_worker` 已通过冻结的 E-01..08 引擎一致性门禁。门禁从公开 Worker 进程与
`file_contract_v1` 文件合同驱动三项真实重能力，不 include 或直接调用 Worker 私有 executor。

## 2. 覆盖范围

| 主题 | 结论 | 主要证据 |
|---|---|---|
| E-01 合同与身份 | PASS | slice/full-preflight/repair 请求结果身份闭合；未知能力 fail-closed |
| E-02 生产包协议 | PASS | `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 与 RIP strict |
| E-03 固定 Golden | PASS | 同 Worker、fixture、Profile 的生产 TIFF layer hashes 逐字节一致 |
| E-04 报告语义 | PASS | scene/slice 稳定字段投影一致；材料与支撑语义取自 `slice_report.totals` |
| E-05 进度与耗时 | PASS | 进度单调、成功终态 100、耗时非负 |
| E-06 错误与拒绝 | PASS | 未知能力、stale scene、缺资源、strict open mesh 均 fail-closed |
| E-07 取消与恢复 | PASS | 取消不超过 2000ms、稳定取消码、旧包不变、owned 临时产物清零 |
| E-08 可替换性 | PASS | Worker 路径参数化，当前基线及真实 Texture2D 3MF preflight 通过 |

## 3. 实现与证据

- `stage14d07_engine_conformance_gate`：C++20 真实 Worker Gate；
- `RunEngineConformance.py`：执行 Gate、核验八份主题证据并汇总；
- `Run14D07EngineConformance.ps1`：Debug/Release 可复用入口；
- `output/benchmarks/14d_07/*`：本地忽略证据，不提交大体积生产包。

E-04 没有虚构独立的 material/support report。当前场景包以
`slice_report.totals.printPixels` 和其中的 `S` 统计作为材料、支撑稳定投影；后续若生产合同新增
独立报告，应通过受控修订扩展门禁。

## 4. 已执行验证

```powershell
cmake --build build-slicesoft/main --config Debug --target stage14d07_engine_conformance_gate --parallel
cmake --build build-slicesoft/main --config Release --target stage14d07_engine_conformance_gate --parallel

scripts/Run14D07EngineConformance.ps1 `
  -WorkerPath build-slicesoft/main/Debug/slicer_worker.exe `
  -GatePath build-slicesoft/main/Debug/stage14d07_engine_conformance_gate.exe `
  -EvidenceRoot output/benchmarks/14d_07/debug-r2-final

scripts/Run14D07EngineConformance.ps1 `
  -WorkerPath build-slicesoft/main/Release/slicer_worker.exe `
  -GatePath build-slicesoft/main/Release/stage14d07_engine_conformance_gate.exe `
  -EvidenceRoot output/benchmarks/14d_07/release-r2-final
```

Debug 与 Release 均输出 `PASS (E-01..E-08)`。定向 CTest 中 R1 定义门、R2 Gate 与真实
Worker 产物测试均通过。

## 5. 边界

- E-08 本轮建立的是“当前 Worker 基线”；未声称测试了另一个 Worker 版本；
- 替换 Worker 时必须保持宿主、模块、请求、fixture 与 Profile 不变并完整重跑；
- 不修改 SPI v1、11 个导出、15 项能力、生产 TIFF、RGBWSV、位深、极性或 Qt UI；
- 性能只记录，不作为当前一致性 PASS 阈值。
