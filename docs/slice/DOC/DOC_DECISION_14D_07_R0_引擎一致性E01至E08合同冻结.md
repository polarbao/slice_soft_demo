# DOC_DECISION_14D-07-R0 引擎一致性 E-01 至 E-08 合同冻结

> 日期：2026-08-06
>
> 决策状态：`ACCEPTED_FOR_IMPLEMENTATION`
>
> 对应任务：`14D-07-R0`
>
> 后续状态：`14D-07-R1=READY`，`14D-07-R2=BLOCKED_BY_14D_05_AND_14D_08`

## 1. 决策目的

冻结可对任意合规 `slicer_worker` 版本执行的八项一致性主题，关闭
`DOC_PREP_14D_07_引擎一致性套件实施准备.md` 的 B1。该合同只定义输入、比较投影和通过规则，
不宣称当前 Worker 已通过，也不解除 14D-05/14D-08 的真实执行与安全发布阻塞。

## 2. 通用规则

1. 套件必须从公开 `file_contract_v1` 或 SPI 发起，不 include Worker 私有实现。
2. Worker 路径、配置和证据目录必须参数化；替换 Worker 时宿主、DLL、请求和 fixture 不变。
3. 固定 fixture 必须提交 model/Profile/scene hash；真实用户模型只作扩展证据，不作唯一 golden。
4. 每次运行记录 Worker/module/engine 版本、build config、合同版本、sceneHash、Profile hash。
5. 成功包必须为 `p0.rgbwsv.2`、`R G B W S V`、uint8、`black_is_print` 并通过 RIP strict。
6. 失败、取消和崩溃用例禁止产生可被识别为成功发布的 package。
7. 性能只记录，不作为 v1 一致性 PASS 条件；性能预算需独立冻结机器和阈值。
8. golden 变更必须显式审批，脚本禁止自动接受当前输出。

## 3. E-01 至 E-08

| 编号 | 目的 | 固定输入 | 比较与通过条件 |
|---|---|---|---|
| E-01 | 合同与身份闭合 | 三能力最小正例、合同/身份负例 | request/result 的 contract、jobId、correlationId、capability、sceneHash 精确闭合；错误码稳定；未知能力无 fallback |
| E-02 | 生产包协议 | 最小确定性 RGBWSV scene/Profile | package 结构、manifest schema、layer list、grid、TIFF tags、通道顺序、位深、极性及 RIP strict 全部通过 |
| E-03 | 固定 golden | 版本化最小 fixture | 同 engine build、backend、Profile 和 fixture 必须生产 TIFF 字节相同；允许的非确定字段从比较投影中显式排除 |
| E-04 | 报告语义 | E-02 包及固定阻断 fixture | scene/material/support/slice report 的 schema 与稳定字段投影一致；路径、时间等非稳定字段不得混入 golden |
| E-05 | 进度与耗时 | 正常成功、失败、取消 | percent 单调不回退，终态唯一；成功终态 100；timing 非负且阶段名合法；不要求不同机器数值相同 |
| E-06 | 错误与拒绝 | 损坏 request、stale scene、资源缺失、strict topology blocker | 稳定错误码/退出类别一致，`ok=false`，无成功 output/package，无进程内 fallback |
| E-07 | 取消与恢复 | 执行中取消、超时、崩溃、既有包 | 满足冻结取消时限；退出码/状态稳定；无 staging/tmp 残留；既有成功包字节不变；恢复动作只处理自有 job |
| E-08 | Worker 可替换性 | 前七项的同一套 fixture | 仅替换 Worker 二进制和声明版本，宿主/DLL/request 不变；E-01..07 全部通过，版本身份可追踪 |

## 4. 字节相等与语义相等

### 4.1 必须字节相等

- 同一 release 输入身份、同一 Worker/engine build、同一 backend/storage/compression 配置下的生产 TIFF；
- canonical request/result 的稳定身份投影；
- 固定 Schema/合同文件的版本化 fixture。

### 4.2 只要求语义相等

- manifest/report 中经合同声明的生成时间、绝对 evidence 路径、耗时和峰值内存；
- 不同 Worker 实现或版本间，在 release note 明确算法版本变化后的生产结果；
- 不同机器间的 wall/CPU/memory 指标。

跨 Worker 版本若生产 TIFF 字节变化，不能自动按“语义相等”放行。必须提交 golden 变更说明、
材料/几何差异报告、RIP strict 结果和发布授权，再更新版本化基线。

## 5. fixture 最小集合

R1 套件至少包含：

1. 一个小型、闭合、确定性 OBJ scene，覆盖 RGBWSV 和支撑；
2. 一个合法 3MF/纹理 scene，用于资源和多实例身份；
3. 一个 strict topology blocker，验证稳定拒绝；
4. 一个 stale sceneHash 请求；
5. 一个资源缺失/越界请求；
6. 一个可稳定进入运行态的取消 fixture；
7. 一个既有 package 保护 fixture。

fixture 必须使用 device-profile build volume 或明确标记为功能夹具；功能夹具通过不等于生产准入。

## 6. 证据格式

每次运行生成忽略的本地目录：

```text
output/benchmarks/14d_07/<run-id>/
  run_identity.json
  e01_contract_identity.json
  e02_package_protocol.json
  e03_golden.json
  e04_reports.json
  e05_progress_timing.json
  e06_negative.json
  e07_cancel_recovery.json
  e08_replaceability.json
  summary.json
```

`summary.json` 只有八项均 PASS 才能 `overall=pass`；NOT_RUN、BLOCKED、SKIPPED 均不得计作 PASS。

## 7. 实施拆分

### 7.1 `14D-07-R1` 参数化套件

- 新增机器可读一致性合同及 schema；
- 新增 fixture identity 清单；
- 建设可接收 Worker 路径的 runner、比较器和负例；
- 当前真实 slice 尚不可用时允许显示 NOT_RUN/BLOCKED，但禁止假 PASS。

### 7.2 `14D-07-R2` 当前 Worker Gate

前置：14D-05、14D-08、14D-04B 均完成。R2 在 Debug/Release 执行八项完整 Gate，并将
当前 Worker 作为基准版本。该结论仍不替代打印侧回签、外部 RIP 互操作或实物验证。

## 8. 不变量

- 不修改 SPI v1、11 个 `pm_*` 导出和 15 项 capability；
- 不修改 `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`；
- 不切换默认 TIFF Writer/压缩；
- 不启用 OpenVDB 默认路径；
- 不修改 Qt 主 UI；
- 不把测试 fake、空 package 或预览图当生产证据。

## 9. 门禁结论

```text
14D_07_R0_STATUS=COMPLETE
E_01_TO_E_08_CONTRACT=FROZEN_V1
14D_07_R1_PREPARATION_GATE=PASS
14D_07_R2_PREPARATION_GATE=BLOCKED_BY_14D_05_14D_08_14D_04B
14D_07_PARENT_GATE=BLOCKED
```

本决策关闭原准备文档的 B1；B2/B3 继续保留。下一步可与 R2-01 并行建设 R1 参数化测试外壳，
但完整替换准入仍必须等待真实 Worker 和安全发布。
