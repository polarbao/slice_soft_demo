# DOC_PREP_14D_08 Worker 独立调试入口实施准备

## 1. 文档状态

| 项目 | 内容 |
|---|---|
| 对应任务 | `14D-08` Worker 独立调试入口 |
| 编制日期 | 2026-08-06 |
| 依据任务清单 | `TASKS_14` v2.22 |
| 依据执行指令 | `CODEX_PROMPT_14` v1.3 |
| `PREPARATION_GATE` | **BLOCKED** |
| 阻塞类型 | **FILE_CONTRACT_TO_FACADE_MAPPING_AND_ENGINE_ADAPTER_GAP** |

本门禁不得标记为 `PASS`。现有 Worker 已提供命令行外壳，但实际执行路径仍返回 `NotImplemented`；同时，冻结文件合同与当前 Facade 输入之间缺少明确映射，三项已声明重能力中只有生产切片 Facade 存在可核实的具体工厂。直接开发会迫使实现者猜测合同语义或只支持部分能力，两者都违反 fail-closed 和“合同先于实现”的要求。

## 2. 目标与当前事实

### 2.1 任务目标

提供开发者可直接启动和附加调试器的私有 Worker 入口：

```text
slicer_worker.exe --spi-request <absolute-request-path>
```

该入口必须与 DLL 启动 Worker 时使用同一请求解析、同一生产执行器、同一结果写入和退出码逻辑，不能形成第二套切片实现。

### 2.2 已核实的代码事实

- `slicer_worker --contract-info` 已存在。
- `--spi-request` 已校验参数个数和绝对路径，但随后明确返回 `NotImplemented`，因此 `14D-08` 尚未实现。
- Worker 当前链接 `slicer_engine`，并已有 `WorkerContract`、协议解析和模块侧 `WorkerClient` 基础。
- 文件合同声明三项重能力：`slice.rgbwsv`、`geometry.preflight.full`、`geometry.repair`。
- 当前可核实的具体工厂只有生产 `SliceFacade`；未发现 full preflight 和 repair Facade 的具体工厂/适配器。
- 当前 `SliceFacade` 请求依赖 `scene_config_path`，而冻结文件合同的切片请求携带内嵌 `scene`、`profile` 与 `output`；二者之间没有冻结的物化和生命周期规则。
- 文件合同对 preflight/repair 使用通用 `input` 对象，而能力 DTO 使用 `modelId`、`modelPath`、`mode`、`outputPath`、`options` 等字段；嵌套映射没有权威定义。

## 3. 依赖

### 3.1 已满足依赖

| 依赖 | 状态 | 本任务使用方式 |
|---|---|---|
| `14D-01` Worker 骨架与 `--contract-info` | 已完成 | 提供可执行程序和合同发现入口 |
| `14D-02` 模块侧 WorkerClient | 已完成 | 后续必须与独立入口共享同一 Worker 行为 |
| `14D-03` 文件合同协商 | 已完成 | 固定合同版本、能力列表和协商语义 |
| SPI v1 / 11 导出 / 15 能力 | 冻结 | 独立入口不得改变公共 ABI 或能力集合 |
| 三车道合同 v1.1 | 冻结 | 生产重操作只在 Worker，宿主不得绕过 DLL |
| file-contract v1 | 冻结 | 请求、结果、stdout、退出码和 staging 语义真源 |
| 取消合同 | 冻结 | 独立入口不得另造取消状态机 |

### 3.2 未满足依赖与解除条件

#### 阻塞 B1：文件合同到 Facade 的映射未冻结

必须先形成经授权的合同补充或实施决策，至少明确：

1. 内嵌 `scene` / `profile` 如何传给当前需要 `scene_config_path` 的 `SliceFacade`。
2. 若需临时物化配置，文件位置、UTF-8 编码、命名、清理、失败和取消语义是什么。
3. `geometry.preflight.full` 与 `geometry.repair` 的 `input` 如何逐字段映射到能力 DTO。
4. 请求中的路径以请求文件目录、作业目录还是绝对路径解析；禁止实现者自行猜测。

#### 阻塞 B2：两项能力缺少可调用的具体引擎适配器

必须满足以下二选一条件之一：

- 实现并冻结 full preflight、repair 的具体 Facade 工厂，使 Worker 能按 `--contract-info` 声明执行三项能力；或
- 通过单独的授权合同修订，拆分 `14D-08` 的能力范围并同步 `--contract-info`、任务卡、DEV、DEMO 和 schema 验收。

在上述条件完成前，禁止静默返回“不支持”、只实现 `slice.rgbwsv` 却继续声明三项能力，或以假结果通过测试。

## 4. 冻结边界

### 4.1 入口与调用边界

- 独立入口仅供开发、回归和附加调试器使用，不是打印宿主公共 API。
- 正式打印宿主仍必须通过 DLL SPI 调用；不得从宿主直接执行 Worker 绕开模块。
- 命令行必须严格为 `--spi-request <absolute-request-path>`，相对路径必须拒绝。
- 独立入口和 DLL 启动路径必须调用同一个 `WorkerJobExecutor` 或等价生产执行器。
- 禁止保留“调试专用简化切片器”、内存内假结果或不同材料策略。

### 4.2 请求、输出与进程边界

- 请求文件为 UTF-8 无 BOM，并通过冻结 JSON Schema 和语义校验。
- `jobId`、`correlationId`、能力名和结果身份必须闭合；身份不一致 fail-closed。
- stdout 只允许冻结的 `SLICE_PROGRESS`、`SLICE_TIMING` 和合同要求输出；普通日志进入 stderr 或日志文件。
- 进度必须单调、字段完整，终态不得与 result/exit code 冲突。
- 处理过的业务失败应生成稳定 `result.json`；写入采用临时文件加原子替换。
- `result.json` 的 `engineVersion` 必填且来源稳定，禁止调试入口写占位值。
- 退出码只表达冻结类别，稳定业务错误码以 result 为准。
- 取消、超时、Job Object、staging 清理和旧包保护沿用冻结合同，不得在本任务创建第二套规则。

### 4.3 产品与协议边界

- 不修改 SPI 版本、11 个导出和 15 项能力。
- 不修改 `p0.rgbwsv.2`、RGBWSV 通道顺序、uint8、`black_is_print` 或 TIFF 行为。
- 不引入 Qt、UI、PrintSDK 或宿主交互逻辑。
- 不把生产切片重新搬回 `slicer_module`。
- 不借调试入口放宽模型、路径、合同或材料闭环校验。

## 5. 文件所有权

以下文件所有权仅在解除 B1/B2 后生效。

### 5.1 `14D-08` 可拥有的文件

| 文件 | 操作 | 所有权说明 |
|---|---|---|
| `apps/slicer_worker/WorkerApplication.cpp` | 窄幅修改 | 将 `--spi-request` 从占位分支接入共享执行链 |
| `apps/slicer_worker/WorkerApplication.h` | 必要时修改 | 只声明命令分派所需私有接口 |
| `apps/slicer_worker/WorkerRequestParser.*` 或等价文件 | 新增 | 负责请求读取、schema/语义校验和规范化 |
| `apps/slicer_worker/WorkerJobExecutor.*` 或等价文件 | 新增 | 三项能力统一分派，复用生产 Facade |
| `apps/slicer_worker/WorkerResultWriter.*` 或等价文件 | 新增 | 结果身份、临时写入、原子发布和退出码映射 |
| `tests/stage14d_08/*` | 新增 | 独立入口、三能力、负例和调试入口测试 |
| Worker/测试相关 `CMakeLists.txt` | 窄幅修改 | 注册源文件和测试，不改变模块依赖 |
| `TASKS_14`、`REPORT_14` | 验收后更新 | 只记录真实结果与证据路径 |

### 5.2 需由前置解阻任务拥有的文件

- 文件合同/DTO 映射决策及其 schema 测试属于 B1 解阻任务，不得由 `14D-08` 临时解释。
- full preflight 与 repair 的 Facade 工厂、Engine 适配器属于 B2 解阻任务。

### 5.3 明确不属于本任务

- `src/slicer_module/*`、11 个 SPI 导出和模块自述。
- `14D-04A` 的核心取消令牌、`14D-04B` 的端到端取消收口。
- `14D-05` 的完整 staging、自检、原子发布和旧包恢复策略。
- `14D-06` 的 DLL 后端路由切换。
- UI、打印宿主、模型编辑、TIFF 编码和材料策略。

## 6. 验收命令

以下命令仅在 B1/B2 解除并完成实现后执行；当前不得记录为已通过。

```powershell
cmake --build build-slicesoft/main --config Debug --target slicer_worker stage14d08_worker_debug_entry_tests
cmake --build build-slicesoft/main --config Release --target slicer_worker
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "stage14d08|stage14d01|stage14d03"
python tests/contracts/ValidateJsonSchemas.py --repo-root .
python tests/contracts/ValidateFileContract.py
git diff --check
```

合同发现与直接执行：

```powershell
& .\build-slicesoft\main\Debug\slicer_worker.exe --contract-info
& .\build-slicesoft\main\Debug\slicer_worker.exe --spi-request (Resolve-Path <slice-request.json>)
& .\build-slicesoft\main\Debug\slicer_worker.exe --spi-request (Resolve-Path <preflight-request.json>)
& .\build-slicesoft\main\Debug\slicer_worker.exe --spi-request (Resolve-Path <repair-request.json>)
```

测试夹具必须在测试临时目录生成，不能写入仓库样例输出。每次执行需校验：

1. 请求/结果 JSON 通过冻结 schema。
2. 三项能力的 `jobId`、`correlationId`、capability 和结果身份闭合。
3. 成功返回 0；失败、拒绝、取消、超时返回冻结退出码。
4. `engineVersion`、`elapsedMs` 和 output/error 分支完整。
5. stdout 没有非协议文本，进度单调且终态一致。
6. Release 与 Debug 行为一致，不加载 `slicer_module` 或 Qt。
7. 可用 Visual Studio/VS Code 对该命令直接附加或启动调试，并命中共享执行器断点。该项必须保留人工或自动化证据，不能以“可编译”代替。

## 7. 必测负例

| 负例 | 期望结果 |
|---|---|
| 缺少参数、多余参数或未知参数 | 非零退出，不进入执行器 |
| `--spi-request` 使用相对路径 | 拒绝并返回冻结错误类别 |
| 请求文件不存在、不可读、带 BOM 或 JSON 损坏 | fail-closed，错误稳定 |
| schema 版本不支持或 capability 未声明 | 拒绝，不尝试降级 |
| `jobId` / `correlationId` 缺失或结果身份不一致 | 结果校验失败 |
| 切片内嵌 scene/profile 无法物化 | 明确失败并清理临时文件，不猜测默认值 |
| preflight/repair 无具体 Facade | 门禁失败，禁止返回伪成功 |
| 普通日志写入 stdout | 协议测试失败 |
| 进度倒退、超过 100、缺少终态或终态与退出码冲突 | 协议测试失败 |
| `result.tmp` 残留或失败覆盖既有成功包 | 清理/旧包保护测试失败 |
| 独立入口调用调试专用切片器而非共享执行器 | 路由一致性测试失败 |
| Worker 直接修改 SPI、TIFF 或材料策略 | 边界审查失败 |

## 8. 与相邻任务的边界

- **与 `14D-03`**：`14D-03` 冻结合同协商和能力发现；`14D-08` 只能消费合同，不能自行更改 `--contract-info` 语义。
- **与 `14D-04A`**：核心 `ICancelToken` 由 `14D-04A` 提供；独立入口按其接口传递令牌，不另造类型。
- **与 `14D-04B`**：端到端取消依赖 `14D-05` 和本任务执行入口；`14D-08` 只保证执行器具备取消接线点，不宣称完整取消验收。
- **与 `14D-05`**：staging、自检、原子发布、失败清理和旧包保护由 `14D-05` 收口；本任务的结果写入必须复用该服务，不能复制实现。
- **与 `14D-06`**：`14D-06` 负责 DLL 将重能力路由到 Worker；独立调试入口不改变模块路由。
- **与 `14D-07`**：`14D-07` 汇总 E-01..08 和跨进程回归；本任务只提供可直接执行的 Worker 证据。
- **与 `14C`**：本任务不修改 DLL、导出或模块初始化。

## 9. 建议的解阻拆分

为避免在一张任务卡中混入合同修订和三种引擎适配，建议在开始 `14D-08` 前完成两个独立解阻原子任务：

1. **文件合同到能力 DTO/Facade 的映射冻结**：补齐三项能力的字段映射、临时物化、路径和清理规则，并增加 schema/语义测试。
2. **full preflight 与 repair Worker Facade 接入**：提供可构造的具体适配器，证明三项已声明能力均可真实分派。

若项目决定 `14D-08` 只验证 `slice.rgbwsv`，必须先通过受控合同修订明确缩小范围；不得仅在代码中暗改。

## 10. 门禁结论

当前结论为：

```text
PREPARATION_GATE=BLOCKED
BLOCKER_1=FILE_CONTRACT_TO_FACADE_MAPPING_NOT_FROZEN
BLOCKER_2=PREFLIGHT_FULL_AND_REPAIR_CONCRETE_ADAPTERS_NOT_AVAILABLE
```

命令行外壳、合同解析基础和 Slice Facade 已提供可复用起点，但不足以诚实完成三能力独立调试入口。在 B1/B2 解除前，不得进入 `14D-08` 正式开发或将其标记为 PASS。
