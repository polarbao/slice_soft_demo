# DOC_DECISION_03D LibTIFF Writer 兼容迁移与性能 Gate

> 文档状态：ACCEPTED PREPARATION / PRIORITY P0 / IMPLEMENTATION NOT STARTED
> 日期：2026-07-31
> 专项代号：`03D-LIBTIFF`
> 编号说明：沿用 03B/03C 的 RGBWSV TIFF 协议责任域；执行顺序高于当前 12E-09D、12E-10 和 12F 算法优化

## 1. 背景

当前 `src/slicer_core/tiff_io.cpp` 同时承担 RGBWSV TIFF 的手写编码和严格读取，支持：

```text
Classic TIFF；
单 IFD；
R G B W S V 六通道；
uint8；
PLANARCONFIG_CONTIG；
stripped / tiled；
COMPRESSION_NONE；
black_is_print（由 package manifest 约束）。
```

手写 Writer 在 stripped 模式会先复制整层 strip 数据，在 tiled 模式会构造整层 tile
缓冲和填充边界像素，再一次性写文件。它可用且已经通过现有 RIP Reader，但协议维护、
边界处理、异常处理和后续性能优化都由项目自行承担。

仓库 `vcpkg.json` 已声明 `tiff`，但当前 CMake 没有 `find_package(TIFF)` 或
`TIFF::TIFF` 链接，生产写入仍完全使用手写实现。依赖声明存在不等于 LibTIFF 已接入。

## 2. 决策

建立 `03D-LIBTIFF` 独立专项，以 LibTIFF 作为目标 Writer 后端，并保留手写 Writer
作为迁移期对照和可回滚后端。

决策顺序：

```text
R0：冻结当前 TIFF tag、像素和性能基线；
R1：以可选 CMake 后端接入 LibTIFF，不改变默认生产行为；
R2：通过统一 Writer 接口实现双后端；
R3：完成像素、tag、Reader、坏包和原子发布等价验证；
R4：在相同预生成 layer buffer 上执行 Release Writer-only benchmark；
R5：只有兼容和性能 Gate 均通过，才把 LibTIFF 切为默认后端；
    手写后端保留一个稳定周期作为回滚路径。
```

本专项的目标是“验证后替换”，不是预先承诺 LibTIFF 一定更快。

## 3. 固定兼容合同

迁移不得改变：

```text
manifest schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
samplesPerPixel = 6；
bitDepth = 8；
sampleFormat = unsigned integer；
planarConfig = contiguous；
polarity = black_is_print；
printValue = 0；
emptyValue = 255；
storageMode = stripped / tiled；
compression = none；
单层一个 TIFF、单 IFD、Classic TIFF；
现有 rowsPerStrip、tileWidth、tileHeight 行为；
现有 package/preview/report/RIP Reader 入口。
```

当前 Writer 没有写入 XResolution/YResolution、BigTIFF 或多 IFD。LibTIFF 迁移不得借机
添加这些能力。

## 4. 备选方案

### 4.1 方案 A：继续维护手写 Writer

优点：

```text
无新增链接和部署依赖；
当前输出已通过既有测试；
文件布局完全可控。
```

缺点：

```text
TIFF tag、offset、strip/tile 和错误处理由项目自行维护；
完整层复制和 tile materialization 存在额外内存/CPU；
扩展和安全维护成本长期由单项目承担。
```

结论：保留为基线和回滚，不作为长期首选。

### 4.2 方案 B：LibTIFF

优点：

```text
专注 TIFF 读写，支持逐 strip/tile 写入和明确 tag 控制；
可直接设置 COMPRESSION_NONE、PLANARCONFIG_CONTIG 和 ExtraSamples；
依赖规模明显小于通用图像处理框架；
许可证宽松，适合商业分发；
vcpkg 已有成熟 port。
```

风险：

```text
文件二进制布局、IFD 顺序或 Software tag 可能与手写文件不同；
动态链接时需要部署 tiff DLL；
默认 vcpkg feature 会带入 JPEG/LZMA/ZIP，而本项目当前并不需要；
LibTIFF 对无压缩小文件未必比简单顺序写更快。
```

结论：选定为目标后端。vcpkg 应使用 `default-features=false`，专项内保持
`COMPRESSION_NONE`。

### 4.3 方案 C：OpenImageIO

优点：

```text
高层 ImageOutput API；
支持任意通道、tile、metadata 和多种图像格式；
后续图像工具能力丰富。
```

缺点：

```text
依赖 fmt、PNG、JPEG、OpenEXR、OpenColorIO 等完整图像生态；
高层插件可能忽略不认识的 metadata，低层 TIFF tag 可控性弱于 LibTIFF；
本专项只需要固定六通道 TIFF，能力和部署成本明显过量。
```

结论：不采用。

## 5. CMake、vcpkg 与部署决策

```text
依赖源：项目 vcpkg manifest；
本机共享根：VCPKG_ROOT（当前为 D:\Program Files Tools\vcpkg）；
普通非 OpenVDB 轨道允许使用该根；
禁止复制 installed/packages/buildtrees 到其他 vcpkg 根；
正式接入前为项目 manifest 锁定 builtin-baseline；
tiff 依赖改为 default-features=false；
CMake 使用 vcpkg port `usage` 声明的 `find_package(TIFF REQUIRED)` 与 `TIFF::TIFF`；
迁移期由 SLICESOFT_TIFF_BACKEND=handwritten|libtiff 选择；
R5 GO 前默认值保持 handwritten；
动态 triplet 必须由 Runtime 部署脚本复制 tiff DLL 并写入 runtime_manifest.json。
```

不得在准备任务中修改共享 vcpkg 安装或下载依赖。依赖接入属于 R1，需单独授权。

## 6. 许可证与维护

LibTIFF 使用允许使用、修改和再分发的宽松许可证。发布包需要携带对应版权和许可文本。
vcpkg 自身为 MIT，但 port 安装的库仍遵循 LibTIFF 原许可证。

维护要求：

```text
锁定 vcpkg builtin-baseline 和实际 tiff 版本；
记录 CVE/版本升级策略；
依赖升级不得和 Writer 行为迁移放在同一个原子任务；
Runtime 发布物记录 DLL 版本和 SHA-256；
保留手写后端直到至少一个稳定发布周期完成。
```

## 7. 性能 Gate

性能测量必须只比较 Writer：

```text
输入为同一组已经生成的 RGBWSV layer buffers；
不运行模型加载、切片、支撑、预览、report 或 manifest；
Release、同一机器、同一磁盘目录；
stripped/tiled 分开；
冷缓存和暖缓存分开；
每 case 至少 1 次预热 + 5 次正式测量；
记录 p50、p95、峰值工作集、写入字节数和失败数。
```

建议 GO 门槛：

```text
所有兼容 Gate PASS；
stripped 主生产矩阵 p50 改善 >= 15%，
或完整真实包 TIFF 保存总耗时改善 >= 10%；
峰值工作集不高于手写后端 1.10 倍；
不得出现稳定性、部署或 Reader 回归。
```

若没有达到性能门槛，仍可因维护收益保留 LibTIFF 可选后端，但不得宣称完成默认替换。

## 8. 影响与边界

正向影响：

```text
TIFF Writer 责任从手写文件格式细节收敛到稳定库；
减少整层复制的实现机会；
建立可复现 Writer-only 性能证据；
为后续 I/O 并发或流水化提供清晰边界。
```

非目标：

```text
不修改 RGBWSV 材料语义；
不实现压缩、BigTIFF、多 IFD 或 planar separate；
不修改 RIP 半色调；
不把 LibTIFF Reader 直接替换成生产 RIP Reader；
不与 12G-TCWS 白色分色或 12E-09D UI 语义混合。
```

## 9. 当前状态

```text
决策与准备：ACCEPTED；
代码：NOT STARTED；
依赖安装/升级：NOT AUTHORIZED；
当前默认 Writer：handwritten；
第一开发入口：03D-01 当前合同与 Writer-only 基线。
```

## 10. 官方参考

```text
LibTIFF TIFFWriteEncodedStrip：
https://libtiff.gitlab.io/libtiff/functions/TIFFWriteEncodedStrip.html

LibTIFF TIFFWriteEncodedTile：
https://libtiff.gitlab.io/libtiff/functions/TIFFWriteEncodedTile.html

LibTIFF 许可证：
https://libtiff.gitlab.io/libtiff/project/license.html

OpenImageIO ImageOutput（候选对比）：
https://openimageio.readthedocs.io/en/latest/imageoutput.html

OpenImageIO TIFF plugin（候选对比）：
https://openimageio.readthedocs.io/en/latest/builtinplugins.html

vcpkg：
https://github.com/microsoft/vcpkg
```
