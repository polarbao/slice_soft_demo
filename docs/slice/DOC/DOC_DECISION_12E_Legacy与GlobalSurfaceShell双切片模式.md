# DOC_DECISION_12E Legacy 与 GlobalSurfaceShell 双切片模式

> 文档状态：Accepted / TARGET CONTRACT
> 日期：2026-07-20
> 前置：12E-08C-R1/R2/R3 真实模型修复与严格准入
> 落地阶段：12E-08D、12E-09B、12E-10

## 1. Context

当前正式生产路径是 legacy 切片。`global_surface_shell` 已完成三维分区、纹理传递、光栅映射和完整材料闭环
诊断，但尚未通过真实模型 topology/Release Gate，也没有进入正式 RGBWSV writer。

现有配置概念容易混淆：

```text
slicingMode：closed_mesh_scanline / relief_heightfield，描述几何切片类别；
texture.applyMode：描述纹理应用策略；
experimental.openvdbPipeline.engine：描述历史实验 backend；
以上均不是正式的端到端切片管线模式选择。
```

## 2. Decision

新增独立的正式管线模式契约：

```text
slicePipeline.mode = legacy | global_surface_shell
```

默认值固定为 `legacy`。旧配置省略 `slicePipeline` 时继续使用 legacy，行为和 TIFF 不变。

用户选择的是端到端切片模式，不是 OpenVDB/CPU backend。首个 global production candidate 仍使用默认
OpenVDB OFF 的 Legacy CPU global-distance backend；OpenVDB 继续只做 optional conformance。

## 3. Output Contract

两个生产模式必须复用同一套输出基础设施：

```text
RGBWSV TIFF writer；
p0.rgbwsv.2 manifest；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
printValue = 0；emptyValue = 255；
同一 preview/report/package 目录规则。
```

任何“切片成功且可使用”的结果都必须包含完整 TIFF layer list。不能只生成 preview PNG、diagnostic JSON 或
内存 mask 后显示成功。

Preview、材料伪彩图、报告和诊断文件继续保留，并作为 TIFF 之外的附加输出；它们不能替代生产 TIFF。

## 4. Admission State

Global 模式具有状态机：

```text
unavailable：实现或构建不可用；
blocked：配置、拓扑、修复或不变量失败；
diagnostic：可查看分区/preview，不写可使用 TIFF；
admitted：全部 Gate 通过，可进入共享 writer 生成 TIFF。
```

用户的目标要求适用于 `admitted` 生产模式。当前 global 仍为 diagnostic/blocked，不能因为新增 UI 选项就宣称
已经满足 TIFF 生产要求。

## 5. No Silent Fallback

Global 模式失败时：

```text
不得静默切换 legacy；
不得输出由 legacy 生成、但标记为 global 的 package；
不得用 preview 成功冒充 production success；
必须返回稳定 blocker 和失败状态。
```

用户可以明确重新选择 legacy 并重新运行，这是一笔新的执行请求。

## 6. UI Decision

普通 UI 最终提供中文选择：

```text
切片模式：传统切片（Legacy）
切片模式：全局纹理壳层（Global Surface Shell）
```

Global 未 admitted 时显示“诊断候选/不可生产”，生产按钮禁用或改为“运行诊断”。不在普通 UI 暴露 backend。

## 7. Alternatives

| 方案 | 结论 | 原因 |
|---|---|---|
| 继续用 `texture.applyMode` 代表完整管线 | 拒绝 | 纹理策略不能代表支撑、材料、writer 和 admission |
| 复用 `slicingMode` | 拒绝 | 会混淆 relief/closed-mesh 几何类别与端到端管线 |
| 让用户直接选择 Legacy CPU/OpenVDB | 拒绝普通 UI | backend 是实现和准入细节，不是产品工作流 |
| Global 失败自动回退 Legacy | 拒绝 | 输出来源不可审计，容易误判测试结果 |
| 两个模式各写一套 TIFF writer | 拒绝 | 协议漂移和维护风险过高 |
| 独立 mode router + 共享 writer | 采用 | 模式明确，输出合同统一，legacy 可保持稳定 |

## 8. Consequences

新增模式路由、配置校验、UI 状态和双模式回归，但不新增第二种 TIFF 协议。12E-08D 必须负责把 admitted global
composer 接到共享 writer；12E-09B 才开放普通用户生产选择。

## 9. Safety

```text
legacy 继续默认；
global 必须显式选择；
global 未 admitted 时不写可使用 TIFF；
OpenVDB optional/OFF；
repair 默认关闭；
协议固定不变。
```
