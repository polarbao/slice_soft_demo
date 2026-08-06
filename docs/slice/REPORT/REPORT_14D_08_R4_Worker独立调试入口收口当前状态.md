# REPORT_14D-08-R4 Worker 独立调试入口收口当前状态

> 日期：2026-08-07
>
> 状态：`COMPLETE / PARENT 14D-08 COMPLETE`

## 1. 收口结论

`slicer_worker.exe --spi-request <absolute-request-path>` 已成为与 DLL 启动链共享同一 runtime、
同一三项真实 executor、同一结果写入与退出映射的独立调试入口。14D-08 父任务的五项收口
条件均已满足。

## 2. 条件核对

| 条件 | 状态 | 证据 |
|---|---|---|
| 三项真实 capability | PASS | slice/full-preflight/repair 均由生产 Worker 精确注册 |
| DLL 与独立入口共享 runtime | PASS | 两者均启动同一 `slicer_worker --spi-request` 链路 |
| 安全发布、取消、唯一路由 | PASS | 14D-05、14D-04B、14D-06 均 COMPLETE |
| Debug/Release、RIP strict、无 fallback | PASS | 14D-07-R2 E-01..08；14D-08 定向 CTest 各 10/10 |
| IDE 直接启动调试 | PASS | VS Code Worker launch/task 与可移植请求生成器已接入 |

## 3. 本任务修正

- 更新 R1 历史无 fake 测试：真实 repair executor 已存在后，测试改为验证非法 repair 请求稳定
  返回 `PM-SLICER-INPUT-0001`，同时继续扫描 production Worker 不得包含测试 executor token；
- 新增 `Prepare14D08WorkerDebugRequest.py`，生成绝对路径、Profile hash 闭合且可成功执行的
  `geometry.repair` 请求；
- 新增 VS Code `SliceSoft: Advanced - Debug Worker Request`，预任务只构建 Worker 并生成请求。

## 4. 验证

```text
Debug  stage14d08 CTest 10/10 PASS
Release stage14d08 CTest 10/10 PASS
Debug  generated geometry.repair direct request PASS
Debug/Release no-fake/fail-closed validator PASS
.vscode/launch.json and tasks.json JSON parse PASS
```

14D-07-R2 同时证明生产 Package、RIP strict、golden、报告、进度、负例、取消恢复和替换基线。

## 5. 边界

- 调试请求生成器只写 `output/debug/stage14d08_worker`，不改仓库样例或生产配置；
- IDE 入口不形成第二套业务实现，不允许把测试 fake 注入 production Worker；
- 不修改 SPI v1、11 导出、15 能力、RGBWSV/TIFF、材料策略或 Qt UI；
- 14D-08 完成不等于 14E 已启动。
