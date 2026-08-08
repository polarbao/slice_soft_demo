# DOC PREP HOSTFLOW H-B-06 切片提交与作业管理准备

> 状态：**PREPARATION GATE PASS / IMPLEMENTATION COMPLETE**
> 日期：2026-08-08
> 任务：HOSTFLOW H-B-06
> 范围：`slice.rgbwsv` 提交、非阻塞进度、协作取消、终结错误展示和句柄释放。

## 1. 准备结论

H-B-06 的前置 H-B-05 已通过。公开 SPI、Worker DTO、取消合同、场景 authority 和有效 Profile
均有冻结真源，不需要新增导出、能力或 schema，可以进入开发。

本任务只补参考宿主的作业生命周期。生产包摘要、报告、层预览和通道图属于 H-B-07；设置与
工作区恢复属于 H-B-08。

## 2. 冻结输入

提交前必须同时满足：

1. `HostModelImportWorkflow` 已持有非零 committed `sceneHandle`；
2. 通过 `scene.get_snapshot` 取得同一 revision 的 `sceneHash` 和 canonical `scene`；Worker
   文件合同要求 `sha256:` 前缀时，宿主只规范化 hash 表示，不修改 scene 内容；
3. H-B-05 `EffectiveProfile()` 已通过本地校验，`profile.profileHash` 与外部 hash 一致；
4. `output.packageDir` 是有效 Profile 中的绝对路径；
5. Profile 经 HQ-08-A 求交后包含 `slice.rgbwsv`。

`scene` 只允许从 snapshot 不透明透传，宿主不得构造、删减或修补内部字段。

## 3. 请求与状态机

请求严格使用既有 DTO：

```text
capability = slice.rgbwsv
jobId / correlationId = 宿主生成的唯一身份
sceneHash / scene = scene.get_snapshot 权威响应
profile = H-B-05 有效 Profile
output.contract = p0.rgbwsv.2
output.packageDir = Profile 输出目录
options.backend = worker
```

宿主状态机：

```text
idle -> queued -> running -> succeeded | failed
                  \-> cancelling -> cancelled | failed
```

- 使用 200-500 ms `QTimer` 轮询，禁止 UI 线程忙等；
- `percent` 必须单调且在 0..100；非法 JSON、未知状态或进度回退均 fail-closed；
- 仅终结态调用 `pm_result`；每个 handle 只调用一次 `pm_release`；
- 正常终结结果根级 `packageDir` 必须与提交目标一致；
- 同一模块最多一个 Worker 作业，活动期间禁用场景/Profile/切片参数编辑。

## 4. 取消与清理

取消遵循 `contracts/slicer_cancel_contract.json`：

1. `pm_cancel` 幂等；UI 进入 `cancelling` 后继续轮询；
2. 2 秒内必须进入 `cancelled` 或显式 `failed`；
3. `cancelled` 结果码为 `PM-SLICER-CANCELLED-0070`；
4. 不发布本次 package，不覆盖上一有效包；
5. 输出根下不得残留 `.staging`、`.backup`、`.lease`；
6. 关闭宿主时若仍活动，先取消、等待 Worker 收口，再释放句柄。

## 5. UI 边界

参考宿主右侧新增“切片作业”页，提供：

- 开始切片与取消作业；
- 状态、phase、current/total、percent、elapsedMs；
- 终结 code、message、packageDir、取消延迟；
- 未导入模型、Profile 不可切片、有效配置无效和模块未加载时的明确禁用原因。

H-B-06 不读取生产 TIFF，也不实现结果预览。

## 6. 文件所有权

允许修改：

```text
apps/slicer_ui_host_sim/HostSliceJobController.*
apps/slicer_ui_host_sim/HostSliceJobPanel.*
apps/slicer_ui_host_sim/HostMainWindowJob.cpp
apps/slicer_ui_host_sim/HostMainWindow.*
apps/slicer_ui_host_sim/Main.cpp
apps/slicer_ui_host_sim/CMakeLists.txt
tests/hostflow/HostSliceJobTests.cpp
apps/slicer_worker/slice/WorkerSliceExecutor.cpp
src/slicer_module/WorkerJobService.cpp
```

后两项仅用于补齐公开 Worker 错误诊断：模块透传 Worker 最后一条非空 stderr，切片 Worker
返回第一个权威预检问题。不得改变成功结果、错误码、生产协议或 Worker 文件合同。

`apps/slicer_debug_ui/**` 仅用于 A/B 对照，禁止修改。参考宿主继续不得 include/link
`slicer_core`。

## 7. 验证门禁

1. Debug/Release 真实 Worker 成功作业发布 `manifest.json`；
2. queued/running 取消在 2 秒内终结，且无临时残留；
3. 零 sceneHandle、Profile hash 不闭合、输出身份不闭合均 fail-closed；
4. 作业活动期间不可重复提交或编辑场景；
5. H-B-01..06 联合回归、参考宿主边界、缺失模块和 self-test 通过；
6. `/W4 /WX`、500 行源文件守卫和 `git diff --check` 通过。

## 8. 停止条件

出现以下任一情况必须停止并建立受控修订：

- 需要修改 SPI v1、11 导出、15 能力或 `p0.rgbwsv.2`；
- 需要宿主读取内部 scene/profile/scenario 文件；
- 需要进程内切片或 Worker 失败后静默 fallback；
- 需要在 H-B-06 读取 TIFF、实现包预览或修改 RGBWSV 极性。

## 9. Revision History

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-08 | v1.1 | H-B-06 实现完成：参考宿主新增真实 Worker 作业提交、轮询进度、协作取消、终态结果和错误详情；闭合 `sceneHash` 表示、根级 `packageDir` 和 Worker 诊断透传。Debug/Release 宿主联合门禁各 13/13、Worker 合同与取消门禁各 6/6 PASS。 |
| 2026-08-08 | v1.0 | 完成输入、状态机、取消、UI、文件所有权、门禁与停止条件准备。 |
