# REPORT HOSTFLOW H-F-09 宿主连续计时与 Worker 快照分离

> 状态：**COMPLETE**
> 日期：2026-08-17
> 对应任务：`H-F-09`

## 1. 根因

旧界面把 Worker 进度快照中的 `elapsedMs` 直接作为处理中计时显示。宿主虽然每 100 ms 轮询一次，
但 Worker 只在发布新阶段/新进度快照时更新该字段，因此界面会先长时间没有数字，第一次显示时已经
是十几秒；下一份快照到达后又直接跳到二十多秒。这不是计时器从十几秒才启动，而是把低频 Worker
快照误当成连续宿主时钟。

## 2. 修复

1. 作业提交成功后立即启动宿主 `QElapsedTimer`，处理中每次轮询都刷新连续墙钟；
2. Worker `elapsedMs` 保留为独立诊断快照，不再覆盖宿主连续计时；
3. 阶段边界估算使用宿主墙钟，避免 Worker 快照发布频率造成阶段耗时跳变；
4. Worker 没有提供真实快照时显示“未提供”，不再用宿主时间伪装 Worker 总耗时；
5. 成功、失败、取消和第二次提交都会结束或重置本次宿主时钟，第二次作业重新从 0 计时。

## 3. 验证

Release 定向 CTest：

```text
hostflow_hb05_slice_settings       PASS
hostflow_hb05_settings_ui_smoke    PASS
hostflow_hb06_slice_job            PASS
hostflow_hb06_job_ui_smoke         PASS
Total: 4/4 PASS
```

控制器测试覆盖宿主耗时单调递增、终结态保留和第二次作业重新计时；UI Smoke 覆盖处理中连续计时、
Worker 快照独立显示，以及缺失快照时的“未提供”语义。

运行时重新部署后，模块、Worker、严格 Reader 与参考宿主的构建/运行文件 SHA-256 一致；
`slicer_ui_host_sim --self-test` 输出 `STAGE14E02_SELF_TEST_PASS spi=1 calls=6`。

## 4. 边界

本次只修复宿主遥测展示与阶段估算，没有修改 SPI v1、Worker 文件合同、生产 TIFF、RGBWSV 通道、
Profile 身份、切片算法或包协议。宿主连续墙钟与 Worker 核心耗时口径不同，界面保留两项独立展示，
不得将二者相减或混作同一性能指标。
