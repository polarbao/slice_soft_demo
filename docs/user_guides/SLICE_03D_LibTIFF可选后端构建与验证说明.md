# SliceSoft LibTIFF 默认后端与遗留 Writer 验证说明

> 适用阶段：03D-07 历史基线；TIFF T-A-03 默认后端切换 COMPLETE
> 更新日期：2026-08-11

## 1. 使用边界

T-A-03 已在用户授权下取代 03D-07 的旧默认结论。SliceSoft 当前保留两条 TIFF Writer 轨道：

| 轨道 | 配置值 | 定位 |
|---|---|---|
| 默认生产轨道 | `libtiff` | 日常 Debug/Release、Qt Runtime 和能力包 Worker |
| 显式遗留轨道 | `handwritten` | T-A-05 弃用观察期、旧字节对照和已知对齐失败证明 |

03D-06/07 的历史性能结论仍保留，但 T-A 专项以 TIFF 6.0 偶偏移合规性为更高优先级，
已把主轨道切换到 LibTIFF 4.7.1。两条轨道输出继续遵守 `p0.rgbwsv.2`、RGBWSV、
uint8、`black_is_print` 和 contiguous 协议；默认压缩仍为 `none`。

## 2. 构建默认轨道

本机应通过环境变量 `VCPKG_ROOT` 指向 vcpkg 根目录。当前参考环境为
`D:\Program Files Tools\vcpkg`。

```powershell
cmake --preset slicesoft-main
cmake --build --preset slicesoft-release
```

构建目录固定为：

```text
build-slicesoft/main
```

可查询实际编译后端与版本：

```powershell
.\build-slicesoft\main\Release\slicer_cli.exe `
  --tiff-backend-info-json
```

输出应包含 `configuredBackend=libtiff`、stripped/tiled Writer 可用和 LibTIFF 版本。

专项复测仍可使用隔离的 `slicesoft-libtiff` preset；它与主轨道行为一致，但使用独立的
`build-slicesoft/03d-libtiff/vcpkg_installed`，避免污染默认依赖根。

显式遗留 Writer 使用：

```powershell
cmake --preset slicesoft-handwritten-legacy
cmake --build --preset slicesoft-handwritten-legacy-release
```

该轨道不是生产回滚默认，其对齐探针必须按已知失败被 CTest 的 `WILL_FAIL` 门禁捕获。

## 3. 部署隔离 Runtime

日常 Runtime 默认部署 LibTIFF。需要隔离验收时使用独立目录：

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 `
  -BuildDir build-slicesoft/main `
  -RuntimeDir output/runtime-libtiff `
  -Config Release `
  -BuildSystem VisualStudio `
  -TiffBackend libtiff
```

部署结果位于 `output/runtime-libtiff/Release`，并必须包含：

```text
tiff.dll；
licenses/libtiff.txt；
runtime_manifest.json；
runtime_manifest.tiffWriter.configuredBackend=libtiff；
LibTIFF 版本和 DLL SHA-256。
```

## 4. 当前收口验证

`Run03DTiffOptionalClosure.ps1` 冻结的是 03D-07 的历史 `handwritten default` 决策，不能再
作为 T-A-03 当前 Gate。当前至少执行：

```powershell
ctest --test-dir build-slicesoft/main -C Release `
  -R "^(tiff_writer_contract_unit_tests|tiff_writer_alignment_conformance_unit_tests|tiff_backend_build_info_unit_tests|tiff_writer_backend_unit_tests|tiff_writer_equivalence_unit_tests)$" `
  --output-on-failure

.\scripts\PrepareSliceSoftRuntime.ps1 `
  -BuildDir build-slicesoft/main `
  -RuntimeDir output/runtime-libtiff `
  -Config Release `
  -BuildSystem VisualStudio `
  -DeployOnly
```

同时应使用默认 `slicer_cli` 生成语义 Golden Package，并通过 `rip_reader_test --quiet`；
能力包发布还需执行 `PackageSlicerModule.ps1` 与 `TestSlicerModulePackage.ps1`，确认
`runtime_dependencies.json`、`checksums.sha256` 和 `tiff.dll` 同步闭环。

## 5. 回滚与限制

遗留对照需显式使用 `slicesoft-handwritten-legacy`，不得把 `slicesoft-main` 解释为
handwritten 回滚。当前默认不启用 TIFF 压缩、BigTIFF、planar separate、多 IFD 或并行写层。

LibTIFF 只有 Writer 后端。项目 Reader/RIP 仍使用同一个严格解析器，因此当前不存在
“LibTIFF Reader 与手写 Reader”的读取性能二选一；03D 性能矩阵只比较写入。
