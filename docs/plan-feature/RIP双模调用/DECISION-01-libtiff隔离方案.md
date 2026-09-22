# DECISION-01 libtiff 隔离方案对比与选型

> 目录：`docs/plan-feature/RIP双模调用/` ｜ 日期：2026-09-17 ｜ 读者：双方 + 决策者
> 配套：[整体](README.md) ｜ [REQ-01 宿主侧](REQ-01-宿主侧双模调用能力要求.md) ｜ [REQ-02 RIP 模块侧](REQ-02-RIP模块侧双模调用能力要求.md)
>
> **本文回答一个具体提问**：如果 RIP 使用与宿主相同版本的 libtiff，是否就能省掉 tiff 库匹配问题？
>
> **结论先行**：**能省掉"加载期冲突"，但省不掉重编，而且会新引入一个更隐蔽的"运行期串扰"。**
> 推荐方案是 **(d) 统一到宿主的 libtiff 4.7.1 源码 + 静态链接进 `RipSlicer.dll`**——
> 它同时拿到隔离、零锁步、零串扰，并额外解掉一个许可证阻断项。

---

## 1. 问题复述

`RipSlicer.dll` 对 `tiff.dll` 是 **load-time 静态导入**；宿主进程里已经加载了同名的 `tiff.dll`。
Windows 加载器对已加载模块**按基名解析导入**，因此进程内模式下 RIP 会被强制绑到宿主那一份，
`LoadLibraryEx` 的绝对路径与各种 `LOAD_LIBRARY_SEARCH_*` 标志一律无效。

---

## 2. 关键实测数据

### 2.1 RIP 对 libtiff 的依赖面：只有 10 个符号

逐符号解析 `RipSlicer.dll` 的导入表，它从 `tiff.dll` 只取用：

| 类别 | 符号 | 跨 4.1.0→4.7.1 稳定性 |
|---|---|---|
| 开关 | `TIFFOpenW`、`TIFFClose` | 稳定 |
| 标签 | `TIFFGetField`、`TIFFSetField` | 稳定（可变参数，签名未变） |
| 像素 | `TIFFReadScanline`、`TIFFWriteScanline` | 稳定 |
| 布局 | `TIFFScanlineSize`、`TIFFDefaultStripSize` | 稳定 |
| **全局** | **`TIFFSetErrorHandler`、`TIFFSetWarningHandler`** | 稳定，**但是进程级全局状态** |

**含义 1**：移植面极小。4.1.0 → 4.7.1 跨了 6 个小版本，但这 8 个非全局 API 在整个 libtiff 4.x
生命周期内签名未变，重编风险低。

**含义 2**：**RIP 确实在安装进程级全局 handler。** 这不是理论风险——它就在导入表里。
今天无害，是因为 RIP 有自己私有的 `tiff.dll` 实例，两份 libtiff 各有各的全局量；
**一旦两侧共用同一个 `tiff.dll` 模块，RIP 的 handler 会直接覆盖宿主的**。

### 2.2 两侧 libtiff 的能力对等

| | 宿主 | RIP 私有 |
|---|---|---|
| 版本 | **4.7.1** | 4.1.0 |
| 构建 | vcpkg `x64-windows`，**`features core`**（`vcpkg.json` 中 `default-features: false`） | 未知来源 |
| CRT | UCRT（`VCRUNTIME140` + `api-ms-win-crt-*`） | UCRT（同上） |
| 外部编解码依赖 | **无**（导入表无 `zlib1.dll` / `jpeg62.dll`） | **无**（同左） |
| 实际可用编解码 | LZW、PackBits、CCITT、raw | 同左 |

**含义**：两侧编解码能力对等，RIP 输出用的 LZW 在宿主版本里可用。
统一版本**不会**丢失 RIP 现在依赖的能力。

> 注意：`features core` 意味着宿主的 libtiff **没有** Deflate / JPEG / LZMA / WEBP / ZSTD 实现
> （名字串在编解码表里，但实现是 `NotConfigured` 桩）。若 RIP 未来需要这些编解码，
> 统一版本会立刻变成阻塞项——需要同步调整宿主 `vcpkg.json` 的 feature 集。

### 2.3 CRT 边界现状

`RipSlicer.dll` 是 MinGW GCC 13.1.0 构建、链 `msvcrt.dll`；两侧的 `tiff.dll` 都是 MSVC/UCRT 构建。
**也就是说 RIP 今天就已经在跨 CRT 调用一个 MSVC 构建的 libtiff，并且工作正常。**
这证明跨 CRT 的 C ABI 调用本身不是问题——问题只在所有权配对。统一版本不改变这一点。

---

## 3. 四个方案对比

| | (a) 静态链 4.1.0 | (b) 私有改名 | (c) 统一 4.7.1 动态 | **(d) 统一 4.7.1 + 静态链** |
|---|---|---|---|---|
| 做法 | libtiff 4.1.0 静态链进 `RipSlicer.dll` | `tiff.dll` → `rip_tiff_4_1_0.dll`，重建导入表 | 删掉私有 `tiff.dll`，改用宿主的 4.7.1 | 用宿主同源的 4.7.1 源码，静态链进 `RipSlicer.dll` |
| 加载期冲突 | 消除 | 消除 | 消除 | 消除 |
| **全局 handler 串扰** | 无 | 无 | **有**（§2.1） | 无 |
| 升级锁步 | 无 | 无 | **强耦合**：宿主升 libtiff → RIP 必须同步重编重验 | 无（各自独立升级） |
| 模块自包含性 | 保持 | 保持 | **破坏**：模块不再可单独部署，`hostRootDeploymentForbidden` 失效 | 保持 |
| 两模式一致性风险 | 无 | 无 | **有**：exe 模式加载 `modules/rip/tiff.dll`、进程内加载宿主的，若非同一构建则两模式产出可能不同 | 无（两模式都用同一份静态代码） |
| 需要重编 `RipSlicer.dll` | 是 | 是 | 是 | 是 |
| 需要移植代码 | 否 | 否 | 是（8 个稳定 API，风险低） | 是（同左） |
| 需要一份静态 libtiff | 是 | 否 | 否 | 是（宿主侧可用 vcpkg `x64-windows-static-md` 提供） |
| 包体积 | +约 1MB | 不变 | −约 700KB | +约 1MB |
| **私有 tiff.dll 的来源/许可证阻断（R-17）** | 仍在 | 仍在 | **解除** | **解除** |

---

## 4. 对"统一版本能否省掉问题"的直接回答

**能省掉的：**

- ✅ 加载期的同名模块冲突（R-01 的直接症状）
- ✅ 版本不一致导致的结构体布局风险
- ✅ 私有 `tiff.dll` 4.1.0 的来源、补丁与 SBOM 阻断（R-17 的一项）——改用宿主 vcpkg 构建后，
  来源、版本、许可证全部清晰可证

**省不掉的：**

- ❌ **重编 `RipSlicer.dll`**——统一版本同样需要重编，并不比静态链接省事
- ❌ 其余 16 项要求（资源目录默认值、取消、ABI 版本、禁止 `exit`/`printf`、退出码映射……）与 libtiff 无关
- ❌ 跨 CRT 的所有权配对规则（仍需写入合同）

**新引入的：**

- ⚠️ **进程级全局 handler 串扰**：已实测 RIP 调用 `TIFFSetErrorHandler` / `TIFFSetWarningHandler`，
  共用一份 `tiff.dll` 后会覆盖宿主的错误处理。这是把"加载期会崩"换成了"运行期悄悄串"，更难排查。
- ⚠️ **锁步升级负担**：宿主任何一次 libtiff 升级都要拉着 RIP 重编重验。
- ⚠️ **两模式可能不一致**：exe 模式用模块目录里的 `tiff.dll`、进程内模式用宿主已加载的那一份，
  只要不是同一构建就可能产出不同字节，直接违反 R-09。

---

## 5. 选型结论

**推荐 (d)：统一到宿主的 libtiff 4.7.1 源码，但静态链接进 `RipSlicer.dll`。**

理由：

1. 与 (c) 同样解除 R-17 的私有 `tiff.dll` 来源阻断——这是统一版本的**真正价值**；
2. 与 (a)/(b) 同样保持完全隔离：无全局串扰、无锁步、模块仍自包含、两模式共用同一份代码；
3. 移植成本就是 §2.1 的 8 个稳定 API，重编无论如何都躲不掉；
4. 宿主侧已具备 `x64-windows-static-md` 的 preset 轨道，可直接产出静态 libtiff 供 RIP 使用。

**若贵方坚持动态链接系统 libtiff（方案 c），必须额外接受三条条款**（写入 REQ-02 的 R-18）：

- C-1 **不得调用** `TIFFSetErrorHandler` / `TIFFSetWarningHandler` / `TIFFSetTagExtender` 等任何
  进程级全局设置函数；错误信息改由 `TIFFOpenExt` + `TIFFOpenOptionsSetErrorHandlerExtR`（4.5.0+）
  的**每句柄**机制获取，或完全放弃 libtiff 错误回调、只依赖返回值；
- C-2 模块目录内**必须**放置与宿主**完全相同的** `tiff.dll` 文件（同一构建产物，SHA-256 一致并记入模块清单），
  以保证 exe 模式与进程内模式使用同一份 libtiff；
- C-3 接受**锁步升级**：宿主 libtiff 版本或 feature 集变更时，RIP 必须在同一次发布中重编并重跑一致性测试。

**次选 (b) 私有改名**：改动量最小、保留已验证的 4.1.0 二进制行为，但解不掉 R-17 的许可证阻断。
若项目只在本地工程使用、不对外分发，(b) 是性价比最高的选择。

**不推荐 (a)**：相比 (d) 没有任何优势——同样要静态链接，却继续背着 4.1.0 的来源问题。

---

## 6. 决策待办

| 项 | 决策方 | 状态 |
|---|---|---|
| 选定 (a)/(b)/(c)/(d) | 双方共同 | **待定**，建议 (d) |
| 若选 (c)：接受 C-1/C-2/C-3 | RIP 侧 | 待回复 |
| 若选 (d)：宿主侧提供静态 libtiff 4.7.1 构建产物与构建脚本 | 切片软件侧 | 待排期 |
| 是否需要 Deflate/JPEG 等额外编解码 | RIP 侧 | **待回复**（影响宿主 `vcpkg.json` feature 集） |

---

## 7. 复核方法

```bash
# RipSlicer 实际用到哪些 libtiff 符号（本文 §2.1 的依据）
python scripts/pe_imports.py --imports-of tiff.dll rip_project/RIPDLL_YYYYMMDD/RipSlicer.dll
```

```bash
# 宿主 libtiff 版本与 feature 集
cat build-slicesoft/vcpkg_installed/x64-windows/lib/pkgconfig/libtiff-4.pc
cat build-slicesoft/vcpkg_installed/x64-windows/share/tiff/vcpkg_abi_info.txt
```

> `scripts/pe_imports.py` 目前尚未入库，是本次分析的一次性脚本；
> 若作为长期门禁应随 REQ-01 的 H-12 一并入库并接进 ctest。
