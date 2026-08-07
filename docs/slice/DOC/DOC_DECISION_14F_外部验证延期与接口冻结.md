# DOC_DECISION_14F 外部验证延期与接口冻结

> 文档状态：**ACCEPTED / USER AUTHORIZED / INTERFACES FROZEN**
> 版本：v1.0
> 日期：2026-08-07
> 适用范围：Stage 14F 打印侧 M1/M2、目标 RIP S2 与端到端验收

## 1. 用户决策

2026-08-07，用户明确授权 Stage 14F 在当前仓库内继续推进，并允许暂不执行打印侧验证：

```text
打印侧可实现性：按假定成立处理；
打印侧实际验证：延期，不伪造 PASS；
切片侧公开接口与交付文档：立即冻结；
Stage 14F：继续完成本地 S1/S2 合同门禁和收口证据。
```

该决策只解除“等待外部打印软件后才能继续开发”的流程阻断，不等价于打印侧、目标 RIP、
干净机或实物打印已经验收。

## 2. 冻结接口

以下接口从本决策生效起进入 Stage 14 冻结状态：

| 接口 | 冻结版本/结论 |
|---|---|
| C ABI | `PM_SPI_VERSION=1`，精确 11 个 `pm_*` 导出 |
| 能力面 | 15 项能力；`contracts/slicer_capability_dtos.json` v1.4 |
| 三车道 | `contracts/slicer_three_lane_contract.json` v1.1 |
| Worker 文件合同 | `file_contract_v1` 请求、结果、协商与退出码 |
| 模块部署 | `module.json`、`slicer_module.dll`、`slicer_worker.exe` 及依赖清单 |
| S1 | `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`、0 打印、255 不打印 |
| S2 | `DOC_DECISION_14_S2_RIP接口合同定案.md` v1.1 |
| UI 参考合同 | `contracts/slicer_ui_view_spec.json` v1.0；不属于能力 ABI |

后续若必须改变上述字段、版本、能力数量或语义，必须新建受控修订文档；不得直接修改本决策
来把破坏性变化伪装成 Stage 14 原合同。

## 3. 14F 状态解释

Stage 14F 后续统一使用以下状态，不再把“本地完成”和“外部验收”混为一谈：

```text
SLICER_SIDE_COMPLETE          本仓库实现、构建、正负例和交付证据通过；
INTERFACE_FROZEN              对外接口已冻结；
IMPLEMENTABILITY_ASSUMED      用户授权按打印侧可实现处理；
EXTERNAL_VALIDATION_DEFERRED  打印侧/RIP/干净机/实物证据尚未执行；
EXTERNAL_ACCEPTED             仅在未来取得外部真实证据后使用。
```

禁止把 `IMPLEMENTABILITY_ASSUMED` 或 `EXTERNAL_VALIDATION_DEFERRED` 写成 `PASS`、
`EXTERNAL_ACCEPTED`、`PRODUCTION READY` 或“打印侧已验证”。

## 4. 后续任务边界

| 任务 | 当前允许执行的内容 | 延期内容 |
|---|---|---|
| 14F-02 | 固化 M1 handoff、模块装载/能力/自检本地门禁 | 打印侧 ModuleRegistry 与进程模块证据 |
| 14F-03 | 单模型导入、变换、切片、S1 正例及 7 类负例 | 打印宿主 M2 的真实 UI/服务联调 |
| 14F-04 | S2 C1-C7 机器合同、`rip_output_validator` 本地门禁 | 目标 RIP 实际输出与打印侧 ChannelSplitter |
| 14F-05 | 汇总切片侧证据并以“外部验收延期”收口 | 干净机、并行打印、长稳、实物工艺放行 |

14F-05 完成后，Stage 14 可标记为：

```text
SLICER PACKAGE READY / INTERFACES FROZEN / EXTERNAL ACCEPTANCE DEFERRED
```

不得标记为完整生产发布或三方端到端验收完成。

## 5. 回退条件

未来打印侧或目标 RIP 若证明冻结接口不可实现：

1. 保留本决策和当时证据；
2. 新建 `DOC_DECISION_14F_R1_*` 受控修订；
3. 重新运行 ABI、Worker、S1、S2 与交付包门禁；
4. 未完成新一轮验证前，继续保持 `EXTERNAL_VALIDATION_DEFERRED`。

## 6. 不变量

本决策不修改：

```text
p0.rgbwsv.2；
R G B W S V 通道顺序；
uint8；
black_is_print；
Legacy 默认切片路径；
手写 TIFF Writer 默认值；
OpenVDB 默认关闭状态。
```
