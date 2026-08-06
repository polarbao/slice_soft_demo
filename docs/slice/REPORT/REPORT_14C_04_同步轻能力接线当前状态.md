# REPORT_14C-04 同步轻能力接线当前状态

> 日期：2026-08-06
> 状态：`COMPLETE`

## 1. 已完成范围

- `pm_submit` 已接入 13 项进程内轻能力，并为每次受理创建不可变终态 job；
- 首次 `pm_poll` 返回 `succeeded|failed`、100% 和稳定能力标识；
- `pm_result`、`pm_poll` 与 `pm_last_error` 统一复用 14C-02 缓冲三态协议；
- `geometry.preflight` 保持冻结能力 ID，仅 `mode=fast` 进程内执行，`mode=full` 显式交由 Worker；
- `scene.get_viewdata` 复用 14B-03A `TexturedSceneViewDataProvider`，不另造 DTO、不允许纹理失败后灰模成功；
- module/job 释放会同步清理能力上下文和终态结果。

## 2. 验证结果

Debug 与 Release 均通过：

- `ValidateSyncCapabilityWiring.py` 真实加载 `slicer_module.dll` 并调用 SPI；
- 13 项同步能力清单漂移、未知能力和 Worker 能力拒绝；
- fast/full preflight 承载边界；
- 模型导入/元数据/释放、场景操作/快照/ViewData/碰撞；
- ViewData top 纹理、three_d 网格/UV/appearance 与 blob 分块读取；
- 包查询负例 fail-closed、首次 poll 终态和二进制/JSON result；
- 14C-03 ABI 回归保持 SPI v1、精确 11 个导出和既有依赖边界。

## 3. 冻结边界

- 能力总数仍为 15，进程内同步承载为 13 项；
- 不存在 `geometry.preflight.fast` 新能力；
- `geometry.repair`、`slice.rgbwsv` 和 full preflight 仍属于 Worker；
- 本卡不修改 TIFF、RGBWSV、Profile、UI 或生产切片算法。

## 4. 后续

14C-04 已解除 14C-06 的同步能力前置。下一批按文件所有权并行审计并执行
14C-05 模块自述与 14D-04 深度取消；共享 CMake 和状态文档继续由主代理串行集成。
