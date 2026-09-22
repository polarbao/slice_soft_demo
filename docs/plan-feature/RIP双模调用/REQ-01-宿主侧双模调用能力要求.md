# REQ-01 宿主侧双模调用能力要求

> 目录：`docs/plan-feature/RIP双模调用/` ｜ 日期：2026-09-17 ｜ 版本：v2 ｜ 读者：切片软件研发
> 配套：[整体](README.md) ｜ [DECISION-01 libtiff 隔离方案](DECISION-01-libtiff隔离方案.md) ｜ [REQ-02 RIP 模块侧](REQ-02-RIP模块侧双模调用能力要求.md)
>
> **范围**：宿主（`slicer_ui_host_sim` + `src/rip_integration`）为支持"exe 子进程 / 进程内 DLL
> 两种调用方式、由宿主运行时选择"所需的能力。
>
> **等级**：`MUST` = 不做则双模不成立或存在正确性风险；`SHOULD` = 强烈建议，可排后。
> **证据等级**：`A` = 当前代码实测。本文所有"现状"条目为 A 级并标注出处。

---

## 1. 能力要求总览

| 编号 | 等级 | 能力 | 现状 | 处置 |
|---|---|---|---|---|
| H-01 | MUST | 执行后端抽象 `IRipBackend` | ✗ 无，控制器直接 `new QProcess` | 新增 |
| H-02 | MUST | `executionMode` 设置项与清单声明 | ✗ 无该字段 | 新增 |
| H-03 | MUST | `RipLibraryBackend`（进程内 DLL 客户端） | ✗ 不存在 | 新增 |
| H-04 | MUST | 统一取消语义（两模式同一入口） | △ 仅进程级 terminate/kill | 改造 |
| H-05 | SHOULD | 进度事件源接入后端 | ✓ 已有后端无关机制（文件计数） | 微调 |
| H-06 | MUST | 统一错误码域（不依赖 exitCode） | △ 码名绑死进程语义 | 改造 |
| H-07 | MUST | 进程内调用的线程模型 | △ 有工作线程先例，未覆盖执行阶段 | 改造 |
| H-08 | MUST | 启用前自检与失败自动回落 | ✗ 无 | 新增 |
| H-09 | MUST | 输出校验等价性硬化 | △ 仅文档约定，代码未强制 | 改造 |
| H-10 | MUST | 同名模块冲突自检 | ✗ 无 | 新增 |
| H-11 | MUST | `rip_module.json` 字段扩展与打包同步 | △ 清单已有但字段不足 | 改造 |
| H-12 | MUST | 双后端等价性测试与假 DLL fixture | △ 已有单测与假模块手法可复用 | 扩展 |
| H-13 | SHOULD | 进程内超时降级的显式说明与 UI 提示 | ✗ 无 | 新增 |
| H-14 | SHOULD | 两模式统一的运行留痕 | △ 仅 stdout/stderr 内存缓冲 | 改造 |

图例：`✓` 已具备 ｜ `△` 部分具备需改造 ｜ `✗` 缺失

> **v2 修订**：H-05 由 MUST 降为 SHOULD。实测发现进度并非解析 stdout，而是
> `apps/slicer_ui_host_sim/HostRipProgress.cpp` 中 500ms 定时器统计 staging 目录下的
> `*.tif` / `*.tiff` 数量——**该机制与后端无关，进程内模式同样把文件写进同一 staging 目录，天然可用**。
> 因此进度不构成双模阻塞项，对应的 RIP 侧要求 R-13 也一并降级。

---

## 2. 现在已具备的能力（A 级，可直接复用）

这一节的价值在于：**双模改造不是重写，下列能力对两种后端完全通用，必须原样保留。**

| # | 能力 | 证据 |
|---|---|---|
| HC-1 | 模块发现与清单校验：读 `rip_module.json`，取 `entrypoint`/`library`/`resourceDirectory`，校验三者均真实存在且不逃出模块目录 | `apps/slicer_ui_host_sim/HostRipJobController.cpp:236`、`:295` |
| HC-2 | 默认模块目录解析为 `modules/rip` | `apps/slicer_ui_host_sim/HostRipJobController.cpp:191` |
| HC-3 | 设置持久化、默认值与校验（13 个字段，独立于切片工作区 schema） | `apps/slicer_ui_host_sim/HostRipSettingsStore.h` |
| HC-4 | 无 shell 的参数构建：全绝对路径、逐参数传递、模块目录与 Package 目录双重围栏、staging 命名强校验 | `src/rip_integration/RipCommandBuilder.cpp` |
| HC-5 | 执行前对真实 S1 TIFF 逐张结构校验，且可取消 | `src/rip_integration/RipInputValidator.h` |
| HC-6 | 源文件身份捕获与复核（SHA-256），执行后确认输入未被篡改 | `apps/slicer_ui_host_sim/HostRipSafety.h` |
| HC-7 | staging 所有权规则：只删真实存在的、自己创建的 `.rip.staging.*` 直接子目录 | `HostRipSafety::RemoveOwnedStaging` |
| HC-8 | 输出逐层校验与命名归一：结构 + W/S/V 上限，`StrictS2` / `DiagnosticUnvalidated` 双档，**全部层校验完成后才改名** | `src/rip_integration/RipOutputValidator.h` |
| HC-9 | 同父原子发布，已存在输出 fail-closed 永不覆盖；Package 与手动两条发布路径 | `src/rip_integration/RipArtifactPublisher.h` |
| HC-10 | 两种作业范围：`PackageBound`（冻结契约）与 `ManualUnbound`（操作员指定目录） | `RipCommandScope` |
| HC-11 | 五态作业状态机与三个对外信号（进度 / 状态 / 完成） | `apps/slicer_ui_host_sim/HostRipJobController.h` |
| HC-12 | **后端无关的进度机制**：500ms 定时器统计 staging 目录 TIFF 文件数 + 阶段名 | `apps/slicer_ui_host_sim/HostRipProgress.cpp` |
| HC-13 | 单元测试 `rip_integration_unit_tests` 已接入 ctest；接线校验 `tests/ripflow/ValidateRipflowWiring.py` | `CMakeLists.txt:3330` |
| HC-14 | 模块打包与来源留痕：逐文件 SHA-256 清单 + `source_provenance.json` + 打包自检脚本 | `scripts/PackageRipModule.ps1`、`scripts/TestRipModulePackage.ps1` |
| HC-15 | 假模块 fixture 手法（构造一个可控的伪 `rip_cli.exe` 跑生命周期门禁） | `scripts/RunRipflowLifecycleGate.ps1` |
| HC-16 | **`src/rip_integration` 是 Qt-free 静态库**，只链 `TIFF::TIFF`，并由一个非 Qt 测试目标覆盖 | `CMakeLists.txt:3290` 起 |

> HC-16 是本次改造最重要的既有条件：它让**风险最高的新代码（DLL 后端）可以落在无 Qt、
> 有单测、开 `/W4 /WX` 的库里**，用假 DLL 就能完整覆盖，不必依赖真实 SDK 或 Qt 环境。

---

## 3. 需整改的能力

### H-01（MUST）执行后端抽象 `IRipBackend`

**现状**：`HostRipJobController::StartExternalProcess()` 直接 `m_process = new QProcess(this)`
（`apps/slicer_ui_host_sim/HostRipJobController.cpp:867`），进程语义与作业编排耦合在同一个类里。

**要求**：抽出接口，把"怎么调用 RIP"与"作业怎么编排"分开。接口草案见 §4.2。

**验收**：抽象落地后，`rip_integration_unit_tests` 与 RIPFLOW 相关门禁全绿且行为无差异；
`HostRipJobController` 中不再出现 `QProcess` 类型。

---

### H-02（MUST）`executionMode` 设置项与清单声明

**现状**：`hostripsettings` 无执行方式字段；`rip_module.json` 也不声明模块支持哪些执行方式。

**要求**：

1. `hostripsettings` 增 `QString executionmode{"process"}`，取值 `process` / `inprocess`，
   `HostRipSettingsStore::Validate` 覆盖；**默认必须是 `process`**。
2. `rip_module.json` 增 `executionModes`（见 H-11）。
3. 设置为 `inprocess` 但清单未声明支持 → **fail-closed 拒绝启动**，不得静默回落
   （与 H-08 的"自检失败回落"区分：声明不支持是配置错误，自检失败才是运行时降级）。
4. UI 上标注两种模式的风险差异（见 H-13）。

**验收**：三种组合（清单支持+设 inprocess、清单不支持+设 inprocess、设 process）各有一条测试。

---

### H-03（MUST）`RipLibraryBackend`（进程内 DLL 客户端）

**现状**：不存在。全仓无任何位置加载 `RipSlicer.dll`（已用 PowerShell 复算加载点）。

**要求**：

| 项 | 要求 |
|---|---|
| 归属 | 放在 **Qt-free 的 `src/rip_integration/`**（见 HC-16），以便用假 DLL 在非 Qt 测试目标里覆盖 |
| 加载方式 | `LoadLibraryExW(绝对路径, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)`，路径来自清单且经过 HC-4 同款围栏 |
| 符号解析 | 一次性解析全部必需导出；**任一必需符号缺失即失败**，不得惰性解析后半途崩溃 |
| 版本校验 | 先查 `rip_abi_version()`（R-11），与宿主预期不符 → fail-closed 卸载 |
| 句柄管理 | RAII 包装 `rip_create`/`rip_destroy`；**销毁全部句柄后**才 `FreeLibrary`（与 `rip_cli.c:622` 的收尾顺序一致） |
| 内存所有权 | 库返回的任何指针只能由库释放（`rip_delete_bmp`），宿主永不 `free`/`delete` |
| 字符串 | 一律 UTF-8 传入，转换集中一处 |
| 日志 | 注册 `rip_set_log_callback`，回调内**只做拷贝入队**，不得在回调线程里碰 Qt 对象 |
| 卸载 | 作业结束即卸载，不长驻；下次作业重新加载（便于换版、便于释放约 12MB LUT） |

**验收**：用 R-09 的一致性用例跑通同一批切片，输出与 `RipProcessBackend` 逐字节一致。

---

### H-04（MUST）统一取消语义

**现状**：取消依赖进程级手段——`m_process->terminate()` 后 2 秒 `kill()`
（`apps/slicer_ui_host_sim/HostRipJobController.cpp:919`）。
进程内模式**没有等价手段**：一旦 `rip_run` 阻塞，宿主既不能中断也不能回收。

**要求**：

1. `IRipBackend::RequestCancel()` 作为唯一取消入口；
2. `RipProcessBackend` 内部维持 terminate→kill 不变；
3. `RipLibraryBackend` 调用 `rip_cancel(handle)`（R-12），并在约定时间内等待 `rip_run` 返回；
4. 超过等待上限仍未返回 → 进入**不可恢复态**：明确告知操作员"RIP 已失去响应，只能重启软件"，
   **禁止用 `TerminateThread` 之类手段伪装成功**；
5. 现有"取消后不得残留 staging"的规则对两模式同样生效。

---

### H-05（SHOULD）进度事件源接入后端

**现状（v2 修订）**：进度由 `HostRipProgress.cpp` 的 500ms 定时器统计 staging 目录内
`*.tif` / `*.tiff` 数量得出，**与后端完全无关**；`OnReadyStandardOutput` 里那次
`UpdateProgress()` 只是在 stdout 到达时额外刷新一次，并不是进度来源。

**要求**（工作量很小）：

1. 把"stdout 到达时刷新"改为"后端事件到达时刷新"，使两种后端都能触发即时刷新；
2. 文件计数保持为进度的**通用基线**，不得为 inprocess 另起一套；
3. RIP 若提供进度回调（R-13），仅用于细化阶段名（如"加载挂网表"），不取代文件计数。

---

### H-06（MUST）统一错误码域

**现状**：失败码直接绑死进程语义——`RIP_PROCESS_CRASHED`、`RIP_PROCESS_EXIT_FAILED`、
`RIP_PROCESS_START_FAILED`。进程内模式下这三个码都无意义。

**要求**：

| 统一码 | process 后端来源 | inprocess 后端来源 |
|---|---|---|
| `RIP_BACKEND_START_FAILED` | `QProcess::FailedToStart` | `LoadLibraryEx` / 符号解析 / ABI 不符 |
| `RIP_BACKEND_ABORTED` | 非正常退出 | —（进程内崩溃即宿主崩溃，见 H-08） |
| `RIP_BACKEND_RUN_FAILED` | exitCode ≠ 0 | `rip_run` 返回非 `RIP_OK` |
| `RIP_BACKEND_CANCELLED` | 取消 / 超时 | `rip_cancel` 后返回 |

原进程专属码保留为 process 后端的 `detail`，不再作为对外主码。
**并要求 RIP 侧提供退出码↔错误码映射表（R-14）**，使两模式的 `detail` 粒度一致。

---

### H-07（MUST）进程内调用的线程模型

**现状**：QProcess 是异步信号驱动，天然不阻塞 UI；校验阶段已有工作线程先例
（`m_validationThread`，`apps/slicer_ui_host_sim/HostRipJobController.cpp:764`）。

**要求**：`rip_run` 是同步阻塞调用，**必须**在工作线程执行，禁止在 GUI 线程调用；
Qt-free 层用 `std::thread`，回调经 `RipBackendCallbacks` 投出；
Qt 层用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 回到 GUI 线程；
取消标志沿用现有 `std::shared_ptr<std::atomic_bool>` 令牌模式，不得用 Qt 信号做同步取消。

---

### H-08（MUST）启用前自检与失败自动回落

**现状**：无。

**要求**：首次启用 `inprocess`（以及每次模块版本变化后）先跑**一次性探针**：

```text
1. 同名模块冲突自检（H-10）                             冲突 → 拒绝并回落 process
2. LoadLibraryEx(RipSlicer.dll)                        失败 → 回落并记录原因
3. rip_abi_version() 与宿主预期比对                     不符 → 回落
4. rip_create / rip_set_resource_dir / rip_load_tables  失败 → 回落
5. rip_destroy / FreeLibrary                           未能干净卸载 → 回落并标记该模块不可进程内使用
```

回落必须**可见**（状态栏 + 运行留痕），不得静默。

> 风险声明：进程内模式下 RIP 的任何越界写或未捕获异常会直接带走整个 UI 进程。
> 自检只能拦住"加载期"问题，拦不住"运行期"崩溃——这是选择进程内模式必须接受的代价，
> 也是默认值保持 `process` 的理由。

---

### H-09（MUST）输出校验等价性硬化

**现状**：`ValidateAndNormalizeRipOutput` 的逐层校验规则本身完备，但"不得用执行成功替代逐层校验"
目前只写在决策文档里，代码层面没有强制。进程内模式下少了 `exitCode` 这道外部信号，更容易被绕过。

**要求**：

1. `FinalizeValidatedOutput` 之前必须持有一份**逐层校验结果**
   （`RipOutputValidationResult.layers` 非空且层数匹配），否则直接失败；
2. 两种后端走**同一个**校验与发布代码路径，不得为 inprocess 另开分支；
3. 增加一条测试：伪造"后端返回成功但产物层数不足"，断言发布被拒——
   **先断言该分支确实被触发，再相信测试全绿**。

---

### H-10（MUST）同名模块冲突自检

**现状**：`rip_module/runtime_dependencies.json` 的 `hostRootDeploymentForbidden: ["tiff.dll"]`
只能阻止私有依赖被复制到宿主根目录，**在进程内模式下完全失效**——同一进程必然共享模块名空间。

**要求**：`RipLibraryBackend` 加载前枚举本进程已加载模块，若存在与 RIP 私有依赖同名者
（当前即 `tiff.dll`）→ **拒绝进程内加载并回落 process**，错误信息点名冲突模块与两侧版本。

即使 DECISION-01 选定的方案让该冲突消失，这道自检也要保留：
它是防止未来任何第三方插件引入同名模块的最后一道闸。

---

### H-11（MUST）`rip_module.json` 字段扩展与打包同步

**现状**：清单已有 `schema` / `version` / `entrypoint` / `library` / `resourceDirectory` / 逐文件 SHA-256
（`scripts/PackageRipModule.ps1:161` 起），但不声明执行方式与 ABI。

**要求**：清单增以下字段，由 `PackageRipModule.ps1` 生成、`TestRipModulePackage.ps1` 校验：

```json
{
  "executionModes": ["process", "inprocess"],
  "abiVersion": 2,
  "exitCodeMap": { "0": "RIP_OK", "13": "RIP_ERR_RESOURCE", "…": "…" }
}
```

`schema` 随之升版；旧清单（无这些字段）按 `["process"]` 处理，保持向后兼容。

> 若 DECISION-01 选定方案 (c)（统一动态 libtiff），清单还需记录所用 `tiff.dll` 的 SHA-256，
> 并与宿主运行目录下的那一份比对一致——否则两种模式会用上不同的 libtiff 构建。

---

### H-12（MUST）双后端等价性测试与假 DLL fixture

**现状**：已有 `rip_integration_unit_tests`（Qt-free 逻辑层）与 `RunRipflowLifecycleGate.ps1`
的假 `rip_cli.exe` 手法可复用。

**要求**：

1. 新增**假 `RipSlicer.dll`** fixture（导出全套符号、行为可控），使 `RipLibraryBackend`
   在没有真实 SDK 的机器上也能被测；
2. 新增**双后端等价性测试**：同一组切片 fixture 分别跑两个后端，产物逐字节比对；
3. 覆盖失败面：符号缺失、ABI 不符、加载失败、`rip_run` 返回错误、取消、卸载残留、同名模块冲突；
4. 把 `pe_imports.py`（导入表检查）入库，作为模块包自检的一部分；
5. 新增文件必须 `git check-ignore -v` 确认未被忽略（`tests/` 下多个目录被 gitignore 命中过）；
6. 用 ctest 名注册；构建 target 名与 ctest 名不同，别混用。

---

### H-13（SHOULD）进程内超时降级的显式说明与 UI 提示

超时在 process 模式下有强杀兜底，在 inprocess 模式下没有。设置界面切换到 `inprocess` 时应明确提示：

> 该模式下 RIP 与本软件同进程运行，超时只能请求停止、无法强制终止；RIP 崩溃会导致本软件一并退出。

---

### H-14（SHOULD）两模式统一的运行留痕

现状 stdout/stderr 只进内存缓冲（`m_stdout` / `m_stderr`），作业结束即散。
要求两种后端都把「模块版本 + ABI 版本 + 执行方式 + 参数 + 逐层结果 + 耗时 + 结束原因」落到一份运行记录，
便于事后比对两模式差异——这也是 R-09 一致性出现争议时唯一可用的证据。

---

## 4. 实施建议（成熟落地路径）

### 4.1 分层归属（最关键的一个决定）

`src/rip_integration/` 是 **Qt-free 静态库**，只链 `TIFF::TIFF`，被非 Qt 的
`rip_integration_unit_tests` 覆盖（HC-16）。据此划分：

| 新增件 | 归属 | 理由 |
|---|---|---|
| `RipBackend.h`（纯接口） | `src/rip_integration/` | Qt-free，两侧共用 |
| `RipLibraryBackend.{h,cpp}` | `src/rip_integration/` | 只用 Win32 API，**可用假 DLL 在非 Qt 单测里完整覆盖** |
| `RipProcessBackend.{h,cpp}` | `apps/slicer_ui_host_sim/` | 依赖 QProcess，必须留在 Qt 层 |

**收益**：风险最高的新代码落在有单测、开 `/W4 /WX` 的库里；进程后端只是平移，不承担新风险。

### 4.2 接口草案（Qt-free）

```cpp
namespace slicesoft::rip
{
struct RipRunOutcome
{
    RipStatus status;             // 复用现有 RipStatus，不另造错误体系
    int layers_ok{0};
    int layers_failed{0};
    std::string backend_detail;   // exitCode 原文，或 rip_last_error() 原文
};

struct RipBackendCallbacks
{
    std::function<void(int done, int total, std::string_view phase)> on_progress;
    std::function<void(int level, std::string_view text)> on_log;
    std::function<void(RipRunOutcome)> on_finished;
};

class IRipBackend
{
public:
    virtual ~IRipBackend() = default;
    /** 复用现有 RipCommandRequest，参数只有一处真相。 */
    [[nodiscard]] virtual RipStatus Start(
        const RipCommandRequest& request, RipBackendCallbacks callbacks) = 0;
    virtual void RequestCancel() = 0;
    [[nodiscard]] virtual bool IsRunning() const = 0;
};
}  // namespace slicesoft::rip
```

**要点**：`RipCommandRequest` 已经是两种后端都需要的完整输入（runtime 路径 + package + staging +
settings + scope），**不要为 DLL 后端另造请求结构**——`RipLibraryBackend` 直接消费同一个 request，
只是把它翻译成 setter 调用而不是 argv。这样参数语义天然只有一处真相，R-09 的一致性才有基础。

### 4.3 六步迁移（每步独立可验收，前三步不依赖 RIP 侧）

| 步 | 动作 | 验收 |
|---|---|---|
| 1 | 加 `RipBackend.h`（只有头、无实现、不接线） | 编译通过，行为零变化 |
| 2 | 新建 `RipProcessBackend`，把进程部分**整体平移**；控制器改持 `std::unique_ptr<IRipBackend>` | 现有 RIPFLOW 门禁与 `rip_integration_unit_tests` 全绿；控制器里不再出现 `QProcess` |
| 3 | 加 `executionmode` 设置 + 清单 `executionModes` + 校验（此时只有 `process` 生效） | H-02 三组合测试 |
| 4 | 实现 `RipLibraryBackend` + 假 `RipSlicer.dll` fixture | 新单测覆盖成功/符号缺失/ABI 不符/取消/卸载 |
| 5 | 自检探针 + 回落 + 同名模块冲突自检 | 模拟冲突被拒并回落 |
| 6 | 双后端等价性测试 | 同 fixture 产物逐字节一致 |

**第 2 步的搬迁清单**（照搬，不改逻辑）：

- 搬走：`StartExternalProcess()` 第 866 行起的 QProcess 构造、PATH 前置、四个 `connect`、
  `start()`；`OnReadyStandardOutput` / `OnReadyStandardError` / `OnProcessFinished` /
  `OnProcessError`；`Cancel()` 中的 `terminate()` + `m_killTimer`；`m_stdout` / `m_stderr` 缓冲。
- 留下：阶段机（`Phase`）、输入校验、源身份、staging 生命周期、输出校验、发布、
  超时定时器、进度定时器。

> 第 2 步是纯重构，**必须在没有任何新功能的前提下单独验收**。
> 这样第 4 步引入 DLL 后端时，任何回归都能立刻定位到新代码而不是搬迁动作。

### 4.4 与 RIP 侧的符号约定

`RipLibraryBackend` 需要的导出符号（对应 REQ-02）：

```text
必需：rip_abi_version、rip_create、rip_destroy、rip_last_error、rip_error_string、
      rip_set_resource_dir、rip_set_input_icc_path、rip_set_output_icc_path、
      rip_set_intents、rip_set_transparent、rip_set_color_texture、rip_set_rip_mode、
      rip_set_input_path、rip_set_output_path、rip_load_tables、
      rip_run、rip_run_continue_on_error、rip_cancel、rip_set_log_callback
可选：rip_set_progress_callback（缺失时退回文件计数进度，不算失败）
```

**"可选"这一项很重要**：解析符号时单独标记，缺失不应导致加载失败——
因为进度已有后端无关的兜底机制（HC-12）。

### 4.5 工作量估计

| 步 | 规模 | 风险 |
|---|---|---|
| 1–2 纯重构 | 约 300 行搬迁 + 80 行新接口 | 低（有现成门禁兜底） |
| 3 设置与清单 | 约 120 行 + 脚本 | 低 |
| 4 DLL 后端 + 假 DLL | 约 400 行 + fixture | **中高**（新代码，但在 Qt-free 有测层） |
| 5 自检与回落 | 约 150 行 | 中 |
| 6 等价性测试 | 约 200 行 | 中（依赖真实 SDK） |

---

## 5. 验收清单

- [ ] `HostRipJobController` 中不再出现 `QProcess` 类型，两后端均通过 `IRipBackend`
- [ ] 第 2 步纯重构单独验收通过，行为零变化
- [ ] `executionMode` 三种组合的行为测试通过，默认值仍为 `process`
- [ ] `RipLibraryBackend` 落在 Qt-free 层，并由假 DLL 单测覆盖全部失败面
- [ ] 双后端等价性测试通过：同一 fixture 产物逐字节一致
- [ ] 取消测试：两模式均无 staging 残留，inprocess 的不可恢复态有明确提示
- [ ] 输出校验等价性测试：伪造"成功但层数不足"被拒绝发布
- [ ] 同名模块冲突自检测试：模拟已加载 `tiff.dll` 时 inprocess 被拒绝并回落
- [ ] 清单新字段由打包脚本生成、由自检脚本校验，旧清单向后兼容
- [ ] `pe_imports.py` 入库并接入模块包自检
- [ ] 新增测试文件均已 `git check-ignore -v` 确认入库

---

## 6. 阻断与未决

| 项 | 状态 |
|---|---|
| §4.3 第 1–3 步 | **不阻断**，可立即开工 |
| §4.3 第 4–6 步 | **阻断**于 RIP 侧 R-01 / R-03 / R-11 / R-12（见 README §5） |
| libtiff 方案选型 | **待决**，见 [DECISION-01](DECISION-01-libtiff隔离方案.md) §6；若选方案 (c)，宿主侧需额外提供与模块目录一致的 `tiff.dll` 并纳入清单 SHA-256 |
| 静态 libtiff 产物（若选方案 d） | 宿主侧待排期：可用 `x64-windows-static-md` preset 轨道产出 |
| 进程内模式的崩溃域风险 | 无技术解，只能靠默认值 + UI 提示 + 自检缓解，需产品决策确认可接受 |
| 对外分发 | 与本专项无关但同样阻断：`redistributionStatus` 仍为 `BLOCKED_EXTERNAL_LICENSE_EVIDENCE` |
