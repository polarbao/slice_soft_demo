# REPORT_14D-08-R1-02 Worker 结果原子写入当前状态

> 更新日期：2026-08-06
>
> 任务状态：`COMPLETE`
>
> 准备门：`PASS`
>
> 下一任务：`14D-08-R1-03 capability 精确调度与命令入口接线`

## 1. 本轮目标

基于 R1-01 的可信请求身份，生成身份闭合的 `file_contract_v1` 结果，并通过同一作业目录的
`result.json.tmp -> result.json` 原子替换发布。该 Writer 只负责 Worker 结果合同，不负责
生产 package 发布、staging/backup 或算法执行。

## 2. 已完成内容

1. 新增 `WorkerResultEnvelope`：
   - `ok=true` 只允许 `PM-SLICER-OK-0000` 与非空 output；
   - `ok=false` 要求冻结的非成功错误码和非空 message；
   - jobId、correlationId、capability 只能复制 R1-01 的可信身份；
   - elapsed 必须有限且非负，engineVersion 必须非空；
   - cancelled 结果必须携带 `stagingRemoved=true/published=false`。
2. 新增稳定进程退出映射：0/1/2/3/4/5/6/7/8 与 `file_contract_v1.exit_codes.json`
   对齐；合同没有独立类别的 layout 错误按 unknown/internal 退出 1，result.code 仍为权威码。
3. 新增 `WorkerResultWriter`：
   - 只写身份所有的同目录 `result.json.tmp`；
   - flush/close 后使用 Windows `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)` 原子替换；
   - 写入或替换失败删除当前 tmp，并稳定映射 `PM-SLICER-OUTPUT-0050` / exit 6；
   - 不删除既有目标，不扫描其他 job，也不创建生产 package。
4. 新增正负例，覆盖成功、失败、取消、重复替换、非法结果、tmp 打开失败与 replace 失败。

## 3. 验证结果

Debug 与 Release 均通过：

```text
file_contract_v1_test                                PASS
slicer_stage14d08_r1_worker_runtime_boundary_test    PASS
stage14d08_r1_worker_runtime_tests                   PASS
stage14d08_r1_result_writer_tests                    PASS
```

两套配置均为 `4/4 PASS`；`slicer_worker`、parser 与 result writer 测试目标均通过 `/W4 /WX`。

## 4. 边界与后续

- 无可信身份时不得调用 Writer，也不得猜测 result 路径。
- 本任务未修改 `WorkerApplication`，未接入 dispatcher 或生产 executor。
- 本任务不替代 14D-05 的 package staging、安全发布和崩溃恢复。
- R1-03 的 parser、result、退出映射和无 executor fail-closed 条件已齐，准备门转 PASS。
- 父任务 14D-08 仍保持 BLOCKED。

```text
14D_08_R1_02_STATUS=COMPLETE
14D_08_R1_03_PREPARATION_GATE=PASS
14D_08_R1_03_STATUS=READY
14D_08_R1_STATUS=IN_PROGRESS
14D_08_PARENT_GATE=BLOCKED
```
