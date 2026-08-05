# SliceSoft 取消与清理合同

> 合同版本：1.0
> 机器可读真源：`contracts/slicer_cancel_contract.json`

## 1. 状态语义

```text
queued  -> running | cancelling
running -> succeeded | failed | cancelling
cancelling -> cancelled | failed
```

`pm_cancel` 返回 `PM_OK` 只表示取消请求已受理，调用返回前状态必须已经变为 `cancelling`。
`cancelling` 是非终结状态，不能调用 `pm_result` 读取最终结果。只有 Worker 真实退出且 staging
清理完成后，状态才允许变为 `cancelled`。

重复取消幂等并返回 `PM_OK`；对终结作业取消是返回 `PM_OK` 的空操作。取消请求受理到成功进入
`cancelled` 的上限为 2000ms。

## 2. Worker 与强制兜底

DLL 原子创建 `cancel.requested`，Worker 在步骤边界和逐层循环内检查。Worker 在宽限期内协作退出时，
返回进程退出码 8 和 `PM-SLICER-CANCELLED-0070`。若 2000ms 内未退出，DLL 终止承载 Worker 的
Windows Job Object，等待完整进程树退出后再执行模块侧清理。

关闭 Job Object 是进程树兜底，不等于把作业提前标成 `cancelled`。`pm_release` 遇到活动作业时
必须先走同一取消流程并等待结束，不能直接释放句柄或杀宿主线程。

## 3. 安全发布与双保险清理

取消结果必须满足：

```json
{
  "ok": false,
  "code": "PM-SLICER-CANCELLED-0070",
  "cleanup": {
    "stagingRemoved": true,
    "published": false
  }
}
```

Worker 启动时清理当前精确目标的历史 staging；取消时 Worker 先清理，进程退出后 DLL 再做一次
幂等清理。所有路径必须规范化，并限制在 `<packageDir>` 同级、由该精确目标派生的 staging/
backup 名称。不得递归删除目标父目录之外的内容，不得覆盖上一个有效包。

若 Worker 已退出但 staging 仍无法清除，禁止谎报 `cancelled`；作业进入 `failed` 并返回
`PM-SLICER-OUTPUT-0050`，同时保留诊断路径。该分支不属于“成功取消 ≤2s”的验收样本，必须作为
独立清理失败负例留证。

## 4. 验收边界

14D 实现时须在 import/preflight/layout/grid/mask/texture/support/compose/write/report 各阶段取消一次，
每个成功取消样本均须在 2s 内进入 `cancelled` 且无 staging 残留。还必须覆盖重复取消、终结后取消、
`cancelling` 期间读取结果、Worker 无响应后的 Job Object 兜底和清理失败负例。
