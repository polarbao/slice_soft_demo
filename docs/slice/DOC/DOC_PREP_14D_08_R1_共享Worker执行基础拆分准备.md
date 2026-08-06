# DOC_PREP_14D-08-R1 共享 Worker 执行基础拆分准备

> 编制日期：2026-08-06
>
> 审计基线：`e0993a0`
>
> 父任务：`14D-08` Worker 独立调试入口
>
> 审计范围：共享请求解析、作业身份、结果封装与能力调度基础
>
> 文档状态：`PREPARATION_GATE=PASS_WITH_SPLIT`
>
> 父任务状态：`14D-08=BLOCKED`
>
> R1-01 实施状态：`COMPLETE（2026-08-06）`

## 1. 审计结论

`14D-08` 可以受控拆分。当前仓库已经具备 Worker 可执行外壳、文件合同发现、
模块侧进程管理、冻结的 `file_contract_v1` Schema、稳定退出码表，以及可调用的生产
`SliceFacade` 工厂；因此可以先实现一套由正式 DLL 启动链和独立调试入口共同消费的
**Worker 执行基础**：

```text
绝对 request.json 路径
  -> 严格读取与合同校验
  -> 不可变作业身份
  -> 精确 capability 调度
  -> 注入式执行端口
  -> result.json.tmp 原子替换
  -> 稳定进程退出类别
```

该基础层不要求现在完成三项重能力的真实算法适配，也不允许用测试桩、空包或占位 JSON
伪造 Worker 成功。生产 `slicer_worker` 在某个 capability 尚未安装真实执行器时，必须生成
身份闭合的失败结果并以非零退出；不得静默 fallback 到 DLL 进程内或调试专用实现。

拆分后的门禁结论为：

| 范围 | 结论 | 说明 |
|---|---|---|
| `14D-08-R1` 共享执行基础 | **PASS / READY** | 当前合同和代码事实足以实施，不依赖 full preflight/repair 算法适配 |
| `14D-08-R2` 切片请求映射与真实执行 | **BLOCKED** | 内嵌 scene/profile 到 `scene_config_path` 的物化规则尚未冻结；安全发布仍依赖 14D-05 |
| `14D-08-R3` full preflight/repair 适配 | **BLOCKED** | 两项 Facade 只有接口，没有具体工厂/适配器 |
| `14D-08-R4` 三能力独立调试收口 | **BLOCKED** | 需 R1/R2/R3 和 14D-05 全部完成 |

因此，本文件只解除“共享执行基础”的准备门，不改变原
`DOC_PREP_14D_08_Worker独立调试入口实施准备.md` 对父任务的 `BLOCKED` 结论。

## 2. 已读取依据

### 2.1 项目规则与正式设计

- `AGENTS.md`、`.agents/AGENTS.md`；
- `.agents/docs/SLICE_AI_SKILL_MASTER.md`；
- `.agents/docs/architecture-boundary.md`；
- `.agents/docs/build-and-test.md`；
- `DEV_14_切片能力包封装与打印软件集成.md`；
- `TASKS_14_切片能力包封装与打印软件集成任务清单.md` 的 14D 段；
- `DOC_PREP_14D_08_Worker独立调试入口实施准备.md`；
- `DOC_PREP_14D_05_安全发布与清理双保险实施准备.md`；
- `DOC_PREP_14D_06_重能力Worker唯一路由实施准备.md`。

### 2.2 合同与当前代码

- `contracts/file_contract_v1.md`；
- request/result/contract-info Schema 与 exit-code 表；
- `contracts/slicer_three_lane_contract.json/.md`；
- `apps/slicer_worker/Main.cpp`、`WorkerApplication.*`；
- `src/slicer_module/WorkerClient.*`、`WorkerContract.*`；
- `src/slicer_core/api/SliceFacade.h`、`SliceDtos.h`；
- `src/slicer_core/engine/ProductionSliceFacadeFactory.*`；
- 当前 CMake 中 `slicer_worker -> slicer_engine` 的单向依赖。

## 3. 当前代码事实

### 3.1 已具备

1. `slicer_worker --contract-info` 输出三项冻结 Worker capability：
   `slice.rgbwsv`、`geometry.preflight.full`、`geometry.repair`。
2. `--spi-request` 已要求一个绝对路径，命令行外壳和稳定参数负例存在。
3. `file_contract_v1.request.schema.json` 已冻结身份、超时和按 capability 分支的必填字段。
4. `WorkerClient` 已具备 Job Object、进程树回收、有限超时、取消标记和 stdout 协议解析。
5. `SliceFacade`、`PreflightFullFacade`、`RepairFacade` 的 engine-only 接口已存在。
6. `CreateProductionSliceFacade()` 已存在，且 CLI 已真实使用该工厂。
7. `slicer_worker` 链接 `slicer_engine`；`slicer_module` 仍只链接 `slicer_base`，依赖方向正确。

### 3.2 尚不具备

1. 仓库中没有 `WorkerRequest`、`WorkerJobIdentity`、`WorkerJobDispatcher` 或等价实现。
2. `WorkerApplication::HandleSpiRequest()` 只做参数检查，随后返回 `NotImplemented`。
3. 没有 C++ 侧的 request/result 语义校验、身份闭合和原子 `result.json` 写入服务。
4. 没有 capability 到真实执行器的精确注册表，也没有缺执行器时的统一 fail-closed 路径。
5. 内嵌 scene/profile/output 到 `SliceRequest::scene_config_path` 的映射未冻结。
6. `PreflightFullFacade` 与 `RepairFacade` 仅有抽象接口，未发现可构造的具体工厂。
7. Worker 未接入 14D-05 的作业级 staging/backup 所有权和崩溃恢复。
8. 模块 `pm_submit` 尚未路由到 `WorkerClient`，但该缺口属于 14D-06，不属于本 R1。

## 4. 拆分原则

### 4.1 “共享”的准确含义

共享执行基础是 **Worker 私有运行时组件**。DLL 正式启动 Worker 与开发者直接执行
`slicer_worker --spi-request` 最终都进入同一个 `WorkerApplication -> WorkerJobDispatcher`
链路。不得为独立调试入口复制第二套 parser、dispatcher 或 executor。

本 R1 不要求 `slicer_module` 链接 Worker 私有实现，也不把 engine 拉入 DLL。模块只负责按
文件合同生成请求、启动进程和验证结果；Worker 才解析并执行请求。

### 4.2 R1 可以冻结的边界

- 文件合同 envelope、身份和进程退出语义；
- 请求路径、job 目录、`result.json` 和 `cancel.requested` 的确定方式；
- capability 精确分派与执行端口；
- 缺执行器、身份错误、结果写入失败时的 fail-closed 行为；
- 单进程单作业调度模型；
- 测试替身只能存在于测试 target 的边界。

### 4.3 R1 不得猜测的边界

- `scene` / `profile` 如何物化为生产 scene-config；
- `input` 如何映射到 full preflight/repair DTO；
- package staging、backup、发布租约和崩溃恢复；
- 模块侧 Worker 定位、异步 job 存储和 `backend` 规范化；
- full preflight/repair 的算法和生产准入结论。

## 5. 子任务顺序

### 5.1 `14D-08-R1-01` 请求解析与不可变身份

**目标**：把冻结 request Schema 的 envelope 映射为不含算法对象的 `WorkerRequestEnvelope`
和 `WorkerJobIdentity`。

必须完成：

1. 读取绝对、存在且为普通文件的 request path；拒绝 UTF-8 BOM、空文件、损坏 JSON。
2. 校验 `contract=file_contract`、`major=1`、受支持 minor、jobId、correlationId、
   capability、有限 timeout。
3. 按 capability 校验当前 v1.0 必填字段，但保留原始 `scene/profile/input/output` JSON，
   不在 R1 内解释业务含义。
4. 冻结路径：

```text
jobDirectory = normalized(requestPath.parent_path())
requestPath  = jobDirectory/request.json 的实际传入绝对路径
resultPath   = jobDirectory/result.json
resultTmp    = jobDirectory/result.json.tmp
cancelPath   = jobDirectory/cancel.requested
```

5. `jobId` 不参与拼接父目录；请求中 jobId 必须与 Schema 字符约束一致。
6. 建立身份后全链只传不可变对象，禁止执行器改写 job/correlation/capability。

**出口**：解析成功不代表 capability 执行成功，也不得创建生产包。

### 5.2 `14D-08-R1-02` 结果封装与稳定退出映射

**前置**：R1-01。

必须完成：

1. 单一 `WorkerResultEnvelope` 生成器，强制复制请求身份。
2. `ok=true` 只接受真实执行器返回的 `PM-SLICER-OK-0000` 和非空 output。
3. `ok=false` 必须有稳定 code 和非空 error.message。
4. 先写 `result.json.tmp`、关闭文件，再在同一 job 目录原子替换为 `result.json`。
5. 无法建立可信身份时只输出 stderr 并返回非零，不得猜测 result 路径。
6. 可信身份建立后，缺执行器映射为 `PM-SLICER-INTERNAL-0099`、exit 1；
   合同版本/capability 错误映射为 `PM-SLICER-CONTRACT-0060`、exit 7；
   结果文件写入失败映射为 `PM-SLICER-OUTPUT-0050`、exit 6。
7. 本结果 Writer 只处理 `result.json`，不处理生产 package，不与 14D-05 的 publisher 重叠。

### 5.3 `14D-08-R1-03` 精确调度与命令入口接线

**前置**：R1-01、R1-02。

必须完成：

1. 定义最小 `IWorkerCapabilityExecutor` 或等价端口：输入只读请求/身份和 cancel token，
   输出统一执行结果；异常不得越过调度边界。
2. 调度表只接受三个精确 capability；不允许前缀匹配、默认分支或 unknown fallback。
3. 同一 capability 最多注册一个执行器；重复注册在启动前失败。
4. 一个 Worker 进程只调度 request 中的一个作业，不在 R1 引入线程池或队列。
5. `WorkerApplication::HandleSpiRequest()` 必须只调用共享 runtime，不包含业务切片分支。
6. production registry 在真实执行器尚未接入时保持“未安装”，明确失败；禁止注册
   `AlwaysSuccessExecutor`、空包 executor 或调试专用算法。
7. 单元测试允许注入 fake executor 验证调度和身份，但 fake 只能编译进测试 target，
   生产 `slicer_worker` 的链接图和二进制字符串检查必须证明其不存在。

**R1 总出口**：命令入口已真实完成解析、身份、调度和失败结果闭环，但不宣称任一重能力
已经可生产成功。

### 5.4 `14D-08-R2` 切片请求映射与真实执行（后置）

R2 必须先形成受控映射决定，明确：

1. 内嵌 scene/profile 的规范化、hash 复核与 scene-config 临时物化；
2. 所有资源路径的基准目录、绝对化和逃逸拒绝；
3. `sceneHash` 必须与物化后的 committed scene 一致；
4. `packageDir`、jobId、attemptId 如何传入 14D-05 所有权模型；
5. Worker 内 full preflight 在生产切片前的强制顺序；
6. `CreateProductionSliceFacade()` 的唯一复用，不复制 CLI/切片器实现。

R2 的正向成功必须生成真实 `p0.rgbwsv.2` package 并通过 RIP strict；在 14D-05
安全发布完成前，只能作为受控开发证据，不得形成 M-MVP-CANDIDATE。

### 5.5 `14D-08-R3` full preflight 与 repair 适配（后置）

R3 分两张卡串行或文件隔离后并行：

- R3-01：提供可构造的 `PreflightFullFacade` engine 适配器，输出 authoritative=true；
- R3-02：提供可构造的 `RepairFacade` engine 适配器，并在修复后重跑 strict 诊断。

两项均必须消费 R1 的同一 dispatcher，不得增加第二个命令入口。confirmed self-intersection、
non-manifold 等现有 strict 阻断不得被降级为成功。

### 5.6 `14D-08-R4` 父任务收口（后置）

只有以下条件同时满足时，父任务才可从 `BLOCKED` 改为 `COMPLETE`：

1. 三项 capability 均由真实 executor 执行；
2. 独立入口和 DLL 启动入口调用同一 runtime；
3. 14D-05 安全发布、14D-04B 取消和 14D-06 Worker 唯一路由已闭合；
4. Debug/Release 正负例、RIP strict 和无 fallback 结构门禁通过；
5. 可用 VS Code/Visual Studio 直接启动该命令并命中共享 executor 断点。

## 6. 职责冻结

| 组件 | 责任 | 禁止事项 |
|---|---|---|
| `WorkerApplication` | 命令分派、调用共享 runtime、返回进程退出码 | 解析业务 DTO、写 TIFF、包含 capability 算法分支 |
| `WorkerRequestParser` | 字节读取、envelope/Schema 等价语义校验 | 物化 scene-config、修复模型、创建 package |
| `WorkerJobIdentity` | 固定 request/job/result/cancel 身份和路径 | 扫描其他 job、接受相对/逃逸路径 |
| `WorkerResultWriter` | 身份闭合的 result tmp/replace | 发布 package、清理 staging/backup |
| `WorkerJobDispatcher` | 精确 capability 到唯一 executor 的映射 | 默认成功、未知能力 fallback、算法实现 |
| capability executor | R2/R3 中调用唯一正式 Facade | 复制切片器、绕过 full preflight、修改合同 |
| 14D-05 publisher/recovery | package staging、自检、发布、恢复和双清理 | 解释 request 业务字段、伪造 result 身份 |
| 14D-06 module router | DLL 异步 job、Worker 定位、poll/cancel/result | 链接 engine、在 Worker 失败后进程内执行 |

## 7. 文件所有权

### 7.1 R1 独占文件

建议在 `apps/slicer_worker/runtime/` 建立私有实现，并由一个私有 CMake target 复用：

```text
apps/slicer_worker/runtime/WorkerRequestEnvelope.h
apps/slicer_worker/runtime/WorkerRequestParser.h/.cpp
apps/slicer_worker/runtime/WorkerJobIdentity.h/.cpp
apps/slicer_worker/runtime/WorkerResultEnvelope.h/.cpp
apps/slicer_worker/runtime/WorkerResultWriter.h/.cpp
apps/slicer_worker/runtime/WorkerCapabilityExecutor.h
apps/slicer_worker/runtime/WorkerJobDispatcher.h/.cpp
tests/stage14d_08_r1/*
```

所有 public 接口提供 Doxygen，C++ 使用 Allman 风格；新增源文件遵守 Stage 14 行数门禁，
每文件不超过 500 行。

### 7.2 R1 窄幅共享文件

| 文件 | 允许修改 | 串行规则 |
|---|---|---|
| `apps/slicer_worker/WorkerApplication.*` | 只把 `--spi-request` 接入共享 runtime | R1-03 独占；不得与 14D-05 并行编辑 |
| 根 `CMakeLists.txt` | 注册 runtime/test target | 由集成者串行修改，避免并发冲突 |

### 7.3 相邻任务独占

- `14D-05`：`WorkerArtifactRecovery.*`、package 发布/恢复、`WorkerClient` 二次清理；
- `14D-06`：`src/slicer_module/WorkerJobService.*`、异步路由、backend 规范化；
- `14D-04B`：真实取消终态与残留验收；
- `14D-08-R2/R3`：scene/profile/input 映射和三个 engine executor；
- Stage 16：全部 Stage 16 文档和实现，不在本专项触碰。

### 7.4 明确禁止修改

- SPI v1、11 个导出、15 项能力；
- `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`；
- `contracts/file_contract_v1.*`，除非另有受控合同修订；
- Qt UI、RIP、TIFF Writer、材料策略；
- 当前工作树中的 Stage 16 文件和用户模型资产。

## 8. 正向用例

### 8.1 R1 必须通过

| 编号 | 用例 | 预期 |
|---|---|---|
| P01 | 三种 capability 的最小合法 envelope | 解析成功，身份字段完整且不可变 |
| P02 | 同一请求解析两次 | 规范身份和路径完全一致 |
| P03 | 同 major 含未知可选字段 | 保留/忽略兼容，不改变已知字段 |
| P04 | 每种 capability 注入唯一 fake executor | 只调用对应 executor 一次 |
| P05 | fake 返回成功 output | 测试 target 中生成 Schema 合法、身份闭合的成功 result |
| P06 | fake 返回稳定失败 | result 为 ok=false，code/exit 类别一致 |
| P07 | result 原子写入 | 只观察到完整 result；成功后无 result.tmp |
| P08 | `--spi-request` 使用合法请求但无 production executor | 真实 Worker 明确失败，绝不返回 0 |
| P09 | `--contract-info` | 既有输出和三能力声明字节语义不回退 |
| P10 | Debug/Release | parser/identity/dispatcher 行为一致 |

P05 只证明共享 runtime 的结果封装能力，不是重能力生产成功证据。

### 8.2 后置正向证据

- R2：真实 `slice.rgbwsv`、full preflight、package 自检、RIP strict；
- R3：真实 authoritative preflight 和 repair 后 strict 复检；
- R4：DLL 与独立入口对同一 fixture 的结果身份/错误/产物一致。

## 9. 负例

| 编号 | 负例 | 期望 |
|---|---|---|
| N01 | 相对 request path、参数缺失/多余 | 入口拒绝，不进入 parser/executor |
| N02 | 文件不存在、目录、BOM、空文件、损坏 JSON | 非零退出；无法建立身份时不写 result |
| N03 | contract/major/minor 不兼容 | `PM-SLICER-CONTRACT-0060`，不调 executor |
| N04 | 非法 jobId、空 correlationId、无限/越界 timeout | 拒绝，不创建越界路径 |
| N05 | 未知 capability 或分支必填字段缺失 | 合同失败，不 fallback |
| N06 | result/job/correlation/capability 被 executor 篡改 | writer 拒绝，使用请求身份生成失败结果 |
| N07 | capability 没有真实 production executor | `PM-SLICER-INTERNAL-0099`、exit 1，无 package |
| N08 | 同一 capability 重复注册 | runtime 初始化失败，不选择“最后一个” |
| N09 | executor 抛出异常 | 调度边界转换为稳定 internal failure |
| N10 | result.tmp 写入/替换失败 | `PM-SLICER-OUTPUT-0050`、exit 6，不报告成功 |
| N11 | fake executor 被链接进生产 Worker | 二进制/链接门禁失败 |
| N12 | Worker 失败后启动 DLL 进程内切片 | 结构测试失败 |
| N13 | parser/dispatcher 扫描或删除其他 job/package | 路径安全测试失败 |
| N14 | R1 测试生成空 package 并宣称 slice 成功 | 验收失败；R1 禁止创建生产 package |
| N15 | preflight/repair 无适配器却返回 ok=true | 验收失败，判定伪成功 |
| N16 | stdout 出现普通日志或损坏保留前缀 | 协议测试失败 |
| N17 | result 身份与 request 不一致 | Schema/身份门禁失败 |
| N18 | 修改 Stage 16、SPI/TIFF/材料文件 | 范围审查失败 |

## 10. 验证命令

以下是 R1 实施后的冻结命令；本准备审计未运行尚不存在的 target。

```powershell
cmake --build build-slicesoft/main --config Debug --target `
  slicer_worker `
  stage14d08_r1_worker_runtime_tests
cmake --build build-slicesoft/main --config Release --target `
  slicer_worker `
  stage14d08_r1_worker_runtime_tests

ctest --test-dir build-slicesoft/main -C Debug --output-on-failure `
  -R "^(stage14d08_r1_worker_runtime_tests|stage14d03_worker_contract_unit_tests|slicer_stage14d01_worker_shell_contract_test|file_contract_v1_test)$"
ctest --test-dir build-slicesoft/main -C Release --output-on-failure `
  -R "^(stage14d08_r1_worker_runtime_tests|stage14d03_worker_contract_unit_tests|slicer_stage14d01_worker_shell_contract_test|file_contract_v1_test)$"

python tests/contracts/ValidateFileContract.py
python tests/stage14d_08_r1/ValidateWorkerRuntimeBoundaries.py --repo-root .
python tests/stage14d_08_r1/ValidateNoFakeWorkerExecutor.py `
  --worker build-slicesoft/main/Release/slicer_worker.exe
python tests/architecture/ValidateTargetDependencies.py --repo-root .

git diff --check
```

R1 验收必须另外保留真实命令证据：

```powershell
& .\build-slicesoft\main\Debug\slicer_worker.exe --contract-info
& .\build-slicesoft\main\Debug\slicer_worker.exe `
  --spi-request (Resolve-Path <valid-envelope-without-installed-executor.json>)
```

第二条命令当前阶段的正确结果是**身份闭合的显式失败**，不是成功。

## 11. 与 14D-05/06 的解阻关系

### 11.1 对 14D-05

R1 完成后，14D-05 的 U1 可复用统一 job identity，U2 可在 R2 上实现，result 写入也不再
由 14D-05 临时创造。但 R1 本身不提供真实 slice executor、目标租约或 package 恢复，
所以不能单独把 14D-05 改为 PASS。

### 11.2 对 14D-06

R1 完成后，14D-06 可稳定生成/启动同一文件合同入口，并可证明“无执行器时不 fallback”。
但在 R2/R3 和 14D-05 完成前，重能力仍不可用，14D-06 仍不得标记 COMPLETE。

### 11.3 对 14D-04B/07

- 04B 可复用 R1 job identity 与 cancel path，但仍等待真实执行和 14D-05 清理；
- 07 可复用 R1 正负例框架，但 E-01..08 仍需单独规范冻结。

## 12. 并行与冲突规则

1. R1-01 与 14C-06A 可并行，二者文件无交集。
2. R1-01/R1-02 可在先冻结 DTO 后按目录隔离并行；R1-03 必须在二者合入后串行。
3. R1-03 修改 `WorkerApplication.*`，不得与 14D-05 的 Worker 接线并行。
4. 根 `CMakeLists.txt` 由单一集成者串行修改。
5. 14D-06 不得在 R1 内提前修改 `Exports.cpp`、`HandleRegistry` 或 `WorkerClient`。
6. Stage 16 当前工作树内容视为用户/其他会话修改，必须保持不动。

## 13. 最终门禁

```text
SPLIT_DECISION=ACCEPTED_FOR_PREPARATION
14D_08_R1_PREPARATION_GATE=PASS
14D_08_R1_IMPLEMENTATION_STATUS=IN_PROGRESS
14D_08_R1_01_STATUS=COMPLETE
14D_08_R1_02_PREPARATION_GATE=PASS
14D_08_R1_02_STATUS=READY
14D_08_R1_03_STATUS=PREPARED
14D_08_R2_PREPARATION_GATE=BLOCKED
14D_08_R3_PREPARATION_GATE=BLOCKED
14D_08_PARENT_GATE=BLOCKED
```

`14D-08-R1-01 请求解析与不可变身份` 已按本合同完成并通过 Debug/Release 门禁。下一张
原子卡是 `14D-08-R1-02 result.json 原子写入与稳定退出映射`；其身份、路径和稳定错误
边界已经由 R1-01 提供，因此准备门转为 PASS。不得跳过 R1-02 直接把 R1-03 接入命令
入口，也不得跳过后续映射、真实 Facade、安全发布或取消门禁。
