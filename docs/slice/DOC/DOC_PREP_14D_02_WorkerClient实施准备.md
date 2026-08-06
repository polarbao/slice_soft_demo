# DOC_PREP_14D-02 WorkerClient 实施准备

> 日期：2026-08-06
> 状态：`PREPARATION_GATE = PASS`
> 对应任务：`14D-02`

## 1. 目标与非目标

本卡在 DLL 侧建立 Windows Worker 子进程基础设施：启动、输出读取、进度解析、退出类别映射、
超时/取消和进程树回收。它不实现 14D-03 版本协商、不实现 14D-08 真实 request 执行，也不接入
`pm_submit`。

合同真源为 `contracts/file_contract_v1.md`、四份 `file_contract_v1.*.json`、
`contracts/slicer_cancel_contract.*` 和 `DEMO_14` 的 D14-D 用例。

## 2. 进程与资源合同

- Windows `CreateProcessW`，命令与参数分别转义，禁止拼接后交给 shell；
- stdout/stderr 使用独立匿名管道，父进程关闭不需要的继承端；
- 子进程创建后立即加入带 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` 的 Job Object；
- 所有 `HANDLE` 使用 RAII，成功、失败、超时和析构路径均回收进程、线程、管道和 Job Object；
- 正常完成不调用强杀；取消/超时先给协作宽限，宽限耗尽才终止 Job Object；
- 完成返回前必须等待完整进程树退出，禁止遗留僵尸/孤儿进程。

## 3. 输出与退出合同

保留普通日志；保留并严格解析：

```text
SLICE_PROGRESS phase=<token> current=<uint> total=<uint> percent=<0..100> elapsedMs=<fixed-3>
SLICE_TIMING engine=<token> ... totalMs=<fixed-3> workingSetBytes=<uint> peakWorkingSetBytes=<uint>
```

`current <= total`、百分比范围、同一作业的 percent/elapsedMs 单调。保留前缀但语法非法时，
结果标记为合同错误，不把它降级为普通日志。退出码按
`file_contract_v1.exit_codes.json` 映射；未知退出码归 `internal`。

## 4. 文件所有权、测试和出口

```text
src/slicer_module/WorkerClient.h/.cpp
src/slicer_module/WorkerProtocol.h/.cpp
src/slicer_module/WorkerProcessWindows.h/.cpp
tests/stage14d_02/WorkerClientTests.cpp
```

为满足新增源文件不超过 500 行的门禁，进程资源封装、保留行协议解析和客户端编排分文件落地；
三者仍属于同一个 `WorkerClient` 任务边界，不构成第二套后端。

测试至少覆盖：成功启动与捕获、非零退出映射、普通日志、合法/非法进度、进度回退、超时强制回收、
重复销毁安全。当前 Worker 外壳可作为非零退出 fixture；测试进程可自托管 helper 模式。

出口只证明“子进程后端基础设施可用”。版本协商、真实结果 schema、staging 发布和切片一致性仍为
后续卡，D14-D-01..12 不得因此整体标为 PASS。
