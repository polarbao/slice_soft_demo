# DOC_PREP_14D_06 重能力 Worker 唯一路由实施准备

> 审计日期：2026-08-06
>
> 对应任务：`14D-06` 取消 `backend=inprocess`，重能力收敛到 Worker
>
> 审计基线：分支 `feature/14-slicer-capability-package`，`HEAD=2edb935`，并读取当前工作树代码
>
> 文档性质：准备审计与 R1/R2 实施收口证据
>
> `PREPARATION_GATE`：**PASS（2026-08-06 复审）**
>
> 实施状态：`R1 COMPLETE / R2 COMPLETE`
>
> 验收状态：`R1/R2 DEBUG/RELEASE PASS`

## 1. 审计结论

当前代码已经做到一件重要但容易被误判的事：**没有发现可工作的进程内重能力路径**。
`geometry.preflight(full)`、`geometry.repair` 和 `slice.rgbwsv` 经当前公开 SPI 提交时均会
fail-closed，不会把 `slicer_engine` 拉入 DLL，也不会静默回退到旧切片入口。

但这不等于 `14D-06` 已完成。当前真实调用链是：

```text
pm_submit
  -> SyncCapabilityAdapter::Execute
     -> 13 项同步轻能力：进程内执行并立即形成终态
     -> geometry.preflight(full)：拒绝，提示必须使用 Worker
     -> geometry.repair / slice.rgbwsv：拒绝，提示必须使用 Worker
```

`WorkerClient` 和文件合同协商已经存在，但没有接入 `pm_submit`；
`slicer_worker --spi-request` 仍明确返回 `NotImplemented`。因此当前状态是
“重能力不可用且无进程内后门”，而不是“重能力已经由 Worker 唯一执行”。

基于代码事实，`14D-06` 的完整实施依赖真实 Worker 请求执行入口及安全发布链路。
`14D-08` 与 `14D-05` 当前准备门均为 `BLOCKED`，同时 `options.backend` 的对外 DTO
仍只约束为任意字符串，没有冻结 `worker` 唯一值及缺省语义。故本次准备门必须判定为：

```text
PREPARATION_GATE=BLOCKED
BLOCKER_1=WORKER_SPI_REQUEST_EXECUTION_NOT_AVAILABLE
BLOCKER_2=OPTIONS_BACKEND_WORKER_ONLY_CONTRACT_NOT_FROZEN
BLOCKER_3=WORKER_SAFE_PUBLISH_PATH_NOT_AVAILABLE
```

不得用“当前 inprocess 已被拒绝”代替正向 Worker 路由验收，也不得在 Worker 不可执行时
把 `14D-06` 标记为 `PASS` 或 `COMPLETE`。

## 2. 已读取依据

本审计读取并交叉核对以下权威资料和当前实现：

- 根目录 `AGENTS.md`、`.agents/AGENTS.md`；
- `TASKS_14_切片能力包封装与打印软件集成任务清单.md`；
- `CODEX_PROMPT_14_切片能力包封装与打印软件集成执行指令.md`；
- `DEV_14_切片能力包封装与打印软件集成.md`；
- `contracts/slicer_three_lane_contract.json/.md`；
- `contracts/slicer_capability_dtos.json/.md`；
- `contracts/file_contract_v1.md` 及 request/result/exit-code 合同；
- `src/slicer_module/ModuleInfo.*`；
- `src/slicer_module/SyncCapabilityAdapter.*`；
- `src/slicer_module/WorkerClient.*`、`WorkerContract.*`；
- `src/slicer_module/Exports.cpp`、`module.json.in`、`HandleRegistry.*`；
- `apps/slicer_worker/WorkerApplication.*`；
- 当前 `CMakeLists.txt` 中 module/worker 的依赖和测试路由；
- `DOC_PREP_14D_05_安全发布与清理双保险实施准备.md`；
- `DOC_PREP_14D_08_Worker独立调试入口实施准备.md`。

共享任务表将 `14D-06` 粗粒度登记为 `PREPARED`，但同表明确 `14D-08` 为
`PREPARATION_GATE BLOCKED`。本文件按用户要求以当前代码事实重新审计，不修改共享任务表；
后续只有在阻塞关闭后，才可由对应状态任务同步共享表。

## 3. 冻结目标与承载边界

### 3.1 重能力的唯一承载

| 公开能力 | 模式 | 唯一承载 | 私有 Worker 能力名 |
|---|---|---|---|
| `geometry.preflight` | `fast` | DLL 进程内同步 | 不进入 Worker |
| `geometry.preflight` | `full` | Worker 异步 | `geometry.preflight.full` |
| `geometry.repair` | 不适用 | Worker 异步 | `geometry.repair` |
| `slice.rgbwsv` | 不适用 | Worker 异步 | `slice.rgbwsv` |

公开 DTO 继续使用 15 项能力集合，不新增 `geometry.preflight.full` 公共能力。
`.full` 只属于 `file_contract_v1` 的模块到 Worker 私有能力身份。

### 3.2 同步轻能力边界

以下 13 项继续由 DLL 进程内同步执行，不得为了复用 Worker 路由而搬入 Worker：

```text
model.import
model.get_metadata
model.release
scene.apply_operation
scene.get_snapshot
scene.get_viewdata
geometry.preflight(mode=fast)
geometry.collision
package.verify
package.get_summary
package.get_layer_descriptor
package.render_layer_preview
package.read_report
```

边界要求：

1. 轻能力首次 `pm_poll` 即可得到终态，不启动 `slicer_worker.exe`。
2. `geometry.preflight` 必须先读取 `mode` 再选择承载，不能仅按 capability 字符串路由。
3. `mode=fast` 返回 `authoritative=false`；生产准入只认可 Worker 内的 full preflight。
4. `options.backend` 不得改变任何轻能力的承载，也不得成为将 engine 拉入 DLL 的入口。
5. `slicer_module` 只链接 `slicer_base`；`slicer_worker` 链接 `slicer_engine`。

### 3.3 `options.backend` 收敛规则

`options.backend` 只属于 `slice.rgbwsv` 请求的兼容字段，不是切片算法选择器，也不适用于
同步轻能力。解除 B2 前需要受控冻结以下规范：

| 输入 | 规范化结果 | 是否启动 Worker | 预期 |
|---|---|---:|---|
| 缺少 `options.backend` | `worker` | 是 | 兼容旧调用方，默认唯一 Worker 路由 |
| `options.backend="worker"` | `worker` | 是 | 合法显式值 |
| `options.backend="inprocess"` | 无 | 否 | fail-closed，`PM-SLICER-PROFILE-0031` |
| `options.backend="auto"` | 无 | 否 | fail-closed，禁止自动回退 |
| 其他字符串 | 无 | 否 | fail-closed，`PM-SLICER-PROFILE-0031` |
| 非字符串 | 无 | 否 | 请求类型错误，`PM-SLICER-INPUT-0002` |

实现不得保留以下兼容行为：

- `inprocess`、`auto`、`legacy`、`openvdb` 等值映射到 DLL 内切片；
- Worker 启动、协商或执行失败后回退到进程内切片；
- 通过环境变量、隐藏参数、测试开关或 Debug 分支恢复第二条生产路径；
- 把 `options.backend` 与 Stage 12E 的纹理分区 backend、TIFF Writer backend 混为一谈。

若决定完全删除 `options.backend` 字段，也必须先解决既有请求兼容：字段缺省继续等价于
Worker，显式旧值必须稳定拒绝，不能因 JSON 未知字段规则而被静默忽略。

## 4. 当前代码事实

| 证据 | 当前事实 | 对 `14D-06` 的含义 |
|---|---|---|
| `ModuleInfo.cpp:31-36` | 声明 15 项能力；13 项 `syncCapabilities`；Worker 声明 preflight/repair/slice | 自述边界已具备，preflight 按模式重叠是有意设计 |
| `SyncCapabilityAdapter.h:14-28` | 同步数组固定为 13 项 | 轻能力边界与 `DEV_14` §5 一致 |
| `SyncCapabilityAdapter.cpp:103-139` | full preflight 与 repair/slice 均 fail-closed | 没有进程内重能力后门，但也没有 Worker 正向路由 |
| `Exports.cpp:64-98` | `pm_create(optionsJson)` 当前忽略 options | 尚无模块级 Worker 路径、临时根目录或策略配置入口 |
| `Exports.cpp:117-185` | `pm_submit` 无条件调用同步适配器 | 重能力不会进入 `WorkerClient` |
| `Exports.cpp:187-287` | poll/result/release 只读取同步适配器终态存储 | 尚无异步 Worker job 存储、进度和结果路由 |
| `WorkerClient.h:60-88` | 已有进程路径、参数、超时、取消标记和结果模型 | `14D-02` 基础可复用 |
| `WorkerContract.cpp:165-223` | 可执行 `--contract-info` 并 fail-closed 协商 | `14D-03` 基础可复用，但尚未被 Exports 调用 |
| `WorkerApplication.cpp:114-141` | `--spi-request` 仅校验绝对路径后返回 NotImplemented | **阻塞任何真实重能力执行** |
| `module.json.in:35-38` | 声明同包 `slicer_worker.exe` 与 `file_contract_v1` | 部署声明存在，但模块侧解析/定位尚未接线 |
| `CMakeLists.txt:535-577` | module 包含 WorkerClient，但只链接 `slicer_base` | 依赖方向正确，不得改为链接 engine |
| `CMakeLists.txt:836-845` | Worker 链接 `slicer_engine` | 重能力实现的正确承载位置 |
| `slicer_capability_dtos.json:307-323` | slice 的 backend 仅为可选 string | `worker` 唯一值、默认和拒绝规则尚未机器冻结 |
| `file_contract_v1.md:14-22` | 私有重能力三项，明确无 in-process slicing | 目标方向已冻结 |
| 三车道合同 Production 段 | owner 为 Worker，要求 committed sceneHash、Worker full preflight、无静默回退 | 生产路由必须在 Worker 内重新权威校验 |

### 4.1 当前 backend 路由的准确判断

仓库生产代码中没有发现消费 `options.backend` 的路由实现；该字段目前只出现在能力 DTO
描述中。当前也没有另一段隐藏的 `backend=inprocess` SPI 切片代码可直接删除。

因此 `14D-06` 的实施重点不是机械删除一个分支，而是：

1. 在统一公共请求路由器中把重能力识别出来；
2. 对 slice backend 做唯一值规范化和负例拒绝；
3. 将重能力交给异步 Worker job 服务；
4. 让 `pm_poll/cancel/result/release/destroy` 都按 job carrier 分派；
5. 用结构门禁证明 module 不依赖 engine，且 Worker 失败无回退。

只增加 `if (backend == "inprocess") reject` 不能完成本任务。

## 5. 阻塞项与前置关系

### 5.1 B1：`14D-08` 真实 Worker 执行入口未具备

`--spi-request` 目前返回 NotImplemented。`14D-08` 准备文档又确认以下两项仍未冻结：

- 内嵌 `scene/profile/output` 到当前 Facade 输入的映射和临时物化生命周期；
- `geometry.preflight.full`、`geometry.repair` 的具体 Engine 适配器。

`14D-06` 可以设计 DLL 路由，但在共享 `WorkerJobExecutor` 或等价入口可执行之前，无法完成
正向路由验收。允许的解阻方式只有二选一：

1. 完整解除并完成 `14D-08`；或
2. 经授权从 `14D-08` 拆出模块与独立调试入口共同复用的请求解析、三能力分派和结果写入基础，
   先完成该共享前置，再分别接入 DLL 与命令行。

禁止 DLL 自己再实现一套重能力执行器来绕开 `14D-08`。

### 5.2 B2：`options.backend` 对外合同不完整

当前 DTO 允许任意字符串，也没有声明缺省值。直接写实现会让以下行为由代码偶然决定：

- 缺省时是否启动 Worker；
- `inprocess`、`auto`、大小写变体及未知值是拒绝还是忽略；
- 类型错误对应 input 还是 profile 错误；
- 老请求携带旧字段时是否形成静默兼容。

实施前必须受控更新 `slicer_capability_dtos.json/.md` 及合同测试。由于 DTO v1.2 已冻结，
这属于受控合同修订，不能由 `14D-06` 代码提交顺手暗改。

### 5.3 B3：`14D-05` 安全发布链路未具备

`slice.rgbwsv` 的成功验收必须包含 staging、自检、原子发布、旧包保护和失败/取消清理。
`14D-05` 当前准备门为 BLOCKED，且其最小 `slice.rgbwsv` Worker 适配本身也依赖真实
`--spi-request`。在该链路完成前，`14D-06` 最多只能证明“进程启动和拒绝语义”，不能证明
“唯一 Worker 路由能够安全地产生生产包”。

### 5.4 已满足与非阻塞前置

| 任务 | 状态 | 结论 |
|---|---|---|
| `14D-01` | 已完成 | Worker 可执行外壳存在 |
| `14D-02` | 已完成 | WorkerClient 进程、协议、取消和超时基础可用 |
| `14D-03` | 已完成 | 合同发现和版本协商可复用 |
| `14D-04A` | 已完成 | 核心取消令牌已贯穿 |
| `14C-04` | 已完成 | 13 项同步轻能力边界已接线 |
| `14D-04B` | 等待后置前置 | 不阻塞路由编码，但阻塞完整取消 E2E 关闭 |
| `14D-07` | BLOCKED | 不阻塞最小路由编码，但阻塞 Worker 替换准入结论 |

## 6. 解阻后的唯一实施结构

### 6.1 推荐路由

```text
pm_submit
  -> CapabilityCarrierRouter
     -> in_process_light
        -> SyncCapabilityAdapter
     -> worker_heavy
        -> backend 规范化
        -> Worker 合同协商/缓存
        -> 写 file_contract_v1 request.json
        -> WorkerJobService 异步启动 WorkerClient
        -> HandleRegistry 发布 Running

pm_poll    -> 按 job carrier 读取同步终态或 Worker 进度
pm_cancel  -> HandleRegistry 置 Cancelling + WorkerClient::RequestCancel
pm_result  -> 按 job carrier 返回同步结果或校验后的 result.json
pm_release -> 停止/清理当前 job，再释放 carrier 私有状态
pm_destroy -> 取消并回收该 module 的全部 Worker 作业
```

路由器只决定承载，不做切片、修复、full preflight 或材料策略。

### 6.2 Worker 可执行文件定位

部署合同已冻结 DLL、Worker 和 `module.json` 同属 `modules/slicer/` 包。解阻后应由模块上下文
解析自身部署目录和 `module.json.subprocess.exe`，生成绝对 Worker 路径并验证：

- 路径位于模块部署目录内；
- 文件存在且为普通文件；
- `subprocess.protocol == file_contract_v1`；
- 首个重作业前协商成功；
- 替换 Worker 后按可审计身份重新协商，不复用过期结果。

不得从当前工作目录搜索可执行文件，不得无约束消费 PATH，也不得在请求里允许调用方覆盖
Worker 路径。测试替身只能通过测试专用依赖注入进入，不进入生产 options。

## 7. 文件所有权

以下为解除阻塞后的建议所有权。未解除前不得开始代码修改。

### 7.1 `14D-06` 可拥有

| 文件/目录 | 允许操作 | 责任 |
|---|---|---|
| `src/slicer_module/CapabilityCarrierRouter.*` 或等价文件 | 新增 | 轻/重能力与 preflight 模式分类，backend 规范化 |
| `src/slicer_module/WorkerJobService.*` 或等价文件 | 新增 | Worker 作业、协商缓存、进度、结果、取消和清理协调 |
| `src/slicer_module/Exports.cpp` | 窄幅修改 | 11 个 ABI 导出按 job carrier 接线，不增加导出 |
| `src/slicer_module/HandleRegistry.*` | 窄幅扩展 | 记录 carrier、Running 状态和 Worker job 归属，不保存引擎对象 |
| `src/slicer_module/ModuleInfo.*` | 原则上不改 | 只做一致性验证；15 项能力和 13 项 sync 数量不变 |
| `src/slicer_module/SyncCapabilityAdapter.*` | 原则上不改 | 继续只拥有同步轻能力，不接 Worker，不接 engine |
| `src/slicer_module/module.json.in` | 原则上不改 | 已声明 Worker；仅在受控合同要求时窄改 |
| `tests/stage14d_06/*` | 新增 | 路由、backend 规范化、无回退和 ABI 生命周期测试 |
| `CMakeLists.txt` | 窄幅修改 | 注册新 module 源和测试；保持 module 只链接 base |

### 7.2 不属于 `14D-06`

| 文件/目录 | 所有者/前置 | 禁止事项 |
|---|---|---|
| `apps/slicer_worker/WorkerApplication.*`、Worker 请求解析/执行器 | `14D-08` 或其授权拆分前置 | 不在 DLL 路由任务中临时实现第二套 Worker 执行器 |
| full preflight/repair Engine 适配器 | `14D-08` 解阻前置 | 不返回假成功，不缩小已声明能力但不改合同 |
| staging/原子发布/恢复组件 | `14D-05` | 不复制路径删除或发布逻辑 |
| `contracts/slicer_capability_dtos.*` | 受控合同修订 | 必须先修订并通过合同门，不与代码暗改混交 |
| `src/slicer_core/engine/**`、材料/TIFF/RGBWSV 实现 | 既有引擎所有者 | 不为路由任务改变生产字节或材料语义 |
| `apps/slicer_debug_ui/**` | 14E | 不增加 UI backend 开关 |
| `contracts/print_module_spi.h`、`.def` | 冻结 ABI | 不改 SPI v1、11 个导出、调用约定 |

共享任务表、状态报告和 README 只在对应状态同步任务中修改，本准备审计不拥有这些文件。

## 8. 验收策略与命令

以下命令是**解阻并完成实现后**的验收入口，本次准备审计未运行它们，也不得记录为 PASS。
目标名允许在实现卡冻结时做等价命名，但覆盖范围不得缩减。

### 8.1 合同与静态门禁

```powershell
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
python tests/contracts/ValidateFileContract.py
python tests/stage14d_06/ValidateWorkerOnlyHeavyRouting.py --repo .
```

静态门禁至少验证：

- `slicer_module` 不链接 `slicer_engine`；
- 重能力不出现在同步适配器的可执行分支；
- 无 `backend=inprocess`、`auto` 或环境变量回退；
- ModuleInfo 仍是 SPI v1、11 导出、15 能力、13 项同步声明；
- 正式路由和独立调试入口复用同一 Worker 请求执行器。

### 8.2 Debug/Release 构建与单测

```powershell
cmake --build build-slicesoft/main --config Debug --target slicer_module slicer_worker stage14d06_worker_routing_tests
cmake --build build-slicesoft/main --config Release --target slicer_module slicer_worker stage14d06_worker_routing_tests
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "stage14d06|stage14d02|stage14d03|stage14c04|stage14c05"
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R "stage14d06|stage14d02|stage14d03|stage14c04|stage14c05"
```

### 8.3 正向路由与生产门禁

```powershell
& .\build-slicesoft\main\Debug\slicer_worker.exe --contract-info
& .\build-slicesoft\main\Debug\slicer_worker.exe --spi-request (Resolve-Path <slice-request.json>)
& .\scripts\run_ci_quick.ps1
```

正向 SPI 测试需使用进程观察器或可注入的 WorkerClient 替身证明：

1. backend 缺省和显式 `worker` 均只启动一次 Worker；
2. `geometry.preflight(full)`、repair、slice 均进入 Worker；
3. fast preflight 和 12 项其他轻能力均启动 0 次 Worker；
4. Worker 失败不会执行进程内引擎；
5. slice 成功包通过 `p0.rgbwsv.2`、RIP strict 和 RepairDisabled SHA-256 门禁；
6. Debug/Release 的 carrier、错误码和结果一致。

### 8.4 文档与差异检查

```powershell
git diff --check
git status --short
```

## 9. 必测负例

| 编号 | 负例 | 预期结果 |
|---|---|---|
| N-01 | slice 使用 `backend=inprocess` | 提交前拒绝，Worker 启动 0 次，无包写入 |
| N-02 | slice 使用 `backend=auto` | 提交前拒绝，不回退 |
| N-03 | slice 使用未知 backend 或大小写变体 | `PM-SLICER-PROFILE-0031`，不做宽松猜测 |
| N-04 | backend 为数字、布尔、数组、对象或 null | `PM-SLICER-INPUT-0002` |
| N-05 | backend 缺省 | 规范化为 Worker；若 Worker 不可用则明确失败，不回退 |
| N-06 | Worker 文件缺失、不可执行或启动失败 | 稳定失败，进程内重能力调用次数为 0 |
| N-07 | `--contract-info` major/minor/produces/capability 不兼容 | 启动作业前 fail-closed |
| N-08 | Worker 崩溃、超时或协议行损坏 | 稳定错误；不产生伪成功，不回退 |
| N-09 | `geometry.preflight(full)` 被送入同步适配器 | 路由测试失败 |
| N-10 | `geometry.preflight(fast)` 启动 Worker | 路由测试失败 |
| N-11 | repair/slice 被同步终态存储直接标记成功 | 路由测试失败 |
| N-12 | stale/uncommitted sceneHash 进入生产 | Worker full preflight 前后均 fail-closed，返回稳定布局错误 |
| N-13 | result 的 jobId/correlationId/capability 不闭合 | 结果拒绝，job 失败 |
| N-14 | Worker 失败后已有成功包被覆盖或留下 staging | 生产安全门失败 |
| N-15 | `pm_cancel` 仅改状态但 Worker 仍运行 | 取消门失败；不得提前标记 Cancelled |
| N-16 | `pm_destroy` 后子进程或 job 状态残留 | 生命周期门失败 |
| N-17 | 轻能力请求携带 backend 提示 | 不得改变 carrier，不得启动 Worker |
| N-18 | Debug 构建存在测试专用 inprocess 回退 | 静态和运行时门均失败 |

## 10. 门禁解除清单

将本文件由 `BLOCKED` 改为 `PASS` 前必须提供以下证据：

- [x] `options.backend` 的 worker-only 缺省、枚举、错误码和兼容规则已受控冻结；
- [x] 共享 Worker 请求执行基础可真实执行 full preflight、repair 与 slice；
- [x] `14D-05` 最小安全发布链可供 `slice.rgbwsv` 正向验收；
- [x] Worker 可执行文件按模块部署目录定位，模块销毁会取消并回收其全部作业；
- [x] 异步 job carrier、poll/cancel/result/release/destroy 的状态与所有权已冻结；
- [x] 文件所有权无 14D-05/08 并行冲突；
- [x] 正向三能力、backend 负例、取消及稳定结果码已进入自动化测试；
- [x] Debug/Release 构建目标和 CTest 名称已确认并通过。

截至 2026-08-06 复审，前 3 项已经关闭：三项生产 Worker executor 已接入，
`options.backend` 已由 DTO v1.4 冻结为缺省且唯一合法值 `worker`，Writer 与模块恢复链路已完成
R1..R3 和真实 Worker R4-A。允许继续进行的下一步是：

1. 将 `CapabilityCarrierRouter` 接入 11 个冻结 ABI 的 job carrier 分派；
2. 实现异步 Worker job 的 poll/cancel/result/release/destroy；
3. 通过公开 DLL 完成 slice、full preflight 与 repair 正向路由；
4. 关闭 14D-05-R4-B 和 C-SPI-09。

## 11. 最终结论

```text
NO_INPROCESS_HEAVY_PATH_OBSERVED=true
WORKER_HEAVY_ROUTE_USABLE=true
PREPARATION_GATE=PASS
IMPLEMENTATION=R1_COMPLETE_R2_COMPLETE
ACCEPTANCE=R1_R2_DEBUG_RELEASE_PASS
```

R1 已完成 carrier 分类、full preflight 私有能力映射及 worker-only backend 规范化；
非 `worker` 字符串稳定返回 `PM-SLICER-PROFILE-0031`，类型错误返回
`PM-SLICER-INPUT-0002`。R2 已新增进程级 `WorkerJobService`，公共 SPI 通过异步作业唯一调用
同包 `slicer_worker`，并将 poll/cancel/result/release/destroy 生命周期、私有 job 根目录、
合同协商、结果身份闭合、修复资产发布和 Package 恢复串成一条链。full preflight、repair、
slice 与取消用例均通过 Debug/Release，模块目标仍只链接 `slicer_base`，不存在进程内重能力回退。

## 12. R2 实施证据

| 证据 | 结果 |
|---|---|
| 公共 SPI full preflight | Worker 异步执行并返回 authoritative/complete 结果 |
| 公共 SPI repair | Worker 生成 job-owned 资产，模块发布到请求的绝对输出路径，release 后仍存在 |
| 公共 SPI slice | Worker 写入 `p0.rgbwsv.2` Package，并通过严格包校验 |
| backend 负例 | `inprocess` 在启动 Worker 前稳定拒绝，不产生 Package |
| 取消 | `pm_cancel` 同时写入协作取消标记并通知活动进程，终态与结果码均为 cancelled |
| 生命周期 | `pm_release`/`pm_destroy` 先取消并 join Worker，再退休公开句柄和私有 job 目录 |
| 架构边界 | 静态门禁确认 `slicer_module` 不链接或调用 `slicer_engine` |
| 构建与测试 | Debug/Release 选定 CTest 5/5 PASS；Debug 公共路由测试连续 3 次 PASS |
| 合同门禁 | Capability DTO、Three Lane、File Contract 与 Worker-only 路由校验 PASS |

下一任务为 `14D-05-R4-B`：在公共 DLL 链路补齐旧包保护和 C-SPI-09 证据。
