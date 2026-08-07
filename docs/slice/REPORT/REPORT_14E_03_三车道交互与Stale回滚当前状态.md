# REPORT_14E-03 三车道交互与 Stale 回滚当前状态

> 状态：COMPLETE
> 日期：2026-08-07
> 前置：14E-02 COMPLETE
> 下一任务：14E-04 带纹理俯视渲染与移动优化

## 1. 任务目标

在 Qt 参考宿主中落地冻结的 Transient / Commit / Production 三车道中的前两条交互路径，
证明指针移动不跨 DLL、正常 Commit 不追加快照，并在 `SceneRevisionStale` 时按合同丢弃
本地瞬态状态、读取权威 snapshot，且不自动重试用户操作。

## 2. 实现内容

| 模块 | 交付物 | 责任 |
|---|---|---|
| 本地变换策略 | `TransformCommitPolicy.*` | 保存 transient 平移；不持有 `ModuleClient`；构造带双 revision 和唯一 operationId 的 Commit 请求 |
| 场景控制器 | `SceneInteractionController.*` | 场景 bootstrap、权威 revision/hash、Commit 采纳、Stale 快照恢复 |
| 调用计数 | `ModuleClient::CallCount` | 为 UI-M1 提供可自动断言的跨 ABI 调用证据 |
| 真实模块测试 | `Stage14E03InteractionTests.cpp` | 使用公开 DLL 创建场景、提交变换、制造并发 revision 并验证恢复 |

## 3. 三车道约束

### 3.1 Transient

- `BeginTransient` 和连续 `UpdateTransientTranslation` 仅改变宿主内存。
- 策略对象不接收 `ModuleClient`，结构上禁止在 mouse-move 路径跨 DLL。
- 运行时连续 50 次更新前后 ABI 调用计数保持 0。

### 3.2 Commit

- 请求同时携带 `currentSceneRevision` 与 `expectedSceneRevision`。
- 每次新 payload 使用新的 `operationId`。
- 成功后直接采纳 `newSceneRevision` 与 `sceneHash`，不追加 `scene.get_snapshot`。
- 场景 bootstrap 后只做一次显式初始刷新，用于取得合同允许的 `sceneHandle`；该刷新不属于用户 Commit 尾随调用。

### 3.3 Stale

- `PM-SLICER-LAYOUT-0022` 到达后立即丢弃 transient。
- 只调用一次 `scene.get_snapshot`，采纳权威 revision/hash/handle。
- 不使用旧 operationId，不自动重试原 payload，revision 不额外递增。

Production 车道仍由后续 14E-04b 的能力覆盖任务接入，本任务未重复实现 Worker 切片。

## 4. 验证结果

```text
cmake --build build --config Debug --target slicer_ui_host_sim stage14e03_interaction_tests --parallel 4
ctest --test-dir build -C Debug -R "stage14e0[23]" --output-on-failure
4/4 PASS

cmake --build build --config Release --target slicer_ui_host_sim stage14e03_interaction_tests --parallel 4
ctest --test-dir build -C Release -R "stage14e0[23]" --output-on-failure
4/4 PASS

ValidateSourceSizeGuard.py --base-ref HEAD
PASS（仅既有 G4/G5 warning）
```

测试关闭以下硬指标：

| 指标 | 结果 |
|---|---|
| UI-M1 mouse-move 跨 DLL 调用 | PASS，50 次更新为 0 |
| 正常 Commit 追加 snapshot | PASS，计数不变 |
| UI-M4 Stale 回滚 | PASS，精确读取一次 snapshot，revision 与模块一致 |
| Stale 自动重试 | PASS，未发生额外 revision 递增 |

## 5. 边界与后续

- 未修改 SPI v1、11 导出、15 能力和三车道合同。
- 未修改生产 TIFF、Package schema 或 Worker 路由。
- 未修改既有 `slicer_debug_ui`。
- 14E-03 尚未实现 top ViewData、纹理 blob/cache、俯视帧率和 P95 Commit 采样；这些属于 14E-04。
