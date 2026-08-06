# DOC_PREP_14D-04 取消令牌贯穿实施准备

> 日期：2026-08-06
> 状态：`PREPARATION_GATE = PASS / IMPLEMENTATION COMPLETE`
> 对应任务：`14D-04A / 14D-04B`

## 1. 拆分理由

14B-04 已建立 `ICancelToken` 和 Facade 取消合同。14D-04A 已闭合生产深循环检查；14D-05、
14D-06 与 14D-08 已提供安全发布、公共 SPI 路由和真实 Worker 执行，因此 14D-04B 现已完成
Worker 端到端“2 秒且无残留”验收。原 14D-04 受控拆为：

- `14D-04A`：核心 token 贯穿、阶段/逐层/长循环检查和直接引擎门禁；
- `14D-04B`：真实 Worker 活动期取消、2 秒上限、生命周期幂等和无残留验收，已完成。

两卡合计才代表原 14D-04 COMPLETE。

## 2. 冻结语义

- `Cancelling != Cancelled`；只有执行实体真实退出且清理完成才可进入 Cancelled；
- 取消码固定 `PM-SLICER-CANCELLED-0070`，Worker 退出码固定 8；
- 最大协作延迟 2000ms；单个长循环必须分段检查，不能只在函数入口检查；
- 取消不允许发布目标 package，不改变已有成功包；
- 正常路径输出必须与取消接入前字节和报告语义一致；
- token 为同步期非拥有引用，不得缓存悬垂指针或用异常跨越 C ABI。

## 3. 14D-04A 插入点

token 必须沿以下生产链逐级显式传递：

```text
SliceProductionRunner
  -> ProductionSliceFacadeFactory
  -> MultiModelProductionRequest
  -> LegacySceneLayerAdapterRequest
  -> SliceRunOptions
```

至少检查：模型加载、场景准入、实例循环、grid/mask、纹理、支撑、逐层合成、长像素循环、
每层 TIFF 前后、preview、报告和发布前。进度回调抛异常不得作为唯一取消机制。

文件所有权以 `src/slicer_core/engine`、`pipeline`、`output` 的最窄请求/执行文件为限；不得修改
`WorkerClient*`、`WorkerContract*`、SPI 导出或 file-contract Schema。

## 4. 14D-04A 验收

- 对固定测试 token 在每个阶段注入取消，终止状态和稳定错误码正确；
- 逐层及长循环取消检查有机器门禁；
- 直接引擎样本的取消观察延迟不超过 2000ms；
- 正常路径既有生产回归、TIFF/RGBWSV 和报告回归全绿；
- Debug/Release 均通过，源文件行数与 base/engine 依赖门禁通过。

## 5. 14D-04B 后置条件

前置必须为 `14D-04A + 14D-05 + 14D-08`。届时通过 `cancel.requested` 构造真实 Worker
作业，覆盖 queued/running/cancelling、重复取消、终态后取消、超时 Job Object 兜底、
Worker 退出码 8、结果码和 staging 清理。任何残留目录或提前报告 Cancelled 均判失败。

## 6. 后续准备结论

14D-04A 可与 14C-05 并行开发，二者源码所有权不重叠；CMake、任务表和总状态由主代理串行
集成。14D-04B 通过公开 C SPI 等待真实 Worker 进入生产进度后取消，验证：

- 活动作业调用 `pm_result` 返回 `PM_ERR_INVALID_STATE`；
- `pm_cancel` 后状态为 `cancelling` 或已完成的 `cancelled`；
- 重复取消、终态后取消均为返回 `PM_OK` 的幂等操作；
- 从取消受理到 `cancelled` 不超过 2000ms，结果码固定为 `PM-SLICER-CANCELLED-0070`；
- 目标包未发布，owned staging/backup/lease 均不存在；
- Debug/Release 真实 DLL -> Worker 定向测试通过。

因此原 14D-04 标记 COMPLETE。queued 取消由 14D-05-R4-B 覆盖，超时强杀与旧包保护由
14D-05-R4-A 覆盖；清理失败负例继续由 14D-05 稳定 `PM-SLICER-OUTPUT-0050` 门禁承担。

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-06 | v1.0 | 冻结 14D-04A/04B 拆分、取消语义与后置条件 |
| 2026-08-06 | v1.1 | 完成 14D-04B：公开 SPI 活动期真实 Worker 取消、2000ms 上限、生命周期幂等和无 owned 残留在 Debug/Release 下通过 |
