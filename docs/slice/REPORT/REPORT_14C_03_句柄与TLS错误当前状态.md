# REPORT_14C-03 句柄与 TLS 错误当前状态

> 更新时间：2026-08-06
> 任务：Stage 14C-03
> 状态：`COMPLETE / HANDLE_TLS_FOUNDATION_PASS`

## 1. 任务结论

已建立 `HandleRegistry`、最小 job 生命周期和线程局部 `ErrorApi`，并接入当前 C ABI 外壳：

- module/job 不透明句柄在注册表中统一创建、验证、归属和退役；
- 未知、空、陈旧和跨 module 句柄不在调用方地址上解引用；
- module 销毁会收拢当前登记的 job，句柄 token 不复用，避免陈旧地址重新生效；
- job 支持最小状态和幂等 cancel，为后续同步能力与 Worker 作业接线提供基础；
- `pm_create` / `pm_destroy` 已接入真实 module 句柄；
- `pm_last_error` 已使用 TLS JSON 和 14C-02 唯一 `WriteOut()`；
- 未接线的 `pm_submit` 继续 fail-closed，不伪造能力执行成功。

## 2. 验证证据

- Debug / Release `slicer_module`、注册表单测和 ABI 单测构建：PASS；
- Debug / Release CTest 2/2：PASS；
- `pm_create` / `pm_destroy` 预热后循环 100 次，进程 private bytes 增长小于 1 MiB：PASS；
- module-job 归属、陈旧/伪造指针、module 收拢 job、并发注册、最小状态和重复 cancel：PASS；
- TLS 线程隔离、覆盖、成功不清除、JSON 转义和超长错误降级：PASS；
- `pm_last_error` 探测/写入走共享缓冲协议：PASS；
- Debug / Release `dumpbin`：精确 11 导出，不含 Qt 或 PrintSDK 依赖；
- 14C-01 静态合同和 source-size guard self-test：PASS。

完整 C-SPI-06..15 跨 Worker/conformance 验收仍由 14C-06 与 14D 完成；本卡不把尚未存在的
真实 job 执行、取消等待或 staging 清理写成 PASS。

## 3. 后续接线约束

- 14C-04 的同步能力必须通过 `HandleRegistry::CreateJob()` 创建终态 job；
- 14D Worker 作业销毁时必须先协作取消并等待进程退出，再退役注册表句柄；
- `Cancelling` 不得在真实 Worker 退出前伪装为 `Cancelled`；
- 不允许新增第二套 TLS 错误槽或缓冲写出实现。

## 4. 冻结边界

- SPI v1、11 导出、15 项能力和 ViewData v1.2 不变；
- `p0.rgbwsv.2`、RGBWSV、8-bit、`black_is_print` 不变；
- DLL 仍不链接 Qt、PrintSDK 或 `slicer_engine`。
