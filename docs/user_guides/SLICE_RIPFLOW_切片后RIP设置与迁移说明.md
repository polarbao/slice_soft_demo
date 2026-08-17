# SliceSoft 切片后 RIP 设置与迁移说明

> 适用状态：`SLICER_SIDE_COMPLETE / EXTERNAL_VALIDATION_DEFERRED`
> 当前模块：本地工程候选，不代表目标打印软件或实物打印已验收

## 1. 目录

RIP 只处理已经严格校验成功的切片 Package，不修改切片层：

```text
package/
  manifest.json
  layers/
    layer_000000.tiff
  rip/
    rip_result.json
    rip_000000.tif
```

`layers` 保持原名，`rip` 与其同级。已有 `rip` 时不会覆盖。

## 2. 设置与运行

在右侧“RIP 设置”页配置渲染意图、白色语义、ICC、失败策略、输出 grayBits 校验期望和超时。
“切片完成后自动处理”默认关闭；手动与自动模式使用同一个 QProcess 控制器和同一组前置/输出
检查。自动模式只在切片成功且结果严格加载成功后启动。

输入与输出 TIFF 检查在后台执行，验证期间仍可取消。启动前会冻结 `manifest.json` 与每个输入层的
canonical path、大小和 SHA-256，发布 `rip` 前再次核对；外部 RIP 若改写输入，结果不会发布。

当前 Package 尚未提供 `whiteSemantics` 时，“跟随切片包”会保持禁用；本地候选验证可显式选择
“透明”。颜色模式只允许 0。grayBits 只校验真实输出范围，不会向当前 RIP CLI 传入算法参数。

当前已验证的本地子集：

```text
输入：p0.rgbwsv.2、unsigned 8bit、RGBWSV、contiguous、stripped、600 x 600 DPI
设置：explicit_transparent、colorMode=0、deviceGrayBits=2
输出：至少 7 通道、unsigned 8bit、contiguous、stripped、600 x 600 DPI
命名：rip_%06d.tif
```

635 x 600、tiled、缺层、尺寸扩宽、输出 W/S/V 超限、灰阶 1 实测超限、opaque 灰阶 2 实测
W 超限及白语义冲突均不会发布 `rip`。

## 3. 迁移

迁移到打印软件时复制整个 `modules/rip` 目录，不挑选 DLL：

```text
<application>/modules/rip/
  rip_module.json
  runtime_dependencies.json
  rip_cli.exe
  RipSlicer.dll
  tiff.dll
  CmykFiles/
  licenses/
```

应用从自身目录相对发现 `modules/rip`。私有 `tiff.dll` 必须留在该子目录，不能覆盖宿主使用的
LibTIFF 4.7.1。模块清单中的 11 个运行文件会逐个校验大小和 SHA-256。

本地打包与自检：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/PackageRipModule.ps1 `
  -SourceRoot rip_project -Destination output/ripflow/modules/rip
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/TestRipModulePackage.ps1 `
  -ModuleDirectory output/ripflow/modules/rip
```

本机隔离目录迁移验证可使用 `scripts/TestRipModuleMigration.ps1`。它会在新目录部署 Qt、复制整个
模块并执行真实 RIP；外部交付前仍需在目标打印软件的干净机环境重新验收。

## 4. 结果与故障

成功后 `rip_result.json` 记录输入 manifest hash、模块 hash、设置、退出码、耗时、W/S/V 最小/最大值
以及 `EXTERNAL_VALIDATION_DEFERRED`。RIP 失败、取消或超时不回滚有效切片包，只清理本次
`.rip.staging.*`；UI 会分别保留“切片成功”和 RIP 终态。

清理 staging 前会拒绝 junction、符号链接和 Windows reparse point；不安全目录不会递归删除，而是
保留现场并报告 `RIP_STAGING_CLEANUP_REFUSED`。

外部分发仍等待 RipSlicer/CLI、lcms2、ICC 和私有 LibTIFF 的来源与再分发材料；打印侧还需完成
白语义、极性、ChannelSplitter、干净机、实物打印和长稳验收。
