# REPORT_14D-03 文件合同协商当前状态

> 日期：2026-08-06
> 状态：`COMPLETE`

## 1. 已完成范围

- `slicer_worker.exe --contract-info` 输出单行 UTF-8 `file_contract_v1` JSON；
- 模块侧复用 14D-02 `WorkerClient` 执行发现，不要求作业终态进度；
- major 必须相同，worker minor 必须不低于模块要求；
- `p0.rgbwsv.2`、所需 Worker 能力、能力枚举与唯一性均严格校验；
- 普通诊断只允许进入 stderr，stdout 不是单一 JSON 对象时拒绝；
- 未实现 `--spi-request`，该执行链路仍归 14D-08。

## 2. 验证结果

Debug 与 Release 均通过：

- `slicer_stage14d01_worker_shell_contract_test`；
- `stage14d02_worker_client_unit_tests`；
- `stage14d03_worker_contract_unit_tests`；
- 合同正例、major 篡改、minor 过低/向后兼容、非法 JSON、缺少生产协议、
  缺少或越界能力、重复能力和日志通道边界。

## 3. 冻结边界

- SPI 仍为 v1，导出函数仍为 11 个；
- Worker 合同版本为 `major=1, minor=0`；
- 生产协议仍为 `p0.rgbwsv.2`；
- 本卡不改变 TIFF、RGBWSV、UI 或 Worker 作业执行语义。

## 4. 后续

14D-03 已解除 14D-07 的合同协商前置；14D-07 仍需等待 14D-05 的安全发布。
当前主路径继续完成 14C-04，同步 DLL 能力接线完成后再串行处理 14C-05。
