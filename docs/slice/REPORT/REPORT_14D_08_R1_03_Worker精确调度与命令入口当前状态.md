# REPORT_14D-08-R1-03 Worker 精确调度与命令入口当前状态

> 更新日期：2026-08-06
>
> 任务状态：`COMPLETE`
>
> R1 状态：`COMPLETE`
>
> 下一阶段：`14D-08-R2 PREPARATION_REQUIRED`

## 1. 本轮目标

将 R1-01 的严格请求和不可变身份、R1-02 的身份闭合结果与原子发布接入唯一 Worker
运行时。调度只接受冻结的三项重能力，生产进程没有真实 executor 时必须显式失败，禁止
测试替身、空包或进程内 fallback 伪造成功。

## 2. 已完成内容

1. 新增 `IWorkerCapabilityExecutor` 与无身份权限的业务执行结果，executor 不能改写
   jobId、correlationId 或 capability。
2. 新增 `WorkerJobDispatcher`：
   - 只允许 `slice.rgbwsv`、`geometry.preflight.full`、`geometry.repair` 精确注册；
   - 拒绝未知、空指针和重复注册；
   - 调度前检查 `cancel.requested`；
   - 缺 executor 或 executor 异常稳定映射为显式失败。
3. 新增共享 `WorkerJobRuntime`，统一执行 parse -> dispatch -> result 原子发布；无法建立
   可信身份时不猜测结果路径，可信身份建立后写入身份闭合结果。
4. `WorkerApplication::HandleSpiRequest()` 已接入共享 runtime，不包含任何切片业务分支。
5. production registry 保持为空；合法请求当前返回 `PM-SLICER-INTERNAL-0099` / exit 1，
   同时发布完整 `result.json`，不创建 package。
6. 新增生产二进制边界门禁，检查测试 executor 字符串不进入 Worker，并执行真实合法请求
   证明没有伪成功、stdout 污染或 package 残留。

## 3. 验证结果

Debug 与 Release 定向套件均通过：

```text
stage14d08_r1_worker_runtime_tests                   PASS
stage14d08_r1_result_writer_tests                    PASS
stage14d08_r1_dispatcher_tests                       PASS
slicer_stage14d08_r1_worker_runtime_boundary_test    PASS
slicer_stage14d08_r1_no_fake_worker_executor_test    PASS
stage14d03_worker_contract_unit_tests                PASS
slicer_stage14d01_worker_shell_contract_test         PASS
file_contract_v1_test                                PASS
```

两套配置均为 `8/8 PASS`。额外通过 file contract、target graph、source-size guard 和
`git diff --check`；source-size guard 只报告既有警告，没有新增失败。

## 4. 边界与后续

- 本任务没有安装任何生产 executor，没有生成生产 package。
- 未修改 SPI v1、11 个导出、15 项能力、RGBWSV/TIFF 协议或材料策略。
- R1 只完成共享 Worker 执行基础，不代表三项重能力已经可用。
- `14D-08-R2` 仍需先冻结 scene/profile 物化、路径基准、sceneHash、package 所有权、
  full preflight 顺序和唯一生产 Facade 复用规则。
- 14D-05/06/07、14D-04B、14C-06B 与父任务 14D-08 继续保持 BLOCKED。

```text
14D_08_R1_03_STATUS=COMPLETE
14D_08_R1_STATUS=COMPLETE
14D_08_R2_PREPARATION_GATE=BLOCKED
14D_08_PARENT_GATE=BLOCKED
```
