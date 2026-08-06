# REPORT_14D-07-R1 引擎一致性参数化套件当前状态

> 日期：2026-08-06
>
> 状态：`COMPLETE / EXECUTION BLOCKED BY R2`

## 1. 本任务成果

- 新增 `slicer_engine_conformance_v1` 机器可读合同与结构 schema，固定 E-01..E-08、PASS 规则和冻结协议。
- 新增带 SHA-256 的 OBJ、3MF、strict blocker fixture 身份清单及生成式负例身份。
- 新增零第三方 Python 定义门禁，防止 fixture 漂移、主题缺失或冻结协议被静默修改。
- 新增参数化 Worker runner 与 PowerShell 入口；Worker 路径不写死，证据写入本地 benchmark 目录。
- runner 在真实 executor/safe publish 未完成时只输出 `blocked`，不会把 contract-info 成功伪装成 E-01..08 PASS。

## 2. 当前边界

`14D-07-R1` 建设的是可替换 Worker 的测试外壳，不是当前 Worker 准入结论。完整执行仍等待：

```text
14D-04B  Worker E2E cancellation
14D-05   safe publish/recovery
14D-08   three real capability executors
```

完成这些前置后，由 `14D-07-R2` 使用同一合同、fixture 和 runner 生成 E-01..08 完整证据。

## 3. 验证

```powershell
python tests/stage14d_07/ValidateEngineConformanceDefinition.py --repo-root .
scripts/Run14D07EngineConformance.ps1 `
  -WorkerPath build-slicesoft/main/Debug/slicer_worker.exe `
  -EvidenceRoot output/benchmarks/14d_07/debug-r1
```

第二条命令当前预期 `overall=blocked`，这是诚实结果，不是失败或 PASS。

## 4. 不变量

本任务未修改 SPI v1、11 导出、15 能力、`p0.rgbwsv.2`、RGBWSV、uint8、
`black_is_print`、默认 TIFF Writer、OpenVDB 路径或 Qt UI。
