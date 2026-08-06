# DOC_PREP_14C-03 句柄与 TLS 错误实施准备

> 日期：2026-08-06
> 状态：`PREPARATION_GATE = PASS`
> 对应任务：`14C-03`

## 1. 目标与边界

本卡建立 `pm_module_t` / `pm_job_t` 的最小生命周期基础设施和线程局部错误槽，关闭
`C-SPI-04/12/13/14/15` 所需的底层语义，但不提前实现同步能力、Worker 作业或完整
conformance 套件。

## 2. 句柄合同

- 注册表是进程内唯一句柄权威，不通过未知裸指针解引用判断合法性；
- `nullptr`、已释放句柄、跨 module 的 job 均须稳定拒绝；
- module 与 job 保持显式所有关系；销毁 module 时先收拢其全部 job；
- `pm_destroy(nullptr)` 与 `pm_release(nullptr)` 保持合法空操作；
- 后续活动作业的 cancel/join 由 14C/14D 接线实现，本卡只提供可挂接的状态和收拢点；
- 为避免地址复用把陈旧指针误认成新句柄，句柄 identity 在进程生命周期内不得被重新分配给
  另一个活对象；同时 100 次 create/destroy 的保留开销必须低于 C-SPI-04 的 1 MiB 门槛。

最小 job 状态为 `queued/running/cancelling/succeeded/failed/cancelled`。本卡的注册表允许
设置和读取状态，`pm_result` 的非终结态拒绝及重复 cancel 幂等由导出接线验证。

## 3. TLS 错误合同

错误槽保存稳定 JSON：

```json
{ "code": "PM-SLICER-...", "message": "...", "detail": "..." }
```

- 每个线程独立；
- 下一次失败覆盖当前线程错误；
- 成功调用不清除；
- JSON 字符串必须正确转义引号、反斜杠和控制字符；
- `pm_last_error` 后续必须复用 14C-02 `WriteOut()`，本卡不复制缓冲协议。

## 4. 文件所有权与出口

```text
src/slicer_module/HandleRegistry.h/.cpp
src/slicer_module/ErrorApi.h/.cpp
tests/stage14c_03/HandleRegistryTests.cpp
```

出口要求：生命周期、陈旧句柄、module-job 归属、销毁收拢和 TLS 线程隔离单测通过；无 Qt、
无 engine 依赖；不改变公共 ABI 声明和导出数量。
