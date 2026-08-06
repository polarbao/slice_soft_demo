# DOC_PREP_14D-03 文件合同协商实施准备

> 日期：2026-08-06
> 状态：`PREPARATION_GATE = PASS`
> 对应任务：`14D-03`

## 1. 依赖核验

- 14A-03 已冻结 `file_contract_v1`、四份 JSON 合同和退出码映射；
- 14D-01 已提供独立 `slicer_worker.exe` 参数外壳；
- 14D-02 已提供受控 WorkerClient，可在不要求终态进度的模式运行协商命令；
- Worker 真实请求执行仍属于 14D-08，本卡不得伪造。

## 2. 协商规则

Worker 的 `--contract-info` stdout 必须输出一份符合
`file_contract_v1.contract_info.schema.json` 的 UTF-8 JSON：

```text
contract       = file_contract
major          = 1
minor          >= 0
engineVersion  = 非空
produces       包含 p0.rgbwsv.2
capabilities   是 slice.rgbwsv / geometry.preflight.full / geometry.repair 的非空唯一子集
```

模块侧兼容规则：

```text
worker.major == required.major
worker.minor >= required.minor
```

major 不同、minor 过低、JSON 非法、缺少 `p0.rgbwsv.2`、能力越界或重复均 fail-closed。
未知附加字段可接受，以保持 minor 向后兼容。

## 3. 进程与输出边界

- 协商复用 14D-02 WorkerClient，`requireTerminalProgress=false`；
- `--contract-info` 成功时不得产生切片、staging 或持久化副作用；
- JSON 与普通诊断日志必须可区分，不允许从任意日志片段猜测合同；
- Worker 退出非零、超时、取消、保留行语法错误均按现有稳定错误路径返回；
- 协商结果只决定 Worker 是否可被使用，不修改 SPI、能力 DTO 或 TIFF。

## 4. 文件所有权与验证

允许修改：

```text
apps/slicer_worker/*
src/slicer_module/WorkerContract*.h/.cpp
tests/stage14d_03/*
CMakeLists.txt（主代理串行集成）
```

验证覆盖 Debug/Release 正向协商、major 篡改、minor 过低/向后兼容、非法 JSON、缺少 produces、
能力越界、重复调用无副作用、Worker 14D-01 参数回归和 DLL 11 导出/依赖回归。
