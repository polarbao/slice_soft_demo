# REPORT_14C-06B SPI Worker 生命周期一致性当前状态

> 更新时间：2026-08-06
> 对应任务：`14C-06B`
> 状态：`COMPLETE / 14C-06 COMPLETE / C-SPI-01..18 PASS`

## 1. 结论

14C-06B 已通过冻结的 11 个公开 C ABI 导出验证真实 Worker 生命周期，
关闭 14C-06A 保留的 C-SPI-08、09、13 以及运行中 C-SPI-15。结合 14C-06A
的模块本地证据，原任务 `14C-06` 已全量完成。

## 2. 实现内容

- 为 `test_spi_conformance` 增加真实 Worker 生命周期一致性夹具；
- 先发布一个可严格读取的基线 RGBWSV Package，再对同一目标执行取消作业；
- 使用 Worker 真实阶段进度作为取消触发条件，不使用伪造或睡眠代替的 running 状态；
- 公开 SPI 作业操作仍全部通过运行时装载的 `slicer_module` 执行。

## 3. C-SPI 关闭证据

| 检查 | 结果 | 证据 |
|---|---|---|
| C-SPI-08 | PASS | Worker 活动期取消后在 2000 ms 内进入 `cancelled`，结果码为 `PM-SLICER-CANCELLED-0070` |
| C-SPI-09 | PASS | 旧 `manifest.json` 字节不变，owned staging/backup/lease 均不存在，基线包仍可严格读取 |
| C-SPI-13 | PASS | queued/running/cancelling 期间 `pm_result` 稳定返回 `PM_ERR_INVALID_STATE` |
| C-SPI-15 | PASS | 运行中、重复和终态后 `pm_cancel` 均幂等返回 `PM_OK` |

## 4. 实际验证

以下验证已在 2026-08-06 当前工作树实际运行：

- Debug Stage 14C 定向 CTest：`11/11` PASS，连续 3 次通过；
- Release Stage 14C 定向 CTest：`11/11` PASS，连续 3 次通过；
- `ValidateCapabilityDtos.py`：PASS；
- `ValidateThreeLaneContract.py`：PASS；
- `ValidateSourceSizeGuard.py --self-test`：PASS；
- `ValidateSourceSizeGuard.py`：PASS，仅报告 34 条既有全树告警；
- `git diff --check`：PASS，仅有 LF/CRLF 转换提示。

## 5. 边界与影响

- `PM_SPI_VERSION=1`、11 个导出和 15 项能力不变；
- `slicer_module` 仍只链接 `slicer_base`，重能力仍只在 `slicer_worker` 执行；
- 测试主机链接 `slicer_engine` 仅用于构造确定性 scene/Profile fixture，不绕过公开 ABI 调用模块；
- 未修改 `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`、TIFF 或现有 Qt UI；
- `14C-06 + 14D-05` 已满足 `M-MVP-CANDIDATE`，但仅解锁 14E-01，不代表 14E 已完成。
