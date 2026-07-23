# DOC_DECISION_12E-08C-R4-07-R2 受限生产候选预算冻结规则

> 文档状态：ACCEPTED / ENGINEERING CANDIDATE BUDGET
> 决策时间：2026-07-23
> 适用阶段：12E-08C-R4-07-R2
> 不适用范围：正式产品 SLA、任意机器性能承诺、12E-08D 生产授权

## 1. 决策

R4-07-R2 冻结一组“参考机器工程候选预算”，用于判断当前两模型族、四用例的 Release
`globalCoreMs` 和 `peakWorkingSetBytes` 是否发生明显回退。该预算是 R4-08-R2 的一个必要输入，但单独
通过不构成 production GO。

预算固定在以下轨道：

```text
Reference machine：LENOVO 21LD / Intel Core Ultra 5 125H / 32 GB
Build：Release / MSVC 19.51.36248.0 / Visual Studio 18 2026
Backend：legacy_cpu_global_distance
USE_OPENVDB：OFF
voxelMm：0.20
Warm-up：1
Measurement：每 case 5 次
Core timing：globalCoreMs，不含 TIFF/PNG/JSON 写盘
Memory：peakWorkingSetBytes
```

编译器、CPU、模型 hash、backend、体素或测量口径变化时，不允许直接沿用本预算，必须重新基线并产生新
`policyVersion`。

## 2. 阈值推导

R4-07-R1 的 3 次观测只用来建立阈值依据，不由脚本自动生成阈值：

| Case | R1 median ms | R1 max ms | R1 peak bytes |
|---|---:|---:|---:|
| `development_xiao_ma_minimum` | 372.2191 | 378.2123 | 26,374,144 |
| `development_xiao_ma_all_texture` | 435.9968 | 496.5500 | 26,865,664 |
| `development_yecan_intermediate` | 525.3167 | 534.8622 | 31,137,792 |
| `texture2d_3mf_control` | 40.8597 | 47.5680 | 7,303,168 |

候选预算采用以下保守规则：

```text
median ceiling = R1 median x 1.5，向上取整到 100 ms；
single-run ceiling = R1 max x 2.0，向上取整到 100 ms；
memory ceiling = 至少 R1 peak x 2，向上取 2 的幂字节，并保留 32 MiB 进程基线下限。
```

冻结值：

| Case | Median max ms | Single-run max ms | Peak working set max |
|---|---:|---:|---:|
| `development_xiao_ma_minimum` | 600 | 800 | 64 MiB |
| `development_xiao_ma_all_texture` | 700 | 1000 | 64 MiB |
| `development_yecan_intermediate` | 800 | 1100 | 64 MiB |
| `texture2d_3mf_control` | 100 | 100 | 32 MiB |

该余量用于吸收当前 Windows 调度、温度和后台负载波动，不代表允许算法持续变慢。若连续多次接近阈值，
应先分析回退原因，不应直接抬高预算。

## 3. Gate 语义

预算 Gate 仅在以下条件全部成立时 PASS：

```text
R4-07-R1 候选身份与 source/resource hash 匹配；
参考机器、编译器、Release/OpenVDB OFF/backend/voxel 身份匹配；
四个 case 身份完整且每 case 至少 5 次正式测量；
每个 case 的 median、single-run max、peak working set 均未超预算；
diagnosticOnly=true；
productionOutputWritten=false；
productionAdmission=not_evaluated。
```

任一条件失败必须返回非零退出码，不允许 warning 后继续判定 PASS，也不允许自动修改预算文件。

## 4. 状态边界

R4-07-R2 PASS 后：

```text
candidateBudgetStatus=frozen_pass；
release_budget_not_frozen blocker 可移除；
Quick CI baseline 和 explicit 08D authorization blocker 继续保留；
复杂浮雕 0/3 继续作为覆盖缺口；
global_surface_shell 继续 diagnostic-only；
12E-08D 仍未授权。
```

固定协议 `p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print` 不受本决策影响。
