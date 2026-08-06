# REPORT_14C-06A SPI 本地一致性当前状态

> 更新时间：2026-08-06
> 对应任务：`14C-06A`
> 状态：`COMPLETE / 14C-06 PARTIAL / WAITING_FOR_14C-06B`

> 后续闭环：14C-06B 已于 2026-08-06 关闭真实 Worker 生命周期项；
> 本报告保留 06A 完成时的历史边界，当前总状态见
> `REPORT_14C_06B_SPI_Worker生命周期一致性当前状态.md`。

## 1. 结论

已建立独立的 `test_spi_conformance`。该程序不链接 `slicer_module` 导入库，而是在运行时
装载指定 DLL，并且只通过 `print_module_spi.h` 冻结的 11 个公开 C ABI 导出执行模块本地
一致性验证。

本卡关闭不依赖真实 Worker 生命周期的检查；`C-SPI-08/09/13` 在程序输出和
`pm_self_test` 报告中均明确标记为 `BLOCKED_BY_WORKER_GATE`。因此本卡完成不等于原
`14C-06` 全绿，也不解锁 `M-MVP-CANDIDATE`。

## 2. 实现内容

### 2.1 无持久化副作用自检

- 新增 `ModuleSelfTest`，返回稳定的 `slicesoft.module_self_test.1` JSON；
- `pm_self_test` 复用唯一 `WriteOut()` 三态实现；
- 自检不启动 Worker、不创建业务线程、不读写模型、不创建日志和输出目录；
- 自检报告将 Worker 生命周期检查列为 deferred，不以模块本地 PASS 冒充 Worker PASS。

### 2.2 独立一致性程序

- 运行时使用 `LoadLibraryW` / `GetProcAddress` 装载 11 个公开导出；
- 直接检查已装载 PE 的导出表和导入表，验证精确 11 导出及 Qt/PrintSDK 依赖红线；
- 通过公开 ABI 验证模块自述、运行时匹配、句柄生命周期、缓冲三态、同步作业闭环、
  错误码、12 类非法请求、空句柄、重复取消和无副作用自检；
- Debug 与 Release 使用同一目标、同一断言集合，不通过私有 Adapter 构造成功证据。

## 3. C-SPI 状态

| 检查 | 14C-06A 结果 | 说明 |
|---|---|---|
| C-SPI-01..04 | PASS | SPI v1、模块自述、运行时匹配、100 次生命周期及内存门禁 |
| C-SPI-05a/b/c | PASS | 探测、差 1 哨兵不变、足量写入和 NUL |
| C-SPI-06/07 | PASS | 同步能力提交、终态轮询、结果读取和单调进度 |
| C-SPI-08 | `BLOCKED_BY_WORKER_GATE` | 等待真实 Worker 取消延迟证据 |
| C-SPI-09 | `BLOCKED_BY_WORKER_GATE` | 等待 Worker staging 清理与旧包保护证据 |
| C-SPI-10..12 | PASS | 稳定错误码、12 类非法请求、空句柄 fail-closed |
| C-SPI-13 | `BLOCKED_BY_WORKER_GATE` | 同步轻能力无诚实 running 状态，不伪造该状态 |
| C-SPI-14 | PASS | 销毁 module 时自动退役未 release 的同步 job |
| C-SPI-15 | PASS_LOCAL | 终态重复取消幂等；Worker 运行中变体保留给 06B |
| C-SPI-16/17 | PASS | PE 导出表精确 11 项，导入表无 Qt5/PrintSDK |
| C-SPI-18 | PASS | 合法稳定 JSON，沙箱文件集合前后不变 |

## 4. 实际验证

以下验证已在 2026-08-06 当前工作树实际运行：

- Debug 定向构建：`test_spi_conformance`、`slicer_module`、14C-02/03/05/07 目标，PASS；
- Release 定向构建：同上，PASS；
- Debug 定向 CTest：`stage14c06|stage14c0[1-7]`，`11/11` PASS；
- Release 定向 CTest：`stage14c06|stage14c0[1-7]`，`11/11` PASS；
- `ValidateCapabilityDtos.py`：PASS；
- `ValidateThreeLaneContract.py`：PASS；
- `ValidateSourceSizeGuard.py --self-test`：PASS；
- `ValidateSourceSizeGuard.py`：PASS，报告 32 条既有全树告警，本卡未新增违规；
- `git diff --check`：PASS，仅输出既有 LF/CRLF 转换提示。

## 5. 冻结边界

- `PM_SPI_VERSION=1` 不变；
- 导出集合仍为 11 个，能力集合仍为 15 项；
- 未修改 ViewData v1.2、三车道合同、file contract 或 Worker 语义；
- 未修改 `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 或 TIFF；
- 未把 14C-06A 记为 14C-06 COMPLETE；完整出口仍等待
  `14D-04B + 14D-05 + 14D-08 + 14C-06B`。
