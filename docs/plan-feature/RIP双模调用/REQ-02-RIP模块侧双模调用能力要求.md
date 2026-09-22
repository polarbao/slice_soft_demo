# REQ-02 RIP 模块侧双模调用能力要求

> 目录：`docs/plan-feature/RIP双模调用/` ｜ 日期：2026-09-17 ｜ 版本：v2 ｜ 发起方：切片软件（SliceSoft）
> 读者：RIP 模块负责人与产品决策者
> 配套：[整体](README.md) ｜ [DECISION-01 libtiff 隔离方案](DECISION-01-libtiff隔离方案.md) ｜ [REQ-01 宿主侧](REQ-01-宿主侧双模调用能力要求.md)
>
> **本文用途：对外要求函。** 请按 §6 模板逐条回复「接受 / 拒绝 / 需修订 + 替代方案」。
>
> **目标**：同一份 RIP 实现，既能以 `rip_cli.exe` 子进程被调用，也能以 `RipSlicer.dll` 被宿主
> 直接装入进程内调用，**由宿主在运行时选择**，两种方式产出一致。
>
> **等级**：`MUST` = 不做则进程内模式不成立或存在正确性风险；`SHOULD` = 强烈建议。
> **本文自包含**：作答无需阅读切片项目其他文档（libtiff 方案细节见 DECISION-01）。
>
> **所有"现状"结论均来自对 `rip_project/RIPDLL_20260909` 的二进制与源码实测**，复核方法见 §7。

---

## 0. 先说三句话

1. **不是要你重做一套接口。** `rip_slicer.h` 已经是一套合格的 C ABI，`rip_cli.exe` 也已经是它的薄壳——
   PE 导入表实测显示 `rip_cli.exe` 只依赖 `KERNEL32.dll` / `msvcrt.dll` / `SHELL32.dll`，
   **不静态导入 `RipSlicer.dll`**，而是运行时显式加载。双模的骨架本来就成立。
2. **挡路的是两件烧在二进制里的事**（R-01 libtiff 隔离、R-03 资源目录默认值），
   改配置、换加载参数、加 manifest 都绕不过去。
3. **因此这份要求隐含一个前提：需要重新编译 `RipSlicer.dll`。**
   如果贵方当前不具备源码或可复现构建条件，请在第一轮就直接告知——
   那样双模不成立，我们会退回"exe 唯一"方案，不必再走后续流程。

> **v2 修订摘要**：
> ① R-01 展开为四个可选方案并给出推荐（详见 DECISION-01），其中包含"统一使用与宿主相同的 libtiff"；
> ② R-13 进度回调由 MUST 降为 SHOULD——实测宿主的进度机制是统计输出目录文件数，与调用方式无关，
>    贵方不提供回调也不影响进度显示；
> ③ 新增 R-18（仅当选择方案 c 时生效）；
> ④ 新增 §5 实施建议，给出可直接照抄的代码与参数表。

---

## 1. 能力要求总览

| 编号 | 等级 | 类别 | 能力 | 现状 |
|---|---|---|---|---|
| R-01 | MUST | 阻断 | libtiff 隔离（四方案择一，见 DECISION-01） | ✗ 存在 `tiff.dll` load-time 静态导入 |
| R-02 | MUST | 阻断 | 导入表通用名白名单 | △ 需全量核对 |
| R-03 | MUST | 阻断 | 资源目录默认值改为模块自身目录 | ✗ 当前基于宿主 exe 目录 |
| R-04 | MUST | 边界 | CRT 边界规则写入合同 | △ 事实成立，未成条款 |
| R-05 | MUST | 边界 | 进程内禁止行为清单 | ✗ 未约定 |
| R-06 | MUST | 边界 | 构造函数初始化全部成员 | ✗ `m_ripmode` 自述未初始化 |
| R-07 | MUST | 等价 | exe 仅调用公开 ABI，无独有逻辑 | ✓ 基本成立，需固化 |
| R-08 | MUST | 等价 | 默认值单一来源 | △ CLI 与库各有一份 |
| R-09 | MUST | 等价 | 两模式产出逐位一致 | ✗ 无证据 |
| R-10 | MUST | 等价 | 能力对等矩阵 | ✗ 无 |
| R-11 | MUST | 等价 | ABI 版本号函数与 CLI 查询 | ✗ 仅有版本字符串 |
| R-12 | MUST | 能力 | `rip_cancel` 取消接口 | ✗ 无 |
| R-13 | **SHOULD** | 能力 | 进度回调 | ✗ 仅有日志回调（宿主已有兜底） |
| R-14 | MUST | 能力 | 退出码一一映射 + 机器可读报告 | ✗ 九种错误塌成 1 |
| R-15 | SHOULD | 能力 | 内存与并发声明，内存不足不崩 | ✗ 未声明 |
| R-16 | SHOULD | 能力 | 长稳与重入 | ✗ 无证据 |
| R-17 | MUST | 交付 | 授权、来源与 SBOM 证据 | ✗ 仍阻断 |
| R-18 | 条件 | 边界 | 共享 libtiff 时的附加条款（仅方案 c） | — |

图例：`✓` 已具备 ｜ `△` 部分具备 ｜ `✗` 缺失

---

## 2. 现在已具备的能力（无需重做，请予保持）

| # | 能力 | 证据 |
|---|---|---|
| RC-1 | 纯 C ABI，不依赖 Qt 运行库；`extern "C"` + `__cdecl`；路径一律 UTF-8，中文路径可用 | `rip_slicer.h` 头部约定 |
| RC-2 | 不透明句柄 `rip_handle`，`rip_create` / `rip_destroy` 成对 | `rip_slicer.h:86-87` |
| RC-3 | 完整返回码体系（`RIP_OK` 与 `-1..-9`）+ `rip_error_string` + `rip_last_error` | `rip_slicer.h:39-48, 92-94` |
| RC-4 | 日志回调 `rip_set_log_callback`（等级 + UTF-8 消息 + user_data） | `rip_slicer.h:96` |
| RC-5 | 参数 setter 齐全：资源目录 / 双 ICC / intent / transparent(0..4) / colortexture / ripmode / 输入输出路径 | `rip_slicer.h:102-134` |
| RC-6 | 三种执行粒度：整目录 `rip_run`、单张 `rip_slicer_rgb2cmyk`、内存位图 `rip_convert_rgb2cmyk` | `rip_slicer.h:144-190` |
| RC-7 | 资源预加载可与处理分离：`rip_load_tables` | `rip_slicer.h:172` |
| RC-8 | 位图分配释放成对且同侧：`rip_create_*_bmp` / `rip_delete_bmp` | `rip_slicer.h:177-179` |
| RC-9 | 线程约定已明示：同一 handle 不可并发，不同 handle 可并行 | `rip_slicer.h` 头部 |
| RC-10 | **exe 是薄壳**：不静态导入 DLL，`--dll` 显式指定路径，`LOAD_WITH_ALTERED_SEARCH_PATH` 加载 | `rip_cli.c:4, 519-525`；PE 导入表实测 |
| RC-11 | MinGW 运行时静态链接：模块目录内无 `libgcc_s*.dll` / `libstdc++-6.dll` / `libwinpthread-1.dll` | 目录清单 + 导入表实测 |
| RC-12 | **对 libtiff 的依赖面极小**：只用 10 个符号，其中 8 个是 libtiff 4.x 全程稳定的核心 scanline API | 逐符号导入表解析，见 DECISION-01 §2.1 |

**这 12 项是双模的基础，改造过程中不得退化。**

---

## 3. 需整改的能力

### A 类：阻断级——不解决，进程内模式在物理上不可能

#### R-01（MUST）libtiff 隔离

**现状（实测）**：

| 二进制 | 导入表 |
|---|---|
| `RipSlicer.dll` | `KERNEL32.dll`、`msvcrt.dll`、**`tiff.dll`（load-time 静态导入）** |
| `rip_cli.exe` | `KERNEL32.dll`、`msvcrt.dll`、`SHELL32.dll` |
| 随附 `tiff.dll`（4.1.0） | `KERNEL32.dll`、`VCRUNTIME140.dll`、`api-ms-win-crt-*`（UCRT / MSVC 构建） |

宿主进程里**已经加载了一个同名 `tiff.dll`**（LibTIFF 4.7.1）。Windows 加载器对已加载模块
**按基名解析导入**：`RipSlicer.dll` 的 `tiff.dll` 导入会被绑到宿主那一份，
`LoadLibraryEx` 传绝对路径、`LOAD_WITH_ALTERED_SEARCH_PATH`、`LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`
**一律无效**。后果：轻则导出缺失、加载直接失败；重则符号名对上但两版 libtiff 行为不同，
静默产出错误结果——进程内模式下这会直接带走整个切片软件。

**要求：四选一**，完整对比见 [DECISION-01](DECISION-01-libtiff隔离方案.md)：

| 方案 | 做法 | 我方评价 |
|---|---|---|
| (a) | libtiff 4.1.0 **静态链接**进 `RipSlicer.dll` | 可接受 |
| (b) | `tiff.dll` 改名为 `rip_tiff_4_1_0.dll` 等私有名，重建导入表 | 可接受，改动量最小 |
| (c) | 删掉私有 `tiff.dll`，改用宿主的 **4.7.1 动态库** | 可接受，但**必须同时接受 R-18** |
| **(d)** | **用与宿主同源的 4.7.1 源码，静态链接进 `RipSlicer.dll`** | **推荐** |

**为什么推荐 (d)**：贵方的移植面只有 8 个稳定 API（RC-12），4.1.0→4.7.1 风险很低；
静态链接后完全隔离、无需与宿主锁步升级、模块保持自包含；
同时把私有 `tiff.dll` 4.1.0 的来源与许可证阻断（R-17 第 4 项）一并解除。

**不接受**：SxS / 激活上下文 / manifest 私有程序集——该机制在"被宿主动态 `LoadLibrary` 装载"的
场景下不可靠，且无法在我方验证。

**验收**：提供重编后 `RipSlicer.dll` 的完整导入表，其中不含 `tiff.dll`（方案 c 除外）。

---

#### R-02（MUST）导入表通用名白名单

**要求**：`RipSlicer.dll` 与 `rip_cli.exe` 的导入表中，**不得出现任何宿主也可能加载的通用库名**。
至少包括：`zlib1.dll`、`jpeg62.dll`、`turbojpeg.dll`、`liblzma.dll`、`lcms2.dll`、
`libpng16.dll`、`libcurl.dll`、`openjp2.dll`（以及 `tiff.dll`，方案 c 除外）。

**验收**：随每次 SDK 投放提供两个二进制的完整导入表清单，作为模块包的一部分归档。

---

#### R-03（MUST）资源目录默认值改为模块自身目录

**现状（实测）**：`rip_slicer.h:83-84` 明写——

> 资源目录默认为 `<宿主 exe 所在目录>/CmykFiles`（等价于 Qt 的 `QCoreApplication::applicationDirPath() + "/CmykFiles"`）

exe 模式下这个默认值是对的（exe 就躺在模块目录里）；**DLL 模式下它会指向宿主程序目录**，
而 `CmykFiles` 在模块目录里，直接找不到线性化表与挂网矩阵。

**要求（二选一）**：

- **(a)** 默认值改为"**包含本模块代码的那个模块**所在目录"（代码见 §5.1），exe 与 DLL 两种模式都正确；
- **(b)** 取消隐式默认：未显式调用 `rip_set_resource_dir` 即返回 `RIP_ERR_RESOURCE`。

我们倾向 **(a)**：一处改动同时修好两种模式，且不改变现有 CLI 行为。

---

#### R-04（MUST）CRT 边界规则写入合同

**现状（实测）**：`RipSlicer.dll` 链 `msvcrt.dll`（MinGW GCC 13.1.0 的默认 CRT），
宿主与其 `tiff.dll` 链 UCRT（`VCRUNTIME140` + `api-ms-win-crt-*`）。
**我们不要求统一 CRT**——跨 CRT 完全可行（贵方自己的 MinGW DLL 调 MSVC 构建的 `tiff.dll`
就跑得很好），但必须把"为什么可行"变成可验收条款，否则进程内模式下一次违例就是堆损坏。

**要求**：书面确认以下四条并在头文件中注明：

1. 谁分配谁释放，成对同侧；宿主永不对库返回的指针调 `free`/`delete`
   （现有 `rip_create_*_bmp` / `rip_delete_bmp` 即正确范式）；
2. 不跨边界传递 `FILE*`、CRT 文件描述符、`errno` 语义、C++ 标准库容器或字符串对象；
3. **不得有 C++ 异常或 SEH 越过 DLL 边界**，内部异常一律转错误码（贵方文档已有此表述，请升级为验收条款）；
4. 结构体布局（如 `rip_bitmap`）一经发布不得变更，变更必须升 ABI 版本（R-11）。

---

#### R-05（MUST）进程内禁止行为清单

同一份代码在 exe 里无害、在 DLL 里致命的行为，必须明确禁止：

| 行为 | exe 模式 | 进程内要求 |
|---|---|---|
| `exit()` / `abort()` / `std::terminate` / `assert` 弹窗 | 可接受 | **禁止**，一律转错误码返回 |
| 写 `stdout` / `stderr` | 现在就是这么做的 | **禁止**（会污染宿主输出），诊断只走日志回调 |
| `MessageBox` 或任何 UI | 可接受 | **禁止** |
| `TIFFSetErrorHandler` / `TIFFSetWarningHandler` | 可接受 | **方案 c 下禁止**（见 R-18）；其余方案下因 libtiff 私有化而无害 |
| `setlocale` / `SetCurrentDirectory` / `SetDllDirectory` | 可接受 | **禁止** |
| `signal` / `SetUnhandledExceptionFilter` | 可接受 | **禁止** |
| 修改 FPU / MXCSR 控制字不还原 | 可接受 | **禁止** |
| `DllMain` 中的重活（加载其他库、建线程、同步等待） | — | **禁止**，避免 loader lock 死锁 |
| `FreeLibrary` 后再次 `LoadLibrary` | — | **必须**支持：无泄漏、无残留静态状态、行为与首次一致 |

---

#### R-06（MUST）构造函数初始化全部成员

**现状（实测）**：`rip_slicer.h:127-129` 自述——`m_ripmode` 在原 Qt 工程里"只声明了成员、
没有 setter，构造函数也未初始化"，且"其它取值下 CMYK 通道会保持全 0"。
未初始化成员 + 静默产出全 0 的组合，在进程内模式下尤其危险：它不会报错，只会产出错误结果。

**要求**：所有影响输出的成员在构造函数中初始化为文档声明的默认值；
所有 setter 的**值域外输入必须返回 `RIP_ERR_ARG`**，不得静默按某个分支处理。

---

### B 类：双模等价性——"由宿主选择"的前提是两条路等价

#### R-07（MUST）exe 仅调用公开 ABI

**现状**：基本成立（`rip_cli.c` 通过 `GetProcAddress` 取到的函数表调用）。

**要求**：固化为条款——`rip_cli.exe` 不得包含任何只存在于 exe 的处理逻辑、参数修正或补偿。
exe 能做到的每一件事，宿主用公开 ABI 必须也能做到。

#### R-08（MUST）默认值单一来源

**现状（实测）**：`rip_cli.c:540-553` 在未指定 `--rgb-icc` / `--cmyk-icc` 时，自行拼接
`<资源目录>/CIERGB.icc` 与 `<资源目录>/CMYK.icc`；而库构造函数里**也有**同一组默认值。
两处目前恰好一致，但这是两个独立的真相来源——任何一边改动都会立刻产生模式差异。

**要求**：默认值只保留在库内一处；CLI 不再自行拼接，未指定即沿用库默认。

#### R-09（MUST）两模式产出逐位一致

**要求**：同一输入目录 + 同一参数集，两种调用方式产出的 TIFF **逐字节相同**。
允许差异的字段（如写入时间戳标签）必须显式列举并说明原因。

**验收**：贵方提供可在我方复跑的一致性测试（输入 fixture + 命令 + 比对脚本 + 一次真实运行报告）。

#### R-10（MUST）能力对等矩阵

**要求**：交付一张对照表，逐行给出
`C 接口 ↔ CLI 参数 ↔ 默认值 ↔ 有效值域 ↔ 越界行为 ↔ 是否影响输出`。
当前已知需覆盖：`intent 0..3`、`transparent 0..4`、`colormode`、`ripmode 0|1`、
`resource_dir`、`input_icc` / `output_icc`、`input_path` / `output_path`、`keep-going`、`number`、`quiet`。

#### R-11（MUST）ABI 版本号函数与 CLI 查询

**现状**：只有 `rip_version()` 返回形如 `"1.0.0"` 的字符串，无法做机器判定。

**要求**：

1. 新增 `RIP_API int RIP_CALL rip_abi_version(void)`，返回**整数**；ABI 破坏性变更必须递增；
2. CLI 增 `--abi-version`（或 `--version --json`）输出同一数值；
3. 两种模式报出的版本号必须一致，并与模块清单 `rip_module.json` 的 `version` 对应。

我方装载后会先查该值，不匹配即 fail-closed 拒绝加载。

---

### C 类：为"可切换"必须补的能力

#### R-12（MUST）`rip_cancel` 取消接口

**现状**：导出表中无任何取消手段。exe 模式下我方靠 `terminate()` → 2 秒 → `kill()` 兜底；
**进程内模式下没有任何等价手段**——`rip_run` 一旦阻塞，整个软件只能重启。

**要求**：

```c
/* 可从其它线程调用；对同一 handle 与 rip_run 并发安全 */
RIP_API int RIP_CALL rip_cancel(rip_handle h);
```

1. 层与层之间、以及单层内部的长耗时环节，都要有协作检查点（实现建议见 §5.2）；
2. 取消后 `rip_run` / `rip_run_continue_on_error` 返回专用码（建议新增 `RIP_ERR_CANCELLED = -10`）；
3. **明确声明**取消后是否可能留下半张已写文件——若会，我方按现有 staging 规则清理，但必须知情；
4. 给出"从调用 `rip_cancel` 到 `rip_run` 返回"的时间上限承诺。

#### R-13（SHOULD）进度回调

**现状与降级理由（v2）**：我方进度并非解析贵方 stdout，而是**每 500ms 统计输出目录里的
TIFF 文件数**。该机制与调用方式无关，进程内模式同样适用。因此贵方**不提供进度回调也不影响
进度显示**，本条由 MUST 降为 SHOULD。

**仍然建议提供**，用于细化阶段信息（文件计数无法反映"正在加载挂网表"这类阶段）：

```c
typedef void (RIP_CALL *rip_progress_fn)(int done, int total, int phase, void* user_data);
RIP_API int RIP_CALL rip_set_progress_callback(rip_handle h, rip_progress_fn fn, void* user_data);
```

`phase` 至少区分"加载资源 / 处理层 / 收尾"。回调必须在处理线程同步调用，不得自行起线程投递。

#### R-14（MUST）退出码一一映射 + 机器可读报告

**现状（实测）**：`rip_cli.c:625` 是 `return rc == RIP_OK ? 0 : 1;`——
`RIP_ERR_HANDLE` 到 `RIP_ERR_INTERNAL` 共 **9 种错误码全部塌成退出码 1**；
另有参数解析失败 `return 2`（`rip_cli.c:516`）、DLL 加载失败 `return 1`（`:525`）。
这使 exe 模式天然比 DLL 模式少一整层诊断信息，两模式无法等价。

**要求**：① 退出码与 `RIP_ERR_*` **一一映射**（现成映射表见 §5.3）；
② 新增 `--report <file.json>`，输出机器可读结果（样例见 §5.3）。

#### R-15（SHOULD）内存与并发声明，内存不足不崩

**要求**：

1. 给出单层峰值内存与「宽 × 高 × 通道数」的关系式，以及挂网 LUT（自述约 12MB）的一次性开销；
2. 声明多 handle 并行上限与内部是否起线程——进程内模式下这直接影响宿主线程模型；
3. **内存不足必须返回 `RIP_ERR_MEMORY`**，不得 `abort` 或让 `bad_alloc` 越过边界。

#### R-16（SHOULD）长稳与重入

**要求**：连续 N 次 `create → 设参 → run → destroy` 不泄漏、不累积状态；
同一进程内连续处理多个作业，第 N 次结果与第 1 次一致（不受上次参数残留影响）。
请提供一次长稳运行的证据。

---

### D 类：条件条款与交付

#### R-18（条件 MUST，仅当 R-01 选择方案 c）共享 libtiff 的附加条款

若贵方选择"直接使用宿主的 libtiff 4.7.1 动态库"，则以下三条同时生效：

| 编号 | 条款 | 原因 |
|---|---|---|
| C-1 | **不得调用** `TIFFSetErrorHandler` / `TIFFSetWarningHandler` / `TIFFSetTagExtender` 等任何进程级全局设置函数 | 实测贵方当前**正在调用**前两个（在 `RipSlicer.dll` 导入表里）。共用一份 libtiff 后，这会**直接覆盖宿主的错误处理**——把"加载期崩溃"换成"运行期悄悄串扰"，更难排查。改用 `TIFFOpenExt` + 每句柄错误处理（libtiff 4.5.0+），或完全放弃 libtiff 回调、只依赖返回值 |
| C-2 | 模块目录内**必须**放置与宿主**完全相同的** `tiff.dll` 文件（同一构建产物，SHA-256 一致并记入模块清单） | exe 模式加载模块目录里的 `tiff.dll`、进程内模式加载宿主已加载的那一份。两者若非同一构建，两种模式可能产出不同字节，直接违反 R-09 |
| C-3 | 接受**锁步升级**：宿主 libtiff 版本或 feature 集变更时，RIP 必须在同一次发布中重编并重跑一致性测试 | 模块不再自包含，`hostRootDeploymentForbidden` 失效 |

> 补充信息：宿主的 libtiff 是 vcpkg `x64-windows` + **`features core`** 构建，
> **没有** Deflate / JPEG / LZMA / WEBP / ZSTD 实现（名字在编解码表里，但实现是 `NotConfigured` 桩）。
> 实测贵方当前的 4.1.0 也没有这些外部编解码依赖，LZW 两侧均可用。
> **若贵方未来需要这些编解码，请在回复中说明**——那将需要我方同步调整 `vcpkg.json` 的 feature 集。

#### R-17（MUST）授权、来源与 SBOM 证据

与双模无关，但同样阻断对外交付（当前模块清单中 `redistributionStatus` 仍为
`BLOCKED_EXTERNAL_LICENSE_EVIDENCE`）：

1. `RipSlicer.dll` / `rip_cli.exe` 的所有权、版本来源与再分发授权；
2. lcms2 的版本、链接方式与许可证；
3. `CmykFiles` 下 ICC 配置文件（`CIERGB.icc`、`CMYK.icc`、`JapanColor2001Coated.icc`）的来源与再分发授权；
4. 私有 `tiff.dll` 4.1.0 的确切构建来源、补丁与 SBOM 绑定——
   **若 R-01 选择方案 (c) 或 (d)（统一到宿主的 4.7.1），本项自动解除。**

---

## 4. 交付与验收清单

随 SDK 一并提供，缺项即无法验收：

- [ ] 重编后两个二进制的**完整导入表**（R-01 / R-02）
- [ ] 更新后的 `rip_slicer.h`，含 `rip_abi_version` / `rip_cancel`（及可选的 `rip_set_progress_callback`）
- [ ] 双模**逐位一致性测试**与一次真实运行报告（R-09）
- [ ] **能力对等矩阵**表（R-10）
- [ ] **退出码映射表**与 `--report` 样例输出（R-14）
- [ ] 行为矩阵：取消 / 超时 / 内存不足 / 坏 TIFF / tiled 输入，两模式各一列
- [ ] 资源占用与并发声明（R-15）、长稳运行证据（R-16）
- [ ] 调试符号（PDB 或等价）或可用的崩溃定位手段
- [ ] 授权与 SBOM 证据（R-17）
- [ ] 若选方案 (c)：`tiff.dll` 的 SHA-256 与宿主一致的证明（R-18 C-2）

---

## 5. 实施建议（可直接照抄）

以下为我方给出的参考实现，**不是强制方案**，贵方可用等效手段满足要求。

### 5.1 R-03：模块自身目录解析

一处改动同时修好 exe 与 DLL 两种模式：

```c
/* 取"包含本函数代码的那个模块"所在目录 —— exe 模式得到 exe 目录，
   DLL 模式得到 DLL 目录，正是 CmykFiles 所在处。 */
static void rip_module_dir(char* out_utf8, size_t out_size)
{
    HMODULE self = NULL;
    wchar_t path[MAX_PATH];
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(void*)&rip_module_dir, &self);
    GetModuleFileNameW(self, path, MAX_PATH);
    /* 去掉文件名后转 UTF-8 写入 out_utf8 */
}
```

### 5.2 R-12：取消的实现要点

```c
/* rip_ctx 内增加一个原子标志 */
RIP_API int RIP_CALL rip_cancel(rip_handle h)
{
    if (!h) return RIP_ERR_HANDLE;
    InterlockedExchange(&h->cancel_flag, 1);
    return RIP_OK;
}
```

检查点布置：

1. **每张图之间**——最关键，保证取消延迟不超过一层的处理时间；
2. **单层内部的 scanline 循环里每 64 行检查一次**——开销可忽略，
   避免大尺寸单层导致取消延迟过长；
3. 命中取消后：先 `TIFFClose` 当前句柄，再决定是否删除半成品，最后返回 `RIP_ERR_CANCELLED`。

### 5.3 R-14：退出码映射表与报告格式

建议 `exit = 10 + |rc|`，与既有的 0 / 2 不冲突：

| 退出码 | 含义 | 对应返回码 |
|---|---|---|
| 0 | 成功 | `RIP_OK` |
| 2 | 用法 / 参数错误（保持现状） | — |
| 3 | 加载 `RipSlicer.dll` 失败（与 RIP 错误区分开） | — |
| 11 | 句柄无效 | `RIP_ERR_HANDLE` (-1) |
| 12 | 参数非法 | `RIP_ERR_ARG` (-2) |
| 13 | 资源缺失 / 格式错 | `RIP_ERR_RESOURCE` (-3) |
| 14 | 目录 / 文件读写失败 | `RIP_ERR_IO` (-4) |
| 15 | TIFF 读写失败 | `RIP_ERR_TIFF` (-5) |
| 16 | 输入不受支持 | `RIP_ERR_UNSUPPORTED` (-6) |
| 17 | ICC 打开 / 转换失败 | `RIP_ERR_ICC` (-7) |
| 18 | 内存不足 | `RIP_ERR_MEMORY` (-8) |
| 19 | 其它内部异常 | `RIP_ERR_INTERNAL` (-9) |
| 20 | 已取消 | `RIP_ERR_CANCELLED` (-10，新增) |

`--report <file.json>` 格式：

```json
{
  "abiVersion": 2,
  "version": "1.2.0",
  "returnCode": 0,
  "ok": 175,
  "failed": 0,
  "elapsedMs": 142031,
  "layers": [
    { "index": 0, "input": "layer_000000.tif", "output": "slice.0.tiff", "code": 0, "ms": 812 }
  ]
}
```

### 5.4 R-05：用编译期开关堵住 `printf`

库的 DLL 构建中加一行，可在编译期抓出所有残留的直接输出：

```c
#ifdef RIP_SLICER_BUILD
#  define printf   RIP_DO_NOT_USE_printf_IN_LIBRARY
#  define fprintf  RIP_DO_NOT_USE_fprintf_IN_LIBRARY
#endif
```

### 5.5 R-01 方案 (a)/(d)：静态链接时务必限制导出

静态链接 libtiff 后，**必须确保 `TIFF*` 符号不被一并导出**，否则等于制造了新的名字冲突源：

- MinGW：`-Wl,--exclude-all-symbols` 配合 `.def` 文件，只导出 `rip_*`；
- 或在链接脚本 / `.def` 中显式列出导出清单。

**验收方式**：导出表中只应有 `rip_` 前缀的符号。

### 5.6 R-11：ABI 版本常量

```c
#define RIP_ABI_VERSION 2   /* 当前发布视为 1；本轮改造后为 2 */
RIP_API int RIP_CALL rip_abi_version(void) { return RIP_ABI_VERSION; }
```

任何结构体布局、调用约定或参数语义变更都必须递增该值。

---

## 6. 回复模板

请复制下表逐条填写，一次性回复：

```text
前置问题（请务必先答）：
Q1 是否具备 RipSlicer.dll 的源码与可复现构建条件？（否 → 双模不成立，我方退回 exe 唯一方案）
Q2 R-01 选择哪个方案？ (a) 静态链 4.1.0 / (b) 私有改名 / (c) 统一 4.7.1 动态 / (d) 统一 4.7.1 静态链【推荐】
Q3 是否需要 Deflate / JPEG / LZMA / WEBP / ZSTD 等编解码？（影响我方 vcpkg feature 集）
Q4 预计交付周期与可投放的 RIPDLL_YYYYMMDD 版本号？

R-01 libtiff 隔离        ：方案 __ / 拒绝 / 需修订 —— 说明：
R-02 导入表通用名白名单   ：接受 / 拒绝 / 需修订 —— 说明：
R-03 资源目录默认值      ：接受(方案 a / b) / 拒绝 / 需修订 —— 说明：
R-04 CRT 边界规则        ：接受 / 拒绝 / 需修订 —— 说明：
R-05 进程内禁止行为清单   ：接受 / 拒绝 / 需修订 —— 说明：
R-06 构造函数初始化      ：接受 / 拒绝 / 需修订 —— 说明：
R-07 exe 仅调用公开 ABI  ：接受 / 拒绝 / 需修订 —— 说明：
R-08 默认值单一来源      ：接受 / 拒绝 / 需修订 —— 说明：
R-09 双模逐位一致        ：接受 / 拒绝 / 需修订 —— 说明：
R-10 能力对等矩阵        ：接受 / 拒绝 / 需修订 —— 说明：
R-11 ABI 版本号          ：接受 / 拒绝 / 需修订 —— 说明：
R-12 rip_cancel          ：接受 / 拒绝 / 需修订 —— 说明：
R-13 进度回调（SHOULD）  ：接受 / 拒绝 / 需修订 —— 说明：
R-14 退出码映射 + 报告    ：接受 / 拒绝 / 需修订 —— 说明：
R-15 内存与并发声明      ：接受 / 拒绝 / 需修订 —— 说明：
R-16 长稳与重入          ：接受 / 拒绝 / 需修订 —— 说明：
R-17 授权与 SBOM         ：接受 / 拒绝 / 需修订 —— 说明：
R-18 共享 libtiff 附加条款（仅当 Q2 选 c）：接受 C-1/C-2/C-3 / 拒绝 —— 说明：
```

---

## 7. 复核方法

本文所有实测结论均可复算：

```bash
# 导入表与逐符号依赖（R-01 / R-02 / RC-12 的判定依据）
dumpbin /dependents rip_project/RIPDLL_YYYYMMDD/RipSlicer.dll
dumpbin /imports rip_project/RIPDLL_YYYYMMDD/RipSlicer.dll
```

```bash
# 宿主侧同名模块与 libtiff 版本
ls build-slicesoft/main/Release/tiff.dll
cat build-slicesoft/vcpkg_installed/x64-windows/lib/pkgconfig/libtiff-4.pc
```

源码依据：`rip_project/RIPDLL_20260909/rip_slicer.h`（接口与默认值自述）、
`rip_project/RIPDLL_20260909/rip_cli.c`（显式加载、默认值拼接、退出码）。
