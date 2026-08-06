# DOC_PREP_14C-06 SPI 一致性套件实施准备

> 日期：2026-08-06
> 对应任务：`14C-06`
> 状态：`14C-06A COMPLETE / 14C-06B BLOCKED / CONTROLLED_SPLIT`

## 1. 结论

原任务要求一次性证明 `C-SPI-01..18` 全绿。当前同步轻能力、缓冲协议、句柄、模块自述和
ABI 基础已经存在，14C-06A 已实现无副作用 `pm_self_test` 和公开 C ABI 动态装载一致性程序；
但 Worker 三项重能力仍未接入真实执行入口。因此仍不能宣称 `14C-06 COMPLETE`。

为避免阻塞可验证的模块侧工作，本任务受控拆为：

- `14C-06A`：建立独立、可重复运行的模块一致性程序，关闭不依赖 Worker 真实运行态的检查；
- `14C-06B`：在 `14D-04B + 14D-05 + 14D-08` 完成后，补齐运行中取结果、真实取消、
  staging 清理和 Worker 终态检查；
- `14C-06A + 14C-06B` 全部通过后，原 `14C-06` 才能标记完成并参与
  `M-MVP-CANDIDATE` 判定。

## 2. 当前代码事实

### 2.1 已有可复用证据

| 检查 | 当前基础 |
|---|---|
| C-SPI-01..03 | `14C-05` 已提供 `pm_module_info`、双 Schema、Debug/Release runtime 与部署清单 |
| C-SPI-04 | `stage14c03_module_abi_tests` 已覆盖 100 次 create/destroy 和私有内存增量 |
| C-SPI-05a/b/c | `14C-02` 已冻结唯一 `WriteOut()` 三态实现 |
| C-SPI-06/07 | `14C-04` 同步能力可提交、首次 poll 即终态并读取 result |
| C-SPI-10..12、14/15 | 句柄、TLS 错误与部分非法请求/幂等行为已有基础测试 |
| C-SPI-16/17 | `14C-01/07` 已冻结并验证 11 导出、DllMain、初始化与依赖红线 |

### 2.2 尚未满足的真实缺口

1. 同步轻能力提交后立即终态，无法诚实构造 C-SPI-13 的 running 状态。
2. `geometry.repair`、`slice.rgbwsv` 和 `geometry.preflight.full` 仍 fail-closed，尚无真实
   Worker job 可用于 C-SPI-08/09/13。
3. `14D-08` 的共享执行基础已完成拆分准备，但真实切片映射及 full preflight/repair 适配器仍被阻断。
4. `14D-05` 的 Worker 级 staging、自检、原子发布和崩溃清理准备门仍为 BLOCKED。
5. `14D-04A` 只证明进程内协作取消；Worker 退出码 8、两秒上限与无残留属于 04B。

## 3. 受控拆分

### 3.1 14C-06A 模块本地一致性

前置：`14C-01..05 + 14C-07`。

允许实现：

- 新建独立 `test_spi_conformance` 或等价测试目标，通过动态装载/公开 C ABI 验证模块；
- 汇总并补强 C-SPI-01..07、10..12、14..18 中不依赖运行中 Worker 的部分；
- 实现无持久化副作用的 `pm_self_test`，输出合法、稳定、可通过缓冲三态读取的 JSON；
- 对 C-SPI-08/09/13 明确输出 `BLOCKED_BY_WORKER_GATE`，不得计为 PASS；
- Debug/Release 分别运行，校验 module.json 与实际 DLL 配置一致。

本卡完成后只能写：

```text
14C-06A=COMPLETE
14C-06=PARTIAL / WAITING_FOR_14C-06B
```

### 3.2 14C-06B Worker 生命周期一致性

前置：`14C-06A + 14D-04B + 14D-05 + 14D-08`。

必须用真实 Worker 重能力作业验证：

- C-SPI-08：取消请求后在 `cancelLatencyMs` 内进入 cancelled；
- C-SPI-09：取消后无 `.staging`、不覆盖既有有效包；
- C-SPI-13：queued/running/cancelling 时 `pm_result` 返回 `PM_ERR_INVALID_STATE`；
- C-SPI-15：运行中、重复和终态后取消均幂等；
- 进度、result、退出码与 file-contract 身份闭合。

## 4. 文件所有权

### 4.1 14C-06A

- `tests/stage14c_06/*`：独立一致性主程序、动态装载与 schema/副作用检查；
- `src/slicer_module/Exports.cpp`：仅接入 `pm_self_test`；
- `src/slicer_module/ModuleSelfTest.*`：无副作用自检实现；
- `CMakeLists.txt`：注册测试和 Debug/Release CTest；
- 状态报告、任务表：验收后由主执行者串行更新。

### 4.2 14C-06B

- Worker/SPI 端到端一致性夹具；
- 不重复实现 `14D-04B`、`14D-05` 或 `14D-08` 的业务服务。

## 5. 冻结边界

- SPI v1、11 个导出、15 项能力不变。
- 一致性测试必须经公开 C ABI，不得 include 私有 Adapter 后冒充宿主验证。
- `pm_self_test` 不创建输出目录、不启动 Worker、不加载模型、不写日志或持久文件。
- 模块自检只证明模块基础设施和合同可读性，不代替生产切片或 RIP 验证。
- 不能把 fail-closed 的 Worker 能力当成 C-SPI-06/08/09/13 的成功证据。
- M-MVP-CANDIDATE 仍要求原 `14C-06` 全绿和 `14D-05` 完成；06A 不单独解锁 UI。

## 6. 验收计划

### 6.1 14C-06A

```powershell
cmake --build build-slicesoft/main --config Debug --target test_spi_conformance slicer_module
cmake --build build-slicesoft/main --config Release --target test_spi_conformance slicer_module
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "stage14c06|stage14c0[1-7]"
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R "stage14c06|stage14c0[1-7]"
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
python scripts/ValidateSourceSizeGuard.py
git diff --check
```

### 6.2 14C-06B

在 14D 后置任务完成后，使用真实 Worker 请求运行 C-SPI-08/09/13/15，并同时校验：

- 取消观察延迟不超过 module info 的 `cancelLatencyMs`；
- Worker 退出码、result 状态和稳定错误码一致；
- staging、临时结果和子进程无残留；
- 既有成功包未被失败或取消作业覆盖。

## 7. 必测负例

- DLL 缺失、导出缺失、runtime/buildConfig 不匹配必须拒绝装载；
- 所有字符串出参覆盖探测、差 1、足量、负容量和哨兵不变；
- 12 类非法请求必须返回稳定错误，不崩溃；
- 空/伪造/跨 module 句柄必须 fail-closed；
- 自检前后工作目录文件集合不变；
- 取消前就报告 cancelled、取消后仍发布 package、或 running 时返回最终 result 均失败；
- 任何 Qt、PrintSDK、Engine 或额外导出进入 DLL 均失败。

## 8. 准备门结论

```text
14C-06A STATUS=COMPLETE
14C-06B PREPARATION_GATE=BLOCKED_BY_14D-04B_14D-05_14D-08
14C-06 OVERALL=BLOCKED_WITH_CONTROLLED_SPLIT
```

当前仍不存在理由提前宣称 C-SPI-01..18 全绿。06A 已关闭模块本地项；06B 必须等待真实
Worker 生产、运行态和取消链路完成。
