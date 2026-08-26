# REPORT MATVOL-T T-08 门禁实测留证

> 文档状态：**实测留证 / 由 MATVOL 专项代跑**
> 版本：v1.1 ｜ 日期：2026-08-26
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

## 2.1 更正（v1.1）：`cases` 块是硬编码字面量，不是断言结果

v1.0 把上表当作门禁的断言逐条列出，**该读法是错的**。`run_matvol_t_t08_gate.ps1` 里那八项
是写进报告的**字面量**，位于 `$report = [ordered]@{ cases = [ordered]@{ ... } }` 之内。
门禁自身的 `Assert-True` 只有六条：`SLICE_TIMING` 存在且内存可观测、manifest schema 为
`p0.rgbwsvt.1`、`productionAcceptance` 匹配、T 通道 `printPixels > 0`、
重复运行 TIFF 字节一致、以及耗时与峰值内存不超相对门。**脚本内不执行 `08.obj` / `09.obj`。**

**但这不等于那些性质没被验证**——它们由门禁调起的 4 个 CTest 用例真实覆盖：

| 报告字面量 | 真实覆盖位置 |
|---|---|
| `reality08` / `reality09` | `tests/matvol_t/MatvolTProductionMatrixTests.cpp:262-263`，`OpenRealityFailsWithoutPackage`，且**已按资产取黄色 `{255,255,0}`** |
| `noTransfer` | `tests/matvol_t/LegacyRgbwsvtPackageTests.cpp:173`，`MissingTransferPreservesLegacyProjection`，逐像素逐通道比对前六通道并断言 T 为空 |

结论仍是 PASS，但**证据链应指向这两个用例，而不是报告里的字面量**。

## 3. 对合并最要紧的两条

### 3.1 `noTransfer` 覆盖的是「开启但未命中」，不是「关闭等同合并前」（v1.1 更正）

v1.0 称该用例证明了「不影响既有生产」，**说法过强**。
`MissingTransferPreservesLegacyProjection` 的实际做法是：同一配置跑两遍，一遍走六通道，
一遍**开启** `transferChannelPolicy` 但把匹配色设为 `{1,2,3}`（不命中任何材质），
然后逐像素逐通道断言前六通道相等、T 通道为 255。

这是一条扎实的用例，但它验的是**「策略开启且无缩裹命中」**，
而不是**「策略关闭时与合并前逐字节相同」**。后者才是「合并是否改变你现有产出」的直接命题，
门禁与 CTest 均未覆盖，需由合并前后两套二进制的字节比对另行证明。

另有 `MatvolTransferResolverTests.cpp:183` 断言 `package_protocol` 默认仍为 `p0.rgbwsv.2`，
它保证了默认不改道，但同样不构成字节级证据。

### 3.2 合并后 08/09 的处置（v1.1 修订）

v1.0 判定「合并后 `reality08`/`reality09` 会失败、须把断言改为 `pass`」。
按真实覆盖重新审视后，**处置方式不同且更好**：

`OpenRealityFailsWithoutPackage` 使用的配置**不设** `maxBoundaryEdges`，
而该参数默认为 0，故合并后该用例**仍然成立**，不必改动——它恰好守住了
「默认仍 fail closed」这条纪律。

正确的补法是**新增一条对照用例**而非修改原用例：
`BoundedOpenEdgeRealityProducesPackage` 用同一资产、同一配置，
唯一差别是显式把 `maxBoundaryEdges` 设为 8，断言此时可产出 `p0.rgbwsvt.1` 包。
两条合起来才说明「放宽是真生效，且没有把门整个拆掉」——缺任一条都不足以说明。

配套改动：`LoadTransferChannelPolicy` 增加 `maxBoundaryEdges` 解析（此前只解析
`selfIntersectionPolicy` 与 `maxSelfIntersectionPairs` 两项），
以及 11 个 `samples/configs/matvol_t` 工艺样例补入 `maxBoundaryEdges: 8`
与黄色 `[255,255,0]`。

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
| 2026-08-26 | v1.1 | 更正两处过强的读法：门禁 cases 块是硬编码字面量而非断言，真实覆盖在其调起的 4 个 CTest 用例中；noTransfer 验的是「开启但未命中」而非「关闭等同合并前」，后者仍需字节比对。并把 08/09 的处置由「改原断言」改为「新增对照用例」，因原用例不设 maxBoundaryEdges、默认 0 下仍成立。 |
