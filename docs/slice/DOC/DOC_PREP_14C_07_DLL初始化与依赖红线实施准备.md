# DOC_PREP_14C_07 DLL 初始化与依赖红线实施准备

## 1. 文档状态

| 项目 | 内容 |
|---|---|
| 对应任务 | `14C-07` DLL 初始化与依赖红线 |
| 编制日期 | 2026-08-06 |
| 依据任务清单 | `TASKS_14` v2.22 |
| 依据执行指令 | `CODEX_PROMPT_14` v1.3 |
| `PREPARATION_GATE` | **PASS** |
| `EXECUTION_GATE` | **AUTHORIZED** |

`PREPARATION_GATE=PASS` 仅表示需求、边界、文件所有权和验收方式已经可以指导实现，不表示 `14C-07` 已完成。`14C-05` 已于 2026-08-06 提交并释放 `Exports.cpp`、模块 CMake 和模块元数据文件所有权，因此本任务执行门已打开。

## 2. 目标与当前事实

### 2.1 任务目标

1. 提供最小、无副作用的 Windows DLL 入口。
2. 将进程级初始化延迟到 `pm_create`，并使用 `std::call_once` 保证进程内仅执行一次。
3. 通过静态检查、ABI 测试和 PE 依赖检查，阻止 Qt、PrintSDK、切片引擎及其他非许可依赖进入 `slicer_module`。
4. 保持 SPI v1、11 个导出和 15 项能力不变。

### 2.2 已核实的代码事实

- 当前 `src/slicer_module` 没有 `DllMain`，也没有 `std::call_once` 初始化入口，因此本任务尚未实现。
- 当前 `slicer_module` 目标只链接 `slicer_base`，未发现 Qt、PrintSDK 或 `slicer_engine` 的源码引用。
- 当前 `pm_create` 直接创建并注册模块实例，后续需在不改变每实例生命周期的前提下插入进程级一次性初始化。
- `pm_module_info` 正由相邻任务 `14C-05` 收口；本任务不得重复实现模块自述逻辑。

## 3. 依赖

### 3.1 前置任务与合同

| 依赖 | 状态 | 本任务使用方式 |
|---|---|---|
| `14A-01` SPI 头文件冻结 | 已完成 | 固定 `PM_SPI_VERSION=1`、调用约定和导出函数签名 |
| `14B` Base/Engine 分层 | 已完成 | 模块只能依赖 `slicer_base`，不得回链 Engine |
| `14C-01` DLL 骨架 | 已完成 | 沿用现有模块目标和导出入口 |
| `14C-05` 模块自述与部署清单 | 已完成 | 共享文件所有权已释放；沿用其模块自述实现 |
| `print_module_spi.h` | 冻结 | ABI、句柄、缓冲区和错误边界真源 |
| `slicer_three_lane_contract` v1.1 | 冻结 | DLL 不执行生产切片，不引入 Worker 侧职责 |
| `file_contract_v1` | 冻结 | 本任务不修改文件合同，仅保持模块与 Worker 的边界 |
| 取消合同 | 冻结 | 本任务不创建作业、不启动线程，也不改变取消状态机 |

### 3.2 实现前置

- 使用支持 C++20 `std::once_flag` / `std::call_once` 的 MSVC 工具链。
- Debug 与 Release 必须分别保持 `/MDd` 与 `/MD`，不得静态链接 CRT。
- 执行 PE 依赖验收时必须能调用 Visual Studio 的 `dumpbin.exe`。

## 4. 冻结边界

### 4.1 `DllMain` 红线

`DllMain` 必须等价于以下行为：接收参数后直接返回 `TRUE`。其中禁止：

- 创建、等待或销毁线程；
- 获取业务锁、条件变量或执行 `std::call_once`；
- 文件、注册表、环境变量、路径、日志或控制台 I/O；
- `LoadLibrary`、插件发现、Qt 初始化、PrintSDK 初始化；
- 创建模块实例、注册能力、连接 Worker 或加载切片引擎；
- 在 `DLL_PROCESS_DETACH` 中等待后台任务或做业务清理；
- 抛出异常，或调用任何可能抛出异常的业务函数。

即使 `DisableThreadLibraryCalls` 通常可作为优化，本阶段也不加入，因为冻结结论要求入口只返回 `TRUE`。

### 4.2 一次性初始化边界

- 进程级初始化只能从 `pm_create` 路径触发，并由进程内唯一的 `std::once_flag` 保护。
- `pm_spi_version`、`pm_module_info`、`pm_last_error` 在首次 `pm_create` 前仍须可调用，且不得触发初始化。
- 一次性初始化只允许建立无业务状态的模块基础设施，不得创建模块句柄、作业、线程池、Worker、输出目录或持久化文件。
- 模块实例注册、引用计数、销毁和错误上下文仍按每次 `pm_create` / `pm_destroy` 独立执行，不能被放入 `call_once`。
- 初始化失败不得穿透 ABI；`pm_create` 返回空句柄，并通过稳定错误码和线程局部错误信息报告。
- 多线程并发调用 `pm_create` 时，初始化函数只执行一次，各调用仍获得独立模块实例。

### 4.3 ABI 与依赖边界

- `PM_SPI_VERSION=1` 不变。
- 导出集合严格保持 11 个 `pm_*` C ABI 符号，不增加、删除、改名或导出 C++ 修饰符号。
- 15 项能力集合不变，不借本任务新增能力。
- ABI 参数不得出现 Qt 类型、STL 容器、异常或跨模块所有权对象。
- `slicer_module` 只允许链接 `slicer_base` 和必要的 Windows/CRT 系统库。
- 禁止直接或传递依赖 Qt5/Qt6、PrintSDK、`slicer_engine`、OpenVDB、libtiff 或 UI 目标。
- 禁止通过 `/DELAYLOAD` 隐藏违规依赖。

## 5. 文件所有权

### 5.1 `14C-07` 可拥有的文件

| 文件 | 操作 | 所有权说明 |
|---|---|---|
| `src/slicer_module/DllMain.cpp` | 新增 | 只承载无副作用 DLL 入口 |
| `src/slicer_module/ModuleInitialization.h` | 新增 | 声明进程级初始化内部接口 |
| `src/slicer_module/ModuleInitialization.cpp` | 新增 | 持有 `once_flag` 和初始化实现 |
| `src/slicer_module/Exports.cpp` | 窄幅修改 | 仅在 `pm_create` 接入一次性初始化和失败映射 |
| `tests/stage14c_07/*` | 新增 | DllMain、并发初始化和依赖红线测试 |
| 模块/测试相关 `CMakeLists.txt` | 窄幅修改 | 注册新增源文件与测试，不改变链接层次 |
| `TASKS_14`、`REPORT_14` | 验收后更新 | 仅记录真实执行结果 |

### 5.2 明确不属于本任务

- `ModuleInfo.*`、`module.json.in`、`pm_module_info` 业务内容属于 `14C-05`。
- `apps/slicer_worker/*`、Worker 文件合同、取消和 staging 属于 `14D`。
- `src/slicer_core/engine/*`、生产切片、TIFF、RGBWSV 和报告逻辑不属于本任务。
- Qt UI、PrintSDK 适配器、宿主加载器不属于本任务。
- 任何合同 JSON/Markdown、SPI 版本和能力集合修订都不属于本任务。

共享文件修改只能基于已提交的 `14C-05` 基线进行；若执行前出现未归属的模块改动，执行门禁重新关闭。

## 6. 验收命令

以下命令为实现完成后的最低验收集合；当前准备阶段不将其记为已运行或已通过。

```powershell
cmake --build build-slicesoft/main --config Debug --target slicer_module
cmake --build build-slicesoft/main --config Release --target slicer_module
cmake --build build-slicesoft/main --config Debug --target stage14c07_dll_initialization_tests stage14c03_module_abi_tests
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "stage14c07|stage14c03_module_abi"
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
git diff --check
```

在 Visual Studio Developer PowerShell 中执行 PE 验收：

```powershell
dumpbin /EXPORTS build-slicesoft/main/Debug/slicer_module.dll
dumpbin /DEPENDENTS build-slicesoft/main/Debug/slicer_module.dll
dumpbin /EXPORTS build-slicesoft/main/Release/slicer_module.dll
dumpbin /DEPENDENTS build-slicesoft/main/Release/slicer_module.dll
```

验收证据必须证明：

1. Debug/Release 均构建成功。
2. 导出表恰好包含冻结的 11 个 `pm_*` 符号，没有 C++ 修饰导出。
3. 依赖表不含 Qt、PrintSDK、`slicer_engine`、OpenVDB 和 libtiff。
4. 多线程并发 `pm_create` 时进程初始化计数严格为 1，模块实例数与成功调用数一致。
5. `pm_create` 前调用只读 ABI 不触发初始化。
6. 初始化失败不会抛出 ABI，且错误信息可通过 `pm_last_error` 读取。

## 7. 必测负例

| 负例 | 期望结果 |
|---|---|
| 在 `DllMain` 中记录日志、读取环境变量或创建线程 | 静态红线测试失败 |
| 在 `DllMain`、全局构造或 `pm_module_info` 中调用 `call_once` | 测试失败 |
| 每次 `pm_create` 重复执行进程初始化 | 并发/计数测试失败 |
| 一次性初始化创建 Worker、引擎、作业或输出文件 | 边界测试和代码审查失败 |
| 初始化函数抛出异常穿透 `pm_create` | ABI 负例失败 |
| 模块新增 Qt、PrintSDK、Engine 或 `/DELAYLOAD` 依赖 | `/DEPENDENTS` 或 CMake 红线测试失败 |
| 增删导出函数或改变调用约定 | `/EXPORTS` 和 ABI 测试失败 |
| 借初始化注册第 16 项能力 | 能力合同测试失败 |

## 8. 与相邻任务的边界

- **与 `14C-05`**：`14C-05` 负责模块自述和部署清单；`14C-07` 只负责无副作用入口、一次性初始化和依赖红线。二者共享 `Exports.cpp` 与 CMake，必须串行。
- **与 `14C-06`**：`14C-06` 负责完整 SPI 一致性与宿主门禁汇总；`14C-07` 只提供 C-SPI-16/17 相关证据，不能单独将 `14C-06` 标记完成。
- **与 `14D`**：Worker 启动、请求执行、取消、staging 和进程回收全部留在 Worker 侧；本任务不得把这些工作搬进 DLL 初始化。
- **与 `14E`**：UI 和打印宿主不感知本任务内部初始化实现，只依赖冻结 SPI 行为。

## 9. 门禁结论

`14C-07` 的技术合同、实现范围、文件所有权、验收命令和负例已补齐，因此：

```text
PREPARATION_GATE=PASS
EXECUTION_GATE=AUTHORIZED
```

不存在合同层面的真实阻塞。`14C-05` 已完成并释放共享文件所有权，可按本文件直接进入开发。
