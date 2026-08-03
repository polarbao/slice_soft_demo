# REPORT_03D-07 LibTIFF 可选后端阶段收口

> 状态：COMPLETE / GO_OPTIONAL / DEFAULT HANDWRITTEN
> 日期：2026-08-03

## 1. 阶段结论

03D-07 已按 03D-06 的 `GO_OPTIONAL` 结论完成可选后端收口：

```text
默认 Writer：handwritten，未改变；
可选 Writer：libtiff 4.7.1，显式 CMake/Runtime 入口；
协议：p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print / 无压缩；
Runtime：tiff.dll、许可证、版本、SHA-256 和 manifest 已验证；
Package/RIP：隔离 LibTIFF Runtime smoke PASS；
默认切换：因性能 Gate 未通过继续阻断。
```

## 2. 实现内容

新增 `scripts/Run03DTiffOptionalClosure.ps1`，把以下分散验证固化为单一可复现入口：

```text
CMake Preset 默认/可选后端策略检查；
03D-05 compatibility Gate；
03D-06 Writer-only Release 性能矩阵；
LibTIFF 隔离 Runtime 部署合同；
Runtime Package 与 RIP strict；
默认 handwritten full regression；
slicesoft.tiff_optional_closure.03d.1 JSON 报告。
```

同时更新 `slicesoft-libtiff` Preset 描述，并新增用户构建与验证说明。VS Code 日常
Debug/Release 入口保持精简，不隐式切换或新增生产 LibTIFF UI 环境。

## 3. 写入性能实测

本次收口复测使用同一台 Windows 机器、Release、相同预生成 RGBWSV buffer 和独立进程。
正数表示 LibTIFF 更快，负数表示 LibTIFF 更慢。

| Case | 存储 | handwritten p50 | LibTIFF p50 | p50 改善 | 峰值内存比 |
|---|---|---:|---:|---:|---:|
| small fixture | stripped | 3.6060 ms | 5.7011 ms | -58.100% | 0.9044 |
| reality single | stripped | 3.8778 ms | 5.5794 ms | -43.881% | 0.9267 |
| multi model | stripped | 6.0613 ms | 7.7689 ms | -28.172% | 0.6772 |
| non-integral tile | tiled | 6.5327 ms | 5.2566 ms | +19.534% | 0.9567 |

主判定 case 的最低 warm stripped p50 改善为 `-43.881%`，远低于默认切换要求的
`+15%`；主判定 case 峰值内存比最大值 `0.9267` 满足不超过 `1.10` 的门槛。结论仍为：

```text
LibTIFF 当前写入速度下降，没有形成稳定性能收益；
LibTIFF 峰值内存不高于手写轨道；
保留可选后端，但不切换默认 Writer。
```

03D-06 冻结报告中的 `-58.937%` 与本次 `-43.881%` 都属于本机样本，不是跨机器承诺；
两轮方向一致，均不支持默认切换。

## 4. 读取性能边界

当前没有 LibTIFF Reader 后端。手写 Writer 与 LibTIFF Writer 生成的 Package 都由同一个
项目严格 Reader/RIP 读取，因此不能把 Reader 时间归因于 Writer 选择，也不存在两个读取
实现可直接比较。03D-05 已验证两种文件的 decoded pixel、tag 和 RIP strict 等价；这属于
正确性验证，不是读取性能 benchmark。

为回答“不同 Writer 生成文件是否影响现有 Reader”这一补充问题，本次对同一 20 层
fixture 分别生成 handwritten/LibTIFF Package，并用同一个项目严格 Reader 各启动独立进程
读取 40 次。该轻量样本包含进程启动开销，结果只能作为文件布局 smoke，不能作为 Reader
后端性能结论：

| Writer 生成文件 | TIFF 总字节 | Reader p50 | Reader p95 |
|---|---:|---:|---:|
| handwritten | 143000 | 19.2873 ms | 22.2103 ms |
| LibTIFF | 143020 | 20.6752 ms | 30.1264 ms |

两组文件大小只差 20 字节；p50 差约 1.39 ms，处于独立进程启动和系统调度容易影响的量级。
因此当前没有证据表明 LibTIFF 生成文件能提升读取速度；也不能据此宣称项目 Reader 本身退化。

若未来需要优化读取，应另立 Reader benchmark，以同一文件分别比较项目 Reader 与明确引入
的新 Reader 实现，不得把当前 Writer-only 数字当作读写总耗时。

## 5. 验证证据

```powershell
.\scripts\Run03DTiffOptionalClosure.ps1 `
  -Config Release `
  -SkipBuild
```

实际结果：

```text
03D-05 handwritten CTest：8/8 PASS；
03D-05 LibTIFF CTest：8/8 PASS；
stripped/tiled Package 与 RIP strict：PASS；
18 个 bad package 稳定错误码：PASS；
LibTIFF Runtime manifest/DLL/license：PASS；
LibTIFF Runtime Package/RIP strict：PASS；
默认 handwritten full regression：PASS；
git diff --check：见提交前验证记录。
```

本次本地收口证据：

```text
output/benchmarks/03d_07/20260803_110854_881/optional_closure_report.json
output/benchmarks/03d_07/20260803_110854_881/matrix/20260803_110854_987/tiff_writer_matrix.json
output/benchmarks/03d_07/20260803_110854_881/reader-comparison/same_reader_comparison.json
```

## 6. 后续边界

03D 当前授权范围已完成。下一优先任务可进入 `12E-09D`。未来只有在固定参考机、新性能
Gate、Runtime/Reader/RIP 无回归和用户再次明确授权全部满足时，才能重新讨论默认切换。
