# RIP 双模调用专项（整体）

> 目录：`docs/plan-feature/RIP双模调用/` ｜ 日期：2026-09-17 ｜ 发起方：切片软件（SliceSoft）
>
> 目标：让 RIP 既能以**子进程 exe**调用，也能以**进程内 DLL**调用，**由宿主在运行时选择**。
>
> 本目录五份文档：
>
> | 文档 | 读者 | 内容 |
> |---|---|---|
> | 本文 | 双方 + 决策者 | 整体现状、责任分界、阻断链、推进顺序 |
> | [DECISION-01 libtiff 隔离方案](DECISION-01-libtiff隔离方案.md) | 双方 + 决策者 | 四方案对比与选型；回答"统一 libtiff 版本能否省掉问题" |
> | [REQ-01 宿主侧双模调用能力要求](REQ-01-宿主侧双模调用能力要求.md) | 切片软件研发 | 宿主要满足什么、已具备什么、需整改什么、**怎么改** |
> | [REQ-02 RIP 模块侧双模调用能力要求](REQ-02-RIP模块侧双模调用能力要求.md) | RIP 供方 | RIP 要满足什么、已具备什么、需整改什么、**怎么改** |
> | [任务清单](TASKS_RIP双模调用任务清单.md) | 切片软件研发 | H-xx / R-xx 汇总、阻断链、推进阶段与当前状态 |
>
> **证据等级**：`A` = 当前代码/二进制实测，可直接作为实施依据；`B` = 目标设计，尚未实现。
> 本目录所有"现状"条目均为 A 级，来源在条目内逐条标注。

---

## 1. 一句话现状

**双模的骨架已经存在，缺的不是 API 形态，而是三件物理障碍与两侧各自的工程件。**

`rip_slicer.h` 已经是一套不依赖 Qt 的纯 C ABI，`rip_cli.exe` 也已经是它的薄壳（PE 导入表实测：
`rip_cli.exe` 只依赖 `KERNEL32.dll` / `msvcrt.dll` / `SHELL32.dll`，**不静态导入 `RipSlicer.dll`**，
而是运行时 `LoadLibraryEx` + `GetProcAddress`）。"同一份实现、两种承载"在结构上本来就成立。

挡住进程内模式的是三条硬约束：

| # | 障碍 | 实测证据 | 后果 |
|---|---|---|---|
| 1 | `RipSlicer.dll` 对 `tiff.dll` 是 **load-time 静态导入**，而宿主进程里已有同名 `tiff.dll`（LibTIFF 4.7.1，vcpkg `features core`） | 两侧 PE 导入表；`build-slicesoft/main/Release/tiff.dll` | 加载器按基名解析已加载模块，RIP 被强制绑到宿主那一份 |
| 2 | 资源目录默认值是 `<宿主 exe 所在目录>/CmykFiles` | `rip_project/RIPDLL_20260909/rip_slicer.h:83` | exe 模式下正确，DLL 模式下指向宿主目录，直接找不到 `CmykFiles` |
| 3 | 无取消接口；退出码信息坍缩 | 导出表无 cancel；`rip_cli.c:625` 是 `return rc == RIP_OK ? 0 : 1;` | 进程内没有 `terminate()` 可用，卡住无法停；9 种错误码在 exe 模式塌成 1 |

障碍 1 和 2 的修复都**烧在二进制里**（导入表的库名、默认目录的解析逻辑），
改配置、换加载参数、加 manifest 都绕不过去——**必须重编 `RipSlicer.dll`**。

---

## 2. 关于 libtiff：统一版本能省掉问题吗

**能省掉加载期冲突，省不掉重编，而且会新引入运行期串扰。** 完整分析见
[DECISION-01](DECISION-01-libtiff隔离方案.md)，这里给三条关键事实：

1. **依赖面极小**：`RipSlicer.dll` 从 `tiff.dll` 只取用 **10 个符号**，其中 8 个是 libtiff 4.x
   全程稳定的核心 scanline API（`TIFFOpenW`/`TIFFClose`/`TIFFGetField`/`TIFFSetField`/
   `TIFFReadScanline`/`TIFFWriteScanline`/`TIFFScanlineSize`/`TIFFDefaultStripSize`）。
   4.1.0 → 4.7.1 的移植风险很低。
2. **但另外 2 个是进程级全局**：`TIFFSetErrorHandler` 与 `TIFFSetWarningHandler`。
   今天两份独立的 `tiff.dll` 各有各的全局量，互不干扰；**共用一份之后 RIP 会覆盖宿主的错误处理**。
   统一版本等于把"加载期会崩"换成"运行期悄悄串"。
3. **能力对等**：两侧 libtiff 都是 core-only 构建（导入表均无 `zlib1.dll`），
   RIP 输出所用的 LZW 在宿主版本里可用，统一不会丢能力。

**推荐方案 (d)：统一到宿主的 libtiff 4.7.1 源码，但静态链接进 `RipSlicer.dll`。**
它同时拿到完全隔离、零锁步、零全局串扰，并额外解除私有 `tiff.dll` 的来源与许可证阻断。

---

## 3. 目标链路

```text
宿主（slicer_ui_host_sim，MSVC）
  └─ IRipBackend（新增抽象，运行时按 executionMode 选择）
       ├─ RipProcessBackend  ── QProcess ──> modules/rip/rip_cli.exe
       │                                        └─ LoadLibraryEx(RipSlicer.dll)
       └─ RipLibraryBackend  ── LoadLibraryEx ──> modules/rip/RipSlicer.dll
                                                    （同进程，无子进程）
  两条路径共用：输入校验 → 执行 → 输出逐层校验 → 同父原子发布
```

**不变量：无论走哪条路径，输入校验、输出逐层校验、发布规则完全相同。**
进程内模式下没有 `exitCode` 可用，更不能用"函数返回 `RIP_OK`"替代逐层校验。

---

## 4. 责任分界

| 能力 | 宿主侧 | RIP 侧 | 说明 |
|---|---|---|---|
| 选择执行方式 | ● 唯一决定方 | ○ 两种都必须可用 | RIP 不得假设自己跑在哪一侧 |
| 路径解析与围栏 | ● | ○ 只接受绝对路径 | 现有 `RipCommandBuilder` 的围栏规则对两模式同样适用 |
| 依赖隔离 | ○ 自检 | ● 根本解决 | 宿主只能检测冲突，消除冲突只能靠重编 |
| 取消 / 超时 | ● 触发与兜底 | ● 提供 `rip_cancel` | 进程内模式宿主无强杀手段，必须靠 RIP 协作 |
| 进度 | ● 已有通用机制 | ○ 可选细化 | 宿主按 staging 文件数计进度，与后端无关；RIP 回调仅用于阶段细化 |
| 输出正确性 | ● 逐层校验 | ○ 产出一致 | 校验权永远在宿主，不下放 |
| 双模逐位一致 | ○ 验证 | ● 保证 | 供方提供可复跑的一致性测试 |
| 授权与 SBOM | ○ 归档 | ● 提供 | 与双模无关但同样卡对外交付 |

---

## 5. 阻断链（谁等谁）

```text
DECISION-01 选定 libtiff 方案 ─┐
R-01 消除同名冲突              ─┤
R-03 资源目录默认值            ─┼─> 进程内模式在物理上可行
R-11 ABI 版本函数              ─┤          │
R-12 rip_cancel                ─┘          ├─> H-03 宿主 RipLibraryBackend 可以开工
                                           └─> H-12 双后端等价性测试可以落地
```

**宿主侧 H-01 / H-02（后端抽象与模式开关）不依赖 RIP 侧，可立即开工；
H-03 及其后续必须等 RIP 侧交付重编版本。** 建议按此顺序推进，避免宿主先写好一个无法加载的后端。

---

## 6. 推荐推进顺序

| 阶段 | 内容 | 依赖 |
|---|---|---|
| S0 | 把 REQ-02 与 DECISION-01 作为确认函发给 RIP 供方，逐条回复；**同时确认供方是否有源码与可复现构建** | — |
| S1 | 宿主侧 H-01 / H-02：抽出 `IRipBackend`，现有 QProcess 路径原样落到 `RipProcessBackend`，行为零变化 | 无 |
| S2 | 双方共同选定 libtiff 方案（DECISION-01 §6） | S0 |
| S3 | RIP 侧交付重编后的 SDK（R-01 / R-03 / R-11 / R-12 / R-14） | S2 |
| S4 | 宿主侧 H-03 / H-04 / H-08 / H-10：实现 `RipLibraryBackend` + 启用前自检 + 自动回落 | S1、S3 |
| S5 | H-12 双后端等价性测试 + R-09 逐位一致报告，双方互认 | S4 |
| S6 | 默认仍为 `process`，进程内模式作为可选项开放 | S5 |

**若供方无法重编**：双模不成立，退化为"exe 唯一"。这一点应在 S0 就问清楚，避免两轮往返后才发现。

---

## 7. 复核方法（任何人都可复算）

```bash
# 导入表与逐符号依赖（R-01 / R-02 / DECISION-01 的判定依据）
python scripts/pe_imports.py rip_project/RIPDLL_YYYYMMDD/RipSlicer.dll
```

```bash
# 宿主 libtiff 版本与 feature 集
cat build-slicesoft/vcpkg_installed/x64-windows/lib/pkgconfig/libtiff-4.pc
cat build-slicesoft/vcpkg_installed/x64-windows/share/tiff/vcpkg_abi_info.txt
```

> 注：`scripts/pe_imports.py` 目前尚未入库，是本次分析时的一次性脚本；
> 若要作为长期门禁，应随 REQ-01 的 H-12 一并入库并接进 ctest。

---

## 8. 归位与状态

| 项 | 内容 |
| --- | --- |
| 原位置 | `docs/rip-dual-mode/`（2026-09-17 建，一直未入库） |
| 现位置 | `docs/plan-feature/RIP双模调用/` |
| 归位日期 | 2026-09-22 |
| 当前状态 | **S0 未启动**。四份文档均为分析与要求，尚未发出确认函，两侧均未开工 |

本目录此前以未跟踪文件的形式躺在工作树里五天，既不在任何分支上，也不会随检出发给别人——
这类"写完就丢在盘上"的分析文档是最容易凭空消失的一种。归位到 `docs/plan-feature/`
之后它才真正进了仓。

### 与其它两个 RIP 专项的关系

| 专项 | 管什么 | 与本专项的关系 |
| --- | --- | --- |
| [切片后外置 RIP 集成](../切片后外置RIP集成/README.md) | 宿主怎么调 RIP、怎么验产出、怎么发布 | 本专项要换的是它的**执行后端**，校验与发布规则原样沿用 |
| [RIP 模块版本适配](../RIP模块20260917与20260920版本适配/README.md) | 供方每次给新 SDK 时的适配与取证 | 本专项 S3 要的"重编版本"会走它的流程落地 |

三者的分工是：集成专项定**规则**，版本适配专项管**来料**，本专项换**承载形态**。
