# REPORT_14D-01 Worker 外壳当前状态

> 更新时间：2026-08-05
> 任务：Stage 14D-01
> 状态：`COMPLETE / WORKER_SHELL_PASS`

## 1. 任务结论

已建立独立 `slicer_worker.exe` 目标和 `apps/slicer_worker/` 代码边界。Worker 目标链接
`slicer_engine`，与既有 CLI 使用同一 base/engine 生产构建轨道，但不复制 CLI 大文件，也不把
Qt 或 PrintSDK 引入 Worker。

本卡实现稳定的最小命令外壳：

- `--help` 返回 `0` 并明确当前能力边界；
- 无参数、未知参数、参数数量错误返回 `2`；
- `--contract-info` 和合法形态的 `--spi-request` 被识别，但在后续能力落地前稳定返回
  `1/not_implemented`；
- 相对 request 路径 fail-closed，不伪造合同 JSON、进度、结果或已完成作业。

## 2. 本卡边界

“由 CLI 演进”在 14D-01 表示建立独立进程目标、复用同一引擎依赖轨道和稳定参数入口，不表示
复制 `slicer_cli/main.cpp` 或提前实现完整文件合同。以下内容仍由后续原子任务关闭：

- 14D-02：DLL 侧 `WorkerClient`；
- 14D-03：真实 `--contract-info` 与 major/minor 协商；
- 14D-04：取消链路与 2 秒上限；
- 14D-05：staging 自检和原子发布；
- 14D-08：真实 `--spi-request` 作业执行与独立调试。

因此 D14-D-01..12 验收项仍为 `NOT RUN`，不得因本外壳可执行而提前写成 PASS。

## 3. 验证证据

- Debug / Release `slicer_worker` 构建：PASS；
- `ValidateStage14D01WorkerShell.py` 在 Debug / Release 独立运行：PASS；
- Debug / Release CTest：PASS；
- 无参数、未知参数、合同信息、相对/绝对 request 路径的重复执行结果稳定；
- 新增 C++ 文件均小于 500 行，未产生根目录编译产物。

## 4. 冻结边界

- `file_contract_v1`、退出码表和 request/result schema 不变；
- `PM_SPI_VERSION=1`、11 导出和 15 项能力不变；
- RGBWSV/TIFF 生产协议不变；
- 不把当前 `not_implemented` 当成合同协商成功或切片成功。
