# REPORT_03D-02 LibTIFF 依赖、构建与 Runtime 接入

> 状态：COMPLETE
> 日期：2026-07-31
> 范围：只接入可选依赖轨道，不实现或切换生产 Writer

## 1. 完成内容

```text
vcpkg builtin-baseline=d13fa75214c258099923cf25a5e6311e58c07f3b；
tiff=4.7.1，default-features=false；
新增 SLICESOFT_TIFF_BACKEND=handwritten|libtiff；
默认值保持 handwritten；
仅 libtiff 轨道 find_package(TIFF REQUIRED) 并链接 TIFF::TIFF；
新增独立 slicesoft-libtiff CMake preset；
新增 TIFF 构建能力自检和 CLI JSON 输出；
Runtime 部署 tiff.dll、许可证、版本及 SHA-256 元数据。
```

LibTIFF 使用 vcpkg manifest mode 安装到独立构建目录
`build-slicesoft/03d-libtiff/vcpkg_installed`。没有修改共享 vcpkg 根的 classic
安装状态，也没有启用 OpenVDB。

## 2. 当前后端语义

| 配置 | 依赖可用 | 生产 Writer | 默认 |
|---|---|---|---|
| `handwritten` | 不链接 LibTIFF | 手写 Writer | 是 |
| `libtiff` | 链接 LibTIFF 4.7.1 | 仍为手写 Writer | 否 |

03D-02 的 `libtiff` 值表示“依赖轨道已接入”，不表示 LibTIFF Writer 已实现。
`--tiff-backend-info-json` 中 `libtiffWriterImplemented=false` 用于阻止部署结果被误解。

## 3. Runtime 合同

`PrepareSliceSoftRuntime.ps1` 新增：

```text
-TiffBackend handwritten|libtiff；
-VcpkgRoot，默认读取 VCPKG_ROOT；
构建指纹纳入 TIFF 后端、vcpkg 根和 vcpkg.json；
校验请求后端与 CMake 生成元数据一致；
LibTIFF 轨道部署 tiff.dll；
复制 share/tiff/copyright 到 licenses/libtiff.txt；
runtime_manifest.json 写入版本、DLL SHA-256 和许可证相对路径。
```

本机 Release Runtime 证据：

```text
configuredBackend=libtiff
libtiffDependencyAvailable=true
libtiffWriterImplemented=false
libtiffVersion=LIBTIFF, Version 4.7.1
tiff.dll sha256=239b451cb34a72db191fd793087f9346b9340d49d82a46c5ba5fba457b7b75ec
license=licenses/libtiff.txt
```

## 4. 验证

```text
CMake preset 解析：PASS；
handwritten Release build：PASS；
handwritten build-info CTest：PASS；
handwritten Writer contract CTest：PASS；
handwritten Runtime 部署：PASS；
LibTIFF manifest install/configure：PASS；
LibTIFF Debug build + build-info CTest：PASS；
LibTIFF Release build + build-info CTest：PASS；
LibTIFF Debug Runtime 部署：PASS；
LibTIFF Release Runtime 部署：PASS；
CLI build-info JSON：PASS；
DLL、许可证、版本和 SHA-256 manifest 校验：PASS；
git diff --check：PASS。
```

首次向复用的 Debug Runtime 父目录执行原子目录移动时遇到一次 Windows
`Access denied`；清理后的独立验证目录重试通过。该现象没有改变构建或依赖结论，
但后续 Runtime 回归仍应保留原子部署失败检查。

## 5. 未做内容

```text
没有新增 LibTIFF Writer；
没有修改 write_rgbwsv_tiff 路由；
没有改变 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
没有启用压缩、BigTIFF、多 IFD 或 planar separate；
没有切换生产默认后端；
没有得出 LibTIFF 更快的结论。
```

## 6. 下一 Gate

03D-03 可开始实现 Writer 抽象、手写 adapter 和 LibTIFF stripped Writer。完成前必须：

```text
保持 handwritten 默认；
固定 LibTIFF stripped、contiguous、compression=none；
逐 strip 写入，不复制完整图层；
使用当前严格 Reader 独立验证 decoded pixel 和 required tags；
原子任务独立验证和提交。
```
