# DOC_PREP_14D-05 安全发布与清理双保险实施准备

> 日期：2026-08-06
> 审计任务：Stage 14D-05
> 审计基线：`fba8b04 feat(14D-08-R3-02B-F1): 【生产修复】闭合保守修复Facade`
> 文档状态：`PREPARATION_GATE=PASS_WITH_SPLIT`
> 实施状态：`R1..R3 COMPLETE / R4 NOT STARTED`
> 验收状态：`R3 DEBUG/RELEASE DIRECTED PASS / R4 NOT RUN`

## 1. 审计结论

14D-05 的目标不能仅解释为“Writer 已经使用 staging 目录”。正式目标包含以下完整闭环：

1. 本次任务只写入当前作业拥有的 staging；
2. staging 完整写入后执行生产包自检；
3. 自检成功后在同一父目录内发布，并保护上一个有效包；
4. 协作取消、Worker 异常退出、超时强杀和进程崩溃后均能清理本次临时产物；
5. Worker 先清理，模块在确认进程退出后再次幂等清理；
6. 清理失败不得伪装成 `Cancelled` 或成功；
7. 查询、读取和宿主侧使用路径不得接受 staging、backup、tmp 目录。

复审基线已经具备严格请求身份、真实 `slice.rgbwsv` executor、生产 SliceFacade 和
ProductionRepairFacade。旧审计中“Worker 入口完全未实现”的描述已经失效；当前真实缺口是
生产 Worker 已注册 `slice.rgbwsv` executor，Writer 安全发布、Worker/模块两轮清理和临时路径
读取拒绝已接线；当前剩余缺口是 R4 的真实 Worker、取消、超时、强杀和崩溃验收。

本文件现将实施受控拆为 R1..R4。拆分边界、文件所有权、状态机和验收命令均已冻结，因此
准备门改为 `PASS_WITH_SPLIT`；这只授权按顺序实施，不等于 14D-05 完成，也不构成
C-SPI-09、M-MVP-CANDIDATE 或任何 14E 准入证据。

| 子任务 | 范围 | 当前状态 |
|---|---|---|
| 14D-05-R1 | 共享 job/attempt 产物身份、临时路径识别、精确 owned 恢复 | COMPLETE |
| 14D-05-R2 | Writer 使用 owned staging/backup、同目标租约与发布证据 | COMPLETE |
| 14D-05-R3 | Worker 第一轮、模块第二轮清理及查询/RIP 临时路径拒绝 | COMPLETE |
| 14D-05-R4 | Debug/Release、取消、强杀、崩溃和真实 Worker 集成验收 | NOT STARTED |

### 1.1 最小解阻条件

只有以下最小任务全部关闭后，14D-05 才能改为 `PREPARATION_GATE=PASS` 并进入生产代码实施：

| 编号 | 最小解阻任务 | 完成判据 |
|---|---|---|
| U1 | 冻结并实现作业级临时产物身份 | `jobId` 从 `file_contract_v1` 请求映射到写包请求；staging/backup 名称可唯一归属到目标包、作业和一次尝试 |
| U2 | 提供最小 `slice.rgbwsv` Worker 执行适配 | `slicer_worker --spi-request` 至少能解析合法切片请求、取得绝对 `packageDir` 和 `jobId` 并调用既有切片 Facade；完整 preflight/repair 可继续留在 14D-08 |
| U3 | 提供 Worker/模块共享的路径安全与恢复组件 | 两侧使用同一归一化、所有权识别、恢复和清理规则；不得各自拼接字符串或递归扫描任意目录 |
| U4 | 冻结同目标并发策略 | 同一归一化 `packageDir` 同时只允许一个发布作业；第二个作业在启动写包前以 `PM-SLICER-RESOURCE-0041` 失败 |
| U5 | 冻结两步替换的崩溃恢复状态机 | 覆盖“旧包已移到 backup、staging 尚未发布”窗口，并能在目标缺失时恢复唯一且属于该作业的旧包 |
| U6 | 临时路径读取拒绝落地 | 包查询、RIP Reader 和严格验证入口显式拒绝 `.staging.*`、`.backup.*`、`.tmp`、`.bak` 目标 |

U2 只要求解除 14D-05 所需的窄切片路径阻塞，不代表 14D-08 的完整
`geometry.preflight.full`、`geometry.repair` 或全部输入映射已完成。

## 2. 已读取依据

本审计基于以下当前仓库事实，不依据任务表中的目标描述推断实现状态：

- `AGENTS.md`、`.agents/AGENTS.md`；
- `TASKS_14_切片能力包封装与打印软件集成任务清单.md`；
- `CODEX_PROMPT_14_切片能力包封装与打印软件集成执行指令.md`；
- `DEV_14_切片能力包封装与打印软件集成.md`；
- `DEMO_14_切片能力包封装与打印软件集成验收方案.md`；
- `contracts/file_contract_v1.md` 及 request/result/exit-code Schema；
- `contracts/slicer_cancel_contract.json/.md` 与稳定错误码合同；
- `apps/slicer_worker/` 当前实现；
- `src/slicer_module/WorkerClient.h/.cpp`；
- `src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.h/.cpp`；
- RGBWSV Writer、包验证器和 RIP Reader 现有测试；
- `REPORT_14D_04A_核心取消令牌贯穿当前状态.md` 及其实现证据。

## 3. 当前代码事实与缺口

| 领域 | 当前事实 | 14D-05 判断 |
|---|---|---|
| Worker 入口 | `--spi-request` 已接共享 runtime；full preflight 与 `slice.rgbwsv` executor 均已注册 | R3 已关闭生产注册缺口 |
| WorkerClient | 已有 Windows Job Object、超时、取消标记、强杀和 stdout 协议解析 | 可复用 |
| WorkerClient 清理 | `WorkerLaunchOptions.packageArtifacts` 承载 `packageDir/jobId/attemptId`；进程完全退出后执行共享恢复组件 | R3 已完成模块第二保险 |
| Writer staging | 仍使用 `<package>.staging.<steady_clock>`；R1 已提供 `<package>.staging.<job>.<attempt>` 身份组件 | **R2 负责替换生产接线** |
| Writer 自检 | 写完 manifest/report 后调用场景扩展校验和 `validate_slice_package(stagingDir)` | 已实现正常路径基础 |
| Writer 发布 | 旧目标先 rename 为 backup，再将 staging rename 为目标；第二步失败时尝试恢复 backup | 已有可恢复两步替换雏形 |
| Writer 取消 | 14D-04A 已在阶段、逐层、长循环、TIFF 和发布边界检查 token；异常路径删除当前 staging | 已完成进程内协作取消基础 |
| 崩溃恢复 | Worker 在执行起止各调用精确 owned 恢复，模块在进程退出后再次调用；强杀/崩溃实测仍归 R4 | R3 接线完成，R4 待验收 |
| 旧包保护 | 正常异常能尝试恢复 backup；进程在两次 rename 之间崩溃时，目标可能缺失且 backup 遗留 | **阻塞崩溃场景** |
| 清理错误 | Worker/模块清理失败均覆盖原终态并稳定映射 `PM-SLICER-OUTPUT-0050` | R3 已关闭失败语义 |
| 包入口拒绝 | 对外 PackageQuery/RIP 严格拒绝 staging/backup/lease/tmp/bak；Writer 私有验证仅接受精确 owned 产物 | R3 已关闭 fail-closed 缺口 |
| 同目标并发 | 没有目标级互斥或发布租约 | **阻塞所有权安全** |
| 断电持久性 | 当前没有目录/文件 `fsync` 或 Windows `FlushFileBuffers` 证据 | 不纳入 14D-05，不得宣称抗断电 |

### 3.1 “原子发布”的准确含义

本阶段冻结的“原子”是**同一卷、同一父目录 rename 的可见性边界**，不是跨卷复制，也不是
断电持久性保证。Windows 上替换一个已有非空目录不能依赖单次 rename，因此当前方案是可恢复的
两步事务：

```text
target(valid old package)
  -> target.backup.<job>.<attempt>
staging(verified new package)
  -> target
remove backup
```

实施与报告不得把该流程表述成操作系统级“单指令目录交换”。14D-05 覆盖正常进程、协作取消、
超时强杀和进程崩溃后的恢复；突然断电、文件系统损坏和存储设备缓存丢失属于后续耐久性专项。

## 4. 冻结发布状态机

### 4.1 临时产物命名与所有权

目标包必须是绝对、词法归一化后的目录 `packageDir`。临时目录必须与目标同父目录，名称冻结为：

```text
<packageName>.staging.<jobId>.<attemptId>
<packageName>.backup.<jobId>.<attemptId>
```

- `jobId` 必须先通过 `file_contract_v1.request.schema.json` 的字符约束；
- `attemptId` 由当前进程生成，必须在同一目标内唯一；
- 只允许操作名称完全匹配当前 `packageName + jobId + attemptId` 的目录；
- 恢复启动时可枚举同一 `packageName` 的临时兄弟，但必须先解析并验证名称；
- 任何候选的最终规范路径必须仍位于 `packageDir.parent_path()`；
- 遇到符号链接、junction/reparse point 或解析后逃逸父目录时必须 fail-closed；
- 禁止对 `tempRoot`、目标父目录或来源不明路径执行宽泛 `remove_all`。

### 4.2 正常发布

```text
Prepare
  -> CreateOwnedStaging
  -> WriteAllLayersAndReports
  -> FlushAndCloseAllWriterHandles
  -> ValidateSceneExtension
  -> ValidateRgbwsvPackage
  -> MarkStagingVerifiedInMemory
  -> MoveExistingTargetToOwnedBackup (only when target exists)
  -> RenameVerifiedStagingToTarget
  -> ValidatePublishedTargetIdentity
  -> RemoveOwnedBackup
  -> Success
```

约束：

1. 自检失败时不得移动旧目标；
2. 所有 TIFF、manifest 和 report 句柄关闭后才允许自检和 rename；
3. staging 与 target 不在同一卷时直接拒绝，不降级为 copy；
4. 发布后发现目标身份不符时不得报告成功；
5. backup 清理失败时，新包可保持发布，但作业必须返回失败并使用
   `PM-SLICER-OUTPUT-0050`，不得伪造“无残留成功”。

### 4.3 失败、取消与崩溃恢复

| 观察状态 | 必须动作 | 最终结果 |
|---|---|---|
| target 存在，只有 owned staging | 删除 owned staging，不触碰 target | 失败/取消；旧包保留 |
| target 存在，只有 owned backup | 验证 target 已是发布包后删除 owned backup | 成功清理；若无法确认则失败并保留证据 |
| target 存在，staging 与 backup 都存在 | 不覆盖 target；删除 owned staging；确认 target 后删除 owned backup | 清理成功或 `OUTPUT-0050` |
| target 缺失，存在唯一 owned backup | 将 backup rename 回 target，再删除 owned staging | 恢复旧包；当前作业失败/取消 |
| target 缺失，只有 verified staging | **崩溃恢复不得继续发布**；删除 staging | 当前作业失败，避免模块在未知时点代替 Worker 提交 |
| target 缺失，存在多个或无法归属的 backup | 不猜测、不删除、不任选一个恢复 | `OUTPUT-0050`，保留证据并输出路径 |
| 任意清理/恢复失败 | 不进入 `Cancelled` 或 success | `PM-SLICER-OUTPUT-0050` |

取消状态冻结为：

```text
Running -> Cancelling
  -> Worker exits
  -> Worker cleanup/recovery
  -> module closes Job Object
  -> module repeats idempotent cleanup/recovery
  -> no owned residue ? Cancelled : Failed(OUTPUT-0050)
```

`cancel.requested` 出现只表示请求已发出，不代表清理已经完成。

### 4.4 旧包保护

- 旧包只有在新 staging 已完成自检后才允许移动；
- 任何新包写入失败、自检失败或发布前取消均不得改动旧包；
- 目标移到 backup 后发布失败，Writer 必须立即尝试恢复；
- Writer 被强杀时，由 Worker 启动/退出清理和模块退出后清理恢复唯一 owned backup；
- 不允许用未自检 staging 覆盖旧包；
- 不允许删除与当前目标同父目录但不属于当前作业的其他包或临时目录；
- 已有有效包的文件集合、字节和严格验证结果必须在失败前后保持一致。

## 5. Worker、Writer 与模块职责

### 5.1 RGBWSV Writer

Writer 是**包内容和发布事务的唯一生产责任方**：

- 创建本次 owned staging；
- 写入 TIFF、manifest、reports 和受控 preview；
- 关闭文件句柄并完成 staging 自检；
- 执行旧包备份、新包 rename 和即时回滚；
- 处理进程内异常与协作取消时的本次 staging 清理；
- 返回发布、恢复、清理和残留证据。

Writer 不负责启动 Worker、不读取 `cancel.requested`、不终止进程，也不扫描任意历史作业目录。

### 5.2 slicer_worker

Worker 是**文件合同执行和进程内恢复责任方**：

- 校验 request、`jobId`、绝对 `packageDir` 和 capability；
- 在开始写包前执行同目标 stale-owned 临时产物恢复；
- 将作业身份和取消 token 传入 Facade/Writer；
- 捕获稳定异常并在退出前执行第一轮幂等恢复/清理；
- 只有在发布成功且没有 owned 残留时才写 success result；
- 取消只有在清理成功后才写 `stagingRemoved=true`、`published=false` 和退出码 8。

Worker 不应复制 RGBWSV 内容自检逻辑，也不得实现第二套 TIFF/package publisher。

### 5.3 slicer_module / WorkerClient

模块是**进程所有权和第二保险责任方**：

- 在启动前按归一化 `packageDir` 取得目标级作业租约；
- 创建 job root、request 和 cancellation marker；
- 管理 Job Object、超时、协作取消宽限期和强制终止；
- 等待 Worker 完全退出并关闭 Job Object；
- 使用同一共享组件重复幂等恢复/清理；
- 只有确认无 owned staging/backup 残留后才发布终态；
- 清理失败统一映射 `PM-SLICER-OUTPUT-0050` 并保留诊断路径。

模块不写 TIFF、不验证材料闭环、不在 Worker 存活时删除 staging，也不代替崩溃 Worker 发布 staging。

## 6. 文件所有权冻结

14D-05 实施时允许修改/新增的文件范围如下。超出范围必须先回到准备审计，不得顺手修改 UI、
SPI ABI 或几何算法。

| 所有者 | 文件范围 | 允许职责 |
|---|---|---|
| 共享安全组件 | `src/slicer_core/api/artifacts/PackageArtifactSafety.h/.cpp` | 目标归一化、owned 名称生成/解析、父目录 containment、临时后缀识别、幂等恢复计划；不得依赖 engine |
| Writer | `src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.h/.cpp` | 传入 job/attempt 身份、staging 自检、发布事务、即时回滚、结果证据 |
| Worker | `apps/slicer_worker/WorkerApplication.*`、可新增 `WorkerArtifactRecovery.*` | request 身份映射、启动/退出第一轮清理；不另造 publisher |
| 模块 | `src/slicer_module/WorkerClient.h/.cpp`、可新增 `WorkerArtifactCleanup.*` | 目标租约、进程退出后第二轮清理和终态判定 |
| 查询/验证 | RGBWSV package query、RIP Reader、strict validator 对应实现 | 拒绝 staging/backup/tmp/bak 目标 |
| 单元测试 | `tests/unit/` 中 14D-05 专用测试及既有 Writer/WorkerClient 测试 | 状态机、路径安全、旧包保护和并发负例 |
| 集成测试 | `tests/integration/` 或 Stage 14 现有 Worker 合同测试目录 | Worker 强杀、超时、双清理和 C-SPI-09 证据 |

说明：`DEV_14` 目标树曾预留 `apps/slicer_worker/StagingPublisher.*`。基于当前代码事实，发布已经
由 RGBWSV Writer 承担。14D-05 冻结为“Worker 只编排恢复、Writer 唯一发布”，避免形成两套发布
实现。若后续必须移动发布所有权，应另建架构修订，不得在本卡中静默迁移。

明确禁止修改：

- `include/slicer_module_api.h` 的 SPI v1、11 个导出和 ABI；
- RGBWSV 通道顺序、`uint8`、`black_is_print`、`0=打印/255=不打印`；
- `apps/slicer_debug_ui/**`；
- 几何、纹理、支撑和材料策略；
- TASKS/REPORT/README 的完成状态，直到实施与验收真实通过。

## 7. 依赖与后续关系

### 7.1 已满足依赖

- 14D-01 Worker 壳、文件合同发现和退出码框架；
- 14D-02 模块侧 WorkerClient 进程树、超时和文本协议基础；
- 14D-03 进度/耗时协议基础；
- 14D-04A 核心取消 token、进程内 staging 清理和正常字节不变性；
- 14B-04 Facade 生产切片绑定。

### 7.2 当前实施依赖

- 14D-08-R2-02 已提供 `slice.rgbwsv` executor 与真实 Facade 执行，但生产 Worker 尚未注册；
- 当前请求 `jobId` 尚未贯穿到 `RgbwsvProductionPackageWriteRequest`，归 R2；
- WorkerClient 尚无 `packageDir`/作业身份和退出后清理输入；
- 同目标并发、临时路径拒绝和崩溃恢复规则尚无代码门禁。

### 7.3 被本任务阻塞的后续项

- 14D-04B Worker 真实取消与残留收口；
- 14D-07 引擎一致性套件的安全发布前提；
- 14E-01 的 M-MVP 前置；
- `14C-06 + 14D-05 = M-MVP-CANDIDATE`，因此本准备门未关闭时不得宣称候选完成。

## 8. 验收命令冻结

以下命令是**14D-05 实施后的验收命令**。本准备审计没有运行这些尚不存在或依赖 Worker
执行入口的测试，不得将其写成 PASS。

```powershell
cmake --preset slicesoft-vs-debug
cmake --build --preset slicesoft-vs-debug-build --target slicer_worker slicer_module rgbwsv_package_writer_unit_tests worker_client_unit_tests

cmake --preset slicesoft-vs-release
cmake --build --preset slicesoft-vs-release-build --target slicer_worker slicer_module rgbwsv_package_writer_unit_tests worker_client_unit_tests

python tests/contracts/ValidateFileContract.py
python tests/contracts/ValidateCancelContract.py

ctest --preset slicesoft-vs-debug-test -R "14d05|rgbwsv_package_writer|worker_client|package_query|rip_reader" --output-on-failure
ctest --preset slicesoft-vs-release-test -R "14d05|rgbwsv_package_writer|worker_client|package_query|rip_reader" --output-on-failure

powershell -ExecutionPolicy Bypass -File scripts/run_quick_ci.ps1
```

若仓库实际 preset 名称与上述冻结入口不同，实施卡可以只修正文档中的命令名称，不得缩减测试
语义。C-SPI-09 必须通过真实 DLL -> Worker -> Writer 链路执行，不接受直接调用 Writer 的替代证据。

最终证据至少包含：

- Debug/Release 构建与测试；
- 正常发布后的包可被严格 validator 和 RIP Reader 接受；
- 取消、超时、强杀和模拟崩溃后无 owned staging/backup；
- 已有有效包在所有失败路径中保持字节不变；
- 清理失败返回稳定 `PM-SLICER-OUTPUT-0050`；
- 同目标并发第二作业在写包前稳定失败；
- C-SPI-09 真实链路 PASS。

## 9. 必须覆盖的负例

| 编号 | 负例 | 预期 |
|---|---|---|
| N01 | staging 写层中途取消 | 删除本次 staging；旧包不变；未发布 |
| N02 | TIFF 写入后、manifest 前取消 | 同 N01 |
| N03 | staging 自检失败 | 不移动旧目标；删除本次 staging |
| N04 | 旧目标 rename 到 backup 后，新包 rename 前抛错 | 立即恢复 backup；当前作业失败 |
| N05 | 在 N04 窗口强杀 Worker | Worker/模块恢复唯一 owned backup；无 staging 残留 |
| N06 | Worker 未处理异常退出 | 模块等待退出后执行第二轮清理；不发布 staging |
| N07 | 超时后宽限期结束并强杀 | 退出码/错误码符合合同；清理完成后才终态 |
| N08 | staging/backup 删除被拒绝 | 返回 `PM-SLICER-OUTPUT-0050`；不得报告 Cancelled/success |
| N09 | packageDir 为相对路径、含 `..` 逃逸或跨卷 | fail-closed；不创建临时目录 |
| N10 | 临时候选为 symlink/junction/reparse point | fail-closed；不得跟随删除目标 |
| N11 | 相邻目录名为 `package2.staging.*` | 不得被 `package` 作业误删 |
| N12 | 两个作业同时写同一规范化 packageDir | 第二作业在写包前返回 `PM-SLICER-RESOURCE-0041` |
| N13 | 两个作业写不同 packageDir | 允许并发，互不清理 |
| N14 | target 存在且有未知作业 backup/staging | 不删除未知产物；输出诊断并失败 |
| N15 | target 缺失且存在多个 backup | 不猜测恢复对象；`OUTPUT-0050` |
| N16 | 查询/RIP Reader 输入 staging、backup、tmp、bak | 明确拒绝，不读取为有效包 |
| N17 | 磁盘空间不足或输出目录不可写 | 旧包不变；稳定 `OUTPUT-0050` |
| N18 | 正常路径无取消 | 包文件集合、字节、manifest 和 RGBWSV 协议与基线一致 |

## 10. 非目标

- 不修改切片算法、材料闭环、纹理、支撑或 TIFF 协议；
- 不新增 UI、进度样式或用户配置；
- 不处理断电级持久性和文件系统损坏恢复；
- 不在本卡完成 14D-08 的 full preflight/repair；
- 不允许模块直接写生产包；
- 不允许把 copy-and-delete 当作原子发布降级；
- 不清理本仓库以外的用户目录或无法证明所有权的历史产物。

## 11. 准备门复审清单

将本文件的状态从 BLOCKED 改为 PASS 前，必须逐项提供代码或合同证据：

- [x] U1 作业级临时产物身份已贯穿；
- [x] U2 Worker 最小 `slice.rgbwsv` 入口已可执行；
- [x] U3 Worker/模块共享路径安全和恢复组件已落地；
- [x] U4 同目标并发策略已落地并有测试；
- [ ] U5 崩溃窗口恢复状态机已落地并有强杀测试；
- [x] U6 临时路径读取拒绝已落地；
- [x] Debug/Release R3 定向负例能够运行，而不是仅有测试名称；
- [ ] C-SPI-09 可以通过真实链路执行。

以上项目是 R2..R4 的**完成门**，不是再次阻断已冻结实施方案的准备门。当前结论为：

```text
PREPARATION_GATE=PASS_WITH_SPLIT
IMPLEMENTATION=R1_R2_R3_COMPLETE_R4_PENDING
ACCEPTANCE=R3_DIRECTED_PASS_R4_NOT_RUN
```

## 12. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-06 | v1.0 | 首次审计，因 Worker 请求和安全发布基础缺失判定 BLOCKED |
| 2026-08-06 | v1.1 | 基于 14D-08-R1/R2/R3 与 F1 复审，冻结 R1..R4 实施拆分；R1 共享产物身份与恢复组件完成，准备门改为 PASS_WITH_SPLIT |
| 2026-08-06 | v1.2 | 完成 R2：`jobId`/派生 `attemptId` 贯穿 SliceFacade、场景生产服务与 Writer；Writer 使用精确 owned staging/backup、目标级租约、发布后严格复验和无残留证据，同目标并发在写包前 fail-closed；R3/R4 继续待实施 |
| 2026-08-06 | v1.3 | 完成 R3：生产 Worker 注册 `slice.rgbwsv`；Worker 起止与模块进程退出后使用共享 owned 恢复；PackageQuery/RIP 拒绝临时目录，Writer 使用私有严格验证；Debug/Release 定向门禁通过，R4 继续待实施 |
