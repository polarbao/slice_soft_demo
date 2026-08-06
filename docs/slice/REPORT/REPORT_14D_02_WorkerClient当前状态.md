# REPORT_14D-02 WorkerClient 当前状态

> 更新时间：2026-08-06
> 任务：Stage 14D-02
> 状态：`COMPLETE / WORKER_CLIENT_FOUNDATION_PASS`

## 1. 任务结论

DLL 侧已建立 Windows Worker 子进程基础设施：

- 使用 `CreateProcessW` 启动独立进程，不经 shell 拼接命令；
- stdout / stderr 独立管道读取，普通日志保留；
- `SLICE_PROGRESS` 与 `SLICE_TIMING` 保留行严格解析，非法保留行 fail-closed；
- 退出码映射到稳定类别和错误码，未知退出码归为 `internal`；
- 进度百分比、耗时和同阶段 current/total 执行单调检查；
- 进程加入 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` Job Object；
- 有限超时、协作取消标记、最长 2000 ms 宽限和进程树强制回收已落地；
- 客户端析构等待活动任务退出，不遗留本卡测试创建的子孙进程。

实现按职责拆为 `WorkerClient`、`WorkerProtocol` 和 `WorkerProcessWindows`，所有新增源文件均低于
500 行。模块继续只链接 `slicer_base`，未引入 Qt、PrintSDK 或 `slicer_engine`。

## 2. 验证证据

- Debug / Release `slicer_module` 与 `stage14d02_worker_client_unit_tests` 构建：PASS；
- Debug / Release WorkerClient CTest：PASS；
- Release WorkerClient 完整用例连续执行 3 次：PASS；
- 覆盖成功启动、UTF-8/含空格参数、stdout/stderr、退出码 1..8 与未知退出码：PASS；
- 覆盖合法进度/耗时、非法语法、进度回退和缺少终态进度：PASS；
- 覆盖协作取消、超时强制终止和子进程树回收：PASS；
- 14C-01 / 14D-01 静态合同、source-size guard、14C-03 ABI 回归，Debug / Release：PASS；
- Debug / Release `dumpbin`：仍精确导出 11 个 `pm_*`，依赖中不含 Qt 或 PrintSDK。

## 3. 未完成边界

本卡只证明“受控子进程后端可用”，以下能力不得写成已完成：

- `file_contract_v1` 的 `--contract-info` 协商：14D-03；
- 切片深层 `ICancelToken` 贯穿：14D-04；
- staging 自检、原子发布和残留清理：14D-05；
- `pm_submit` 到真实 Worker 作业接线及独立 `--spi-request`：14D-08 与后续集成卡；
- C-SPI-01..18 全量符合性：14C-06。

## 4. 下一批准备门禁

| 任务 | 依赖核验 | 文档/合同 | 准备结论 |
|---|---|---|---|
| 14C-04 同步轻能力 | 14C-02、14B-00/02/03/03A 均完成 | DTO v1.2、DEV_14 §5 已冻结 | `PASS` |
| 14C-05 模块自述 | 14C-01 完成 | SPI v1、11 导出、15 能力已冻结 | `PASS` |
| 14C-07 初始化红线 | 14C-01 完成 | DllMain/依赖红线及门禁已冻结 | `PASS` |
| 14D-03 文件合同协商 | 14A-03、14D-02 完成 | `file_contract_v1` Schema 与兼容规则已冻结 | `PASS` |

下一批可并行开发 `14C-04` 与 `14D-03`。`14C-05`、`14C-07` 虽已准备完成，但与
`14C-04` 共享 `Exports.cpp`、CMake 和 DLL 验收面，应在集成阶段串行提交，避免并发覆盖。

## 5. 冻结边界

- SPI v1、11 个导出、15 项能力不变；
- ViewData v1.2、top/three_d 真实纹理要求不变；
- `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 不变；
- 14E 仍受 M-MVP 门禁约束，本卡不提前启动 Qt 宿主。
