# DOC_PREP_14C-04 同步轻能力接线实施准备

> 日期：2026-08-06
> 状态：`PREPARATION_GATE = PASS`
> 对应任务：`14C-04`

## 1. 依赖核验

| 依赖 | 状态 | 结论 |
|---|---|---|
| 14B-00 | COMPLETE | `model.import` 已确认归属 `slicer_base` |
| 14B-02 | COMPLETE | Model / PackageQuery Facade 可复用 |
| 14B-03 / 03A | COMPLETE | SceneFacade 与 TexturedSceneViewDataProvider 已冻结 |
| 14C-02 | COMPLETE | 所有 JSON 字符串出参统一使用 `WriteOut()` |
| 14C-03 | COMPLETE | module/job 句柄、终态快照和 TLS 错误槽可复用 |

## 2. 能力边界

`syncCapabilities[]` 必须逐条等于 `DEV_14` §5 中标记“DLL 进程内”的能力：

```text
model.import
model.get_metadata
model.release
scene.apply_operation
scene.get_snapshot
scene.get_viewdata
geometry.collision
geometry.preflight（仅 `mode=fast`）
package.verify
package.get_summary
package.get_layer_descriptor
package.render_layer_preview
package.read_report
```

以下能力禁止进入同步数组：

```text
geometry.preflight（`mode=full`，必须拒绝并交由 Worker）
geometry.repair
slice.rgbwsv
```

外部冻结能力总数仍为 15；上表是承载路由，不新增能力或导出函数。

## 3. 执行合同

- `pm_submit` 解析请求后，通过已有 Facade 适配器同步执行；
- `syncCapabilities[]` 对外仍声明冻结能力 ID `geometry.preflight`，不得另造
  `geometry.preflight.fast`；承载路由由请求字段 `mode=fast|full` 决定；
- 受理成功时创建终态 job，首次 `pm_poll` 已返回 100% 与 succeeded/failed；
- `pm_result` 返回冻结 DTO v1.2 的结果 envelope；
- 未知能力、Worker 能力和非法 JSON fail-closed，并写 TLS 错误；
- `scene.get_viewdata` 必须复用 14B-03A Provider，不允许灰模成功或另造 DTO；
- 所有输出走 14C-02 缓冲协议，不允许第二套写出函数。

## 4. 文件所有权与验证

允许修改：

```text
src/slicer_module/Exports.cpp
src/slicer_module/*Adapter*.h/.cpp
tests/stage14c_04/*
CMakeLists.txt（主代理串行集成）
```

验证至少覆盖同步能力清单漂移、Worker 能力拒绝、非法请求、首次 poll 终态、result 缓冲三态、
ViewData Provider 路由以及 Debug/Release 11 导出/依赖回归。

本卡不启动 Worker，不实现模块自述，不扩大 DLL 依赖图。
