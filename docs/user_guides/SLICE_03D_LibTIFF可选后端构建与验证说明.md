# SliceSoft LibTIFF 可选后端构建与验证说明

> 适用阶段：03D-07 COMPLETE / GO_OPTIONAL
> 更新日期：2026-08-03

## 1. 使用边界

SliceSoft 当前保留两条 TIFF Writer 轨道：

| 轨道 | 配置值 | 定位 |
|---|---|---|
| 默认生产轨道 | `handwritten` | 日常 Debug/Release、Qt UI 和生产回滚基线 |
| 显式可选轨道 | `libtiff` | 兼容验证、Runtime 验证和后续性能复测 |

03D-06/07 的性能证据没有达到默认切换门槛，因此普通用户无需安装 LibTIFF，也不要把
`slicesoft-main` 或 VS Code 日常入口改为 LibTIFF。两条轨道输出继续遵守
`p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`、无压缩和 contiguous 协议。

## 2. 构建可选轨道

本机应通过环境变量 `VCPKG_ROOT` 指向 vcpkg 根目录。当前参考环境为
`D:\Program Files Tools\vcpkg`。

```powershell
cmake --preset slicesoft-libtiff
cmake --build --preset slicesoft-libtiff-release
```

构建目录固定为：

```text
build-slicesoft/03d-libtiff
```

可查询实际编译后端与版本：

```powershell
.\build-slicesoft\03d-libtiff\Release\slicer_cli.exe `
  --tiff-backend-info-json
```

输出应包含 `configuredBackend=libtiff`、stripped/tiled Writer 可用和 LibTIFF 版本。

## 3. 部署隔离 Runtime

不要覆盖日常 `runtime/slicesoft`。使用独立目录部署：

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 `
  -BuildDir build-slicesoft/03d-libtiff `
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

## 4. 一键收口验证

```powershell
.\scripts\Run03DTiffOptionalClosure.ps1 -Config Release
```

脚本按顺序执行：

```text
默认/可选 CMake Preset 路由检查；
03D-05 双 Writer 兼容、Package、RIP 和坏包 Gate；
03D-06 Release Writer-only 性能矩阵；
隔离 LibTIFF Runtime、DLL、许可证和 manifest 检查；
隔离 Runtime Package 与 RIP strict smoke；
默认 handwritten 轨道 full regression；
03D-07 JSON 收口报告。
```

本地证据写入 `output/benchmarks/03d_07/<run-id>`，该目录不进入 Git。

## 5. 回滚与限制

回滚只需继续使用 `slicesoft-main` 或 `SLICESOFT_TIFF_BACKEND=handwritten`，不需要修改
配置文件或生产 Package 协议。当前不启用 TIFF 压缩、BigTIFF、planar separate、多 IFD
或并行写层。

LibTIFF 只有 Writer 后端。项目 Reader/RIP 仍使用同一个严格解析器，因此当前不存在
“LibTIFF Reader 与手写 Reader”的读取性能二选一；03D 性能矩阵只比较写入。
