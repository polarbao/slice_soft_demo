# REPORT_16 切片几何采样、甲片接触姿态与性能专项当前状态

> 状态：**ENGINEERING CANDIDATE COMPLETE / PRODUCTION DEFAULT DEFERRED**
> 日期：2026-08-13
> 当前默认：**Legacy + S0 + P0**

## 1. Current State

Stage 16 已完成准入审计、采样合同与候选、姿态诊断、性能 telemetry/基线、受限生产接入、Qt
诊断和统一回归 Gate：

```text
16-00-01..04       COMPLETE / PARTIAL GO
16A-01..06         COMPLETE
16B-01..03         COMPLETE
16C-01..03         COMPLETE
16D-01..04         COMPLETE
16D-05             NOT AUTHORIZED
```

16D-03 在当前源码身份下完成 Debug/Release 定向 CTest、Quick CI、full regression、Stage 15、
13G、Package/RIP strict 与两套 Runtime 发布。生产协议继续为 `p0.rgbwsv.2`、`uint8`、
`black_is_print`、`R G B W S V`。

## 2. Candidate State

| 候选 | 当前结论 | 可用范围 |
|---|---|---|
| S0 Legacy center sample | 生产默认 | 全部既有生产路径 |
| S3 adaptive 2x2 `>=2/4` | 工程候选 | 仅 `relief_heightfield` 且必须显式 opt-in |
| S4 adaptive 2x2 `>=1/4` | 上限对照 | 诊断/矩阵，不接生产 |
| P0 当前姿态 | 生产默认 | 全部既有生产路径 |
| P3 接触面积候选 | 只读诊断 | Qt 展示和 A/B 评估；未授权写回姿态 |

Reality 5/5、Stage 15 白区载体和合成 fixture 已覆盖采样候选；Reality P0/P2/P3 矩阵证明
P2 在 5/5 上降低首半层接触面积，P3 在 5/5 上提高该指标但支撑扰动明显。因此 P2 不推进，
P3 继续保持诊断状态。

## 3. Production Default Decision

当前不切换默认值：

1. S3 只允许显式选择，未知/S2/S4、S3 与非 `relief_heightfield` 组合均 fail-closed；
2. P3 没有进入 Effective Config、Facade 或 Worker 的姿态应用链；
3. Qt 只展示策略、候选姿态与生产 TIFF A/B，不在 UI 重算几何；
4. 未执行 16D-05，不得把候选可用描述为生产默认完成。

## 4. Performance Budget State

16C-02 已完成 S0/S3/S4 x 1/11/12/22 的 Release cold/warm、core/end-to-end、峰值内存、
确定性 hash 与 RIP strict。S3/S4 没有在所有场景中显著快于 S0，当前价值是几何语义候选，
不是通用性能替代。

该矩阵使用 127 dpi / 0.2 mm 功能场景；正式设备 buildVolume、原点、轴向、22 实例目标时限和
峰值内存预算尚未冻结，故生产预算状态保持 `INPUT_OPEN`。

## 5. External Pending Confirmation

以下内容未在 Stage 16 内伪造完成：

1. 外部目标 RIP、打印机和实物工艺验证；
2. 外部参考 TIFF 的权威 RGBWSV 语义、物理分辨率、层高及重分发授权；
3. 正式设备 1/11/12/22 SLA 与内存上限；
4. 16B-04 P3 姿态接入授权；
5. 16D-05 S3/P3 默认切换授权。

## 6. 12F/13F Carry-In Disposition

| Carry-in | 处理结果 |
|---|---|
| Release 基线、细分 telemetry | 由 16C-01/02 完成 |
| 支撑统计重复扫描 | 由 16C-03 完成 |
| Reality 定向与旧 Writer 基线 | 定向已满足；性能基线由 16C-02 替代 |
| 平移实例复用 | Stage 13B 已满足既定范围 |
| Bottom Projection Range Provider | 未实现，保留后续优化 |
| Compose/Buffer 复用、Occupancy 流式化 | 未实现，保留后续优化 |
| 生产几何/支撑缓存 | 未实现，保留后续优化 |
| Preview/I/O 解耦、自适应 Preview | 未实现，保留后续优化 |
| 有内存预算的有限并行 | 未实现，等待内存预算 |
| 正式 22 实例 production Gate | `INPUT_OPEN` |

这些未实施项不影响“Legacy 默认 + S3 显式候选”的工程收口，但在任何生产默认切换或正式性能
承诺前仍需重新排期。

## 7. 阶段结论

Stage 16 可以以“工程候选完成、生产默认延期”收口。后续有两条独立路线：

```text
产品授权路线：16B-04 -> 新姿态统一回归 -> 16D-05 默认切换评审
性能优化路线：16C-04..09 -> 正式设备输入 -> 16C-10 production Gate
```

在获得新授权前，不再修改默认采样或姿态。
