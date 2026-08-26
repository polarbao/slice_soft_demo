# REPORT MATVOL-T T-08 门禁实测留证

> 文档状态：**实测留证 / 由 MATVOL 专项代跑**
> 版本：v1.0 ｜ 日期：2026-08-26
> 执行方：MATVOL 专项（MATVOL-T 会话不可达，经用户 2026-08-26 指示代为执行）
> 结论：**GATE PASS**

## 1. 执行方式与两处踩坑

门禁脚本默认 `-BuildDir build-slicesoft/main -Config Release`，但**该默认在本工作树上不可用**：

- `build-slicesoft/main` 只有一份 131 行的部分 `CMakeCache.txt`，未生成任何工程文件，
  MSBuild 报 `MSB1009 项目文件不存在`；
- 本专项实际使用的是 NMake 生成器，构建目录为 **`build-slicesoft-nmake/Release`**
  （见 `CMakePresets.json` 的 `slicesoft-nmake-release-fallback`）。

**第二处坑**：NMake 生成器需要 MSVC 环境变量。不经 `vcvars64.bat` 直接 `cmake --build`
会得到 `C1083: 无法打开包括文件 "optional"`——缺的不是代码而是 `INCLUDE`。

实际可用的执行序列：

```
cmd /c '"<VS>\VC\Auxiliary\Build\vcvars64.bat" >nul && \
  cmake --build build-slicesoft-nmake/Release --target \
  slicer_cli rip_reader_test matvol_t_production_matrix_tests \
  matvol_rgbwsvt_legacy_package_tests matvol_legacy_transfer_session_tests'

.\scripts\run_matvol_t_t08_gate.ps1 -SkipBuild \
  -BuildDir "build-slicesoft-nmake/Release" -Config "Release"
```

构建 `BUILD_EXIT=0`（单独判定，不取包装器退出码），门禁 `GATE_EXIT=0`。

**注意**：执行前 `build-slicesoft-nmake/Release/slicer_cli.exe` 比
`src/slicer_core/output/rgbwsvt/RgbwsvtPackageReader.cpp` **陈旧**，直接 `-SkipBuild`
会测到过期二进制。本次已先重建再跑门禁。

## 2. 结果

`status = PASS`，八个用例：

| 用例 | 断言 |
|---|---|
| `reality03` | `pass` |
| `reality08` | **`expected_topology_rejection`** |
| `reality09` | **`expected_topology_rejection`** |
| `noTransfer` | `pass_rgbwsv_projection_exact` |
| `badTransfer` | `pass_strict_rejection` |
| `cancellation` | `pass_no_partial_package` |
| `deterministicTiff` | `pass` |
| `strictRip` | `pass` |

定向 CTest **4/4 PASS**（`matvol_legacy_transfer_session_tests`、
`matvol_rgbwsvt_legacy_package_tests`、`matvol_rgbwsvt_cli_candidate_tests`、
`matvol_t_production_matrix_tests`），另有一条负向 `PASS expected-error E_LAYER_STATISTICS_MISMATCH`。

三个 RGBWSVT 包经 `rip_reader_test` 校验均 PASS：`schema=p0.rgbwsvt.1`、
`channelOrder = R G B W S V T`、`bitDepth=8`、`warnings=0`、
`channelPrintPixels: R=G=B=4189, W=0, S=22257, V=0, T=5167`。

性能（`classification = same_machine_regression_gate_not_device_sla`，**不是设备 SLA**）：

| 指标 | Legacy 六通道 | RGBWSVT 七通道 | 上限 |
|---|---|---|---|
| 中位耗时 | 302.9 ms | 523.8 ms | 5302.9 ms |
| 峰值工作集 | 27.23 MB | 27.19 MB | 94.33 MB |

七通道耗时约为六通道的 1.73 倍，内存基本持平。

## 3. 对合并最要紧的两条

### 3.1 `noTransfer` 通过，即「不影响既有生产」由推断转为事实

`noTransfer = pass_rgbwsv_projection_exact` 覆盖的正是「策略关闭时走六通道旧路径」这一条。
在此之前，「双协议 opt-in 故对既有生产零暴露」只是设计推断；本次实测使其成为事实。
这是合并可以进行的关键凭据。

### 3.2 合并后本门禁的 `reality08` / `reality09` **会失败**，需同步修改

两条用例的断言是**「期望被拓扑拒绝」**，而不是「期望成功」。

MATVOL 的 MQ-06 已新增 `maxBoundaryEdges` 并实测 `08/09` 的材质 02 在三档分辨率下
奇数交点列恒为 0，使其可正常切片。因此一旦合并且把 `maxBoundaryEdges` 接入
`LoadTransferChannelPolicy`（当前该函数只解析 `selfIntersectionPolicy` 与
`maxSelfIntersectionPairs` 两项），`08/09` 将不再被拒——**本门禁随即由 PASS 转 FAIL**。

⇒ 合并时必须把这两条断言由 `expected_topology_rejection` 改为 `pass`，
并同步补齐样例的 `materialDiffuseRgbValues`（`08/09` 的材质 02 是黄色 `[255,255,0]`，
非 `03` 的浅桃色 `[255,220,198]`）。二者是同一件事的两半，不可只做其一。

## 4. 未覆盖项（本次不升级其状态）

| 项 | 状态 |
|---|---|
| `externalRip` | `accepted_user_input_not_locally_retested` |
| `physicalPrint` | `not_tested` |
| 性能口径 | 同机回归门禁，**非设备 SLA** |

## 5. 边界

- 本次仅执行门禁并记录结果，**未修改任何 MATVOL-T 代码或任务卡状态**。
  T-08 由 PREPARED 转 COMPLETE 与否，留给 MATVOL-T 会话或用户裁定。
- 门禁产物落在 `output/benchmarks/matvol_t_t08/`（`.gitignore:3` 已忽略 `output/`），
  未在仓库内留下未跟踪文件。因 PowerShell 与 .NET 当前目录不一致，
  实际落盘位置为主仓而非本工作树，不影响结论有效性——包由本工作树的二进制产出。

## 6. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.0 | 建立。记录门禁 PASS 与八用例、4/4 CTest 与性能数据；记录默认 BuildDir 不可用与 NMake 需 vcvars 两处踩坑及可用执行序列；指出 noTransfer 使「零暴露」由推断转为事实，以及 reality08/09 的拒绝断言将在合并后失败、须与黄色 Kd 一并修改。 |
