# REPORT_16C-06-MEMFLOW 有界流式内存根治当前状态

> 状态：**内存线与耗时线均已收口 / 已并入 `product/packaged-slicer`（合并点 `a226553`）**
> 日期：2026-09-08 ｜ 版本：v1.13
> 任务真源：`TASKS_16C_06_MEMFLOW_有界流式内存根治专项任务清单.md`（v4.21）
> ⚠ **本文档 v1.7 及更早的正文停留在 2026-08-21 的「生产仍为 Retained Dense」，
> 与实际状态相差三周。下方 §0 是现状摘要；§1~§7 的历史正文保留不删，
> 但阅读时须先看 §0。**

## 0.0 一页现状（2026-09-08 收口，**要引用数字请只引这一节**）

本专项分两条线：**内存线**（原始诉求）与**耗时线**（用户 2026-09-05 追加）。
两条都已收口并并入产品分支。下面每个数字都标了口径 —— **本专项最大的一课就是
CLI 口径与生产口径不是同一条代码路径**（见 §0.2 与任务卡 §14.1.9）。

### 内存

| 场景 | 最开始 | 现在 | 口径 |
|---|---|---|---|
| `a-2/0.2.obj` @10um | 22~34 GB（压在物理内存线上、大量换页） | **0.84 GiB** | CLI |
| `0.2.obj`+`0.3.obj` @10um | 外推 270 GB，**必然 OOM** | **1.91 GB，`valid=1`** | 生产 |
| 单实例 @0.1mm（143 层） | — | **971 MB** | 生产 |
| 入库 8 个 STL 资产 | 2,658~3,316 MB | 953~1,006 MB | CLI |

**内存线的结论不是「降了六七成」，而是「峰值与层数脱钩」**：同一批资产层数相差
33%、三角数相差 20 倍，峰值离散度从 25% 收到 5%。

### 耗时

| 场景 | 最开始 | 现在 | 口径 |
|---|---|---|---|
| 单实例 @0.1mm（143 层） | 78,963 ms | **41,117 ms（−43%~−48%）** | 生产 |
| └ 其中包发布（读回全量校验） | 39,567 ms | **5,120 ms（约 8 倍）** | 生产 |
| └ 其中 compose 窗口 | 38,698 ms | 35,770 ms | 生产 |
| `a-2/0.2.obj` @10um | 18.9 分钟 | 9.44 分钟 | CLI |
| `0.2.obj`+`0.3.obj` @10um | 从未跑完 | 31.3 分钟 | 生产 |

⚠ **两条口径不能相乘、也不能互相套用。** 生产口径的 10um 场景（1429 层）
**尚未在耗时线优化后重测** —— 已备好两侧二进制，待机器空窗执行。

### ⚠ 已作废的数字（**不要再引用**）

| 曾发布 | 实际 | 作废原因 |
|---|---|---|
| MF-09「a-2@10um 耗时 −50%」推广到所有场景 | 只成立于 1,429 层场景 | 该归因是推断，未做扫层厚对照 |
| MF-13a「收益低于噪声（−1.2%）」 | **−34.5%** | 测量时 18 个 MSBuild 争用 |
| MF-13「首测 −45%」 | −11.6%（MF-09 第一档） | 跨时间窗比较 |
| MF-14a「compose 窗口 −19.0%」 | **约 −3%** | 4~5 个 MSBuild 争用 |
| MF-13「total −29.8%」作为权威值 | 见上表 | 该轮 allbase 比真空窗高 34% |
| MF-14b「直接移交源层通道」可省大部分 | **不等价，已撤回** | `WriteOwnedPixel` 按归属重新推导像素 |

**这张表本身就是结论之一**：本机凡是靠墙钟测出的「几十个百分点」，只要当时有
并行会话在构建，复测时基本都会缩水。**做法：基准前后都查是否有 `cl.exe` /
`ctest` 在跑，并优先看 CPU 时间；只有真空窗那批数据可以下结论。**

### 剩余空间（按可靠度）

```text
可做   读回校验再并行化提高上限（现取 4 线程；提高须重测峰值内存）
       —— 但收益已从 39.6 s 压到 5.1 s，剩余绝对量不大
需改协议  让 sink 归还层缓冲（省每层 44 MB 分配填充）
需重启    MF-11 层循环并行化（可并行部分仅占 14.5%，上限约 14%）
未测      多材质大幅面（MF-10）的生产口径；缺够大的多材质资产（同 MF-07d）
未测      10um 生产口径的耗时线复测
```

**后两条「需改协议 / 需重启」都是接口与架构层面的改动，不属于「有界流式内存
根治」的范围，应另立专项并由用户裁定。**

---

## 0. 现状摘要（2026-09-06）

**本专项由用户的两项真实生产阻塞发起，两项均已实测解除：**

| 阻塞 | 改前 | 现在 |
|---|---|---|
| `a-2/0.2.obj` @10um 耗时约 20 分钟 | 18.9 分钟 / 22~34 GB | **9.44 分钟 / 0.84 GiB** |
| `0.2.obj`+`0.3.obj` @10um 内存不足失败 | 外推 270 GB，**必然失败** | **`valid=1` / 1.91 GB / 31.3 分钟** |

**生产路径已切换**，不再是「Retained Dense 为主」：

```text
单模型 CLI  relief + bottom_projection / full_vertical_projection 档走 Bounded
            三个整栈（model/support/support_type）不再物化，按列区间逐层重建
场景路径    实例侧、合成侧、写入侧三处均已逐层流式，峰值与层数【彻底脱钩】
            单实例 15/29/58 层同为 1.019 GB；三实例 58 层 2.98 GB
```

仍走 Retained 的档位（由 `EvaluateBoundedReliefSupportPath` 判定并记明 reason）：
`shape_enabled`、`base_projection`、`outer_varnish`、`unsupported_only`、
显式 `placement` 为 upper/both、非 `legacy_center_sample` 采样。

### 0.1 耗时专项（MF-09，2026-09-07 追加）

内存已解决，用户随后要求「不改硬件、看大画幅切片耗时还有多少空间」。插桩实测把
瓶颈定位在**材料闭合语义分析（占 layerCompute 的 86%）**，而不是原先假设的六处
整幅面 pass —— 已剪过的三处合计只占 3%。根因是
`MaterialClosureConfig::enabled` 默认为 `true` 而配置文件从不写它。

第一档已落地：主循环改走库里的 **View 版本** 并把 workspace 跨层复用，拿掉
「用 owning 便利接口的代价」（每层约 110 MB 的 workspace 分配归零 + 约 51.6 MB
的 mask 拷贝）。**交错 A/B（同一时间窗）`42,014 -> 37,132 ms`，−11.6%**，
两组区间不重叠；峰值内存无回退；用户指定资产上层 TIFF 与闭合报告逐字节一致。

⚠ **不要引用 −45% 这个数字。** 那是我第一次拿改后结果去比几小时前的旧基线得出
的，属跨时间窗比较；同一份改前二进制此刻只跑 42,014 ms。口径见任务卡 §14.1.6。

**⚠ 口径警告（比上面的数字更重要）：以上全部是「CLI 且开报告」口径。**
生产路径（`PrintApp -> slicer_module.dll -> slicer_worker.exe`）经
`LegacySceneLayerAdapter` 把 `write_reports` 写死为 `false`，而闭合精确分析由
`write_reports || repair` 门控，**故诊断模式下生产路径从不执行这一整段**。
即 **MF-09 第一档对生产路径收益为零**，§14.1.5 的三步剪枝做完也仍是零。

再往下拆的实测（同一快速档，layerCompute 38,237 ms）：边界洪泛占 **70.6%**、
主判定循环 2.8%、六次整幅面 fill 1.4% —— §14.1.5 原来的排序完全反了。
整项诊断关掉后 `layerComputeMs 37,405 -> 2,953`、`totalMs 42,133 -> 7,072`，
且 143 层 TIFF 逐字节一致（关不关是产品决策，本专项只量代价）。

**本专项此前所有耗时数字（含上表 a-2@10um 的 18.9 -> 9.44 分钟）都是 CLI 口径，
生产路径的耗时构成从未测量。** 已立 MF-12 先量生产路径，在那张表出来之前
不再对 CLI 专有路径做优化。见任务卡 §14.1.7~14.1.9 与 §14.4。

### 0.2 生产口径的耗时优化（MF-12~MF-14，2026-09-07）

**MF-12 量出生产口径后，整条优化线的靶子全换了。** 生产路径
（`a-2/0.2.obj @0.1mm`、143 层、单实例）改前的构成：

| 段 | 改前 | 占比 |
|---|---|---|
| 包发布（读回全量校验） | 46,806 ms | 48% |
| compose 窗口 | 约 46,400 ms | 52%（其中生产者实算仅约 13,100） |
| TIFF 写盘 | 约 3,800 ms | 4% |

**已落地三笔，均为「严格更少的工作量、判据一字不改」：**

```text
MF-13a  每个被校验的层被读两次、IFD 解析两次   -> 隔离口径 -34.5%
MF-13b  逐字节七件事改为逐通道 256 桶直方图     -> 隔离口径 -31.7%
        两者合计 11,482 -> 5,272 ms（-54.1%），CPU 时间同幅
MF-14a  逐像素热路径去掉约 210 亿次边界检查     -> 约 -3%（见下方口径说明）
```

**生产口径端到端（权威值，测于 MSBuild = 0 的空窗，交错取最小）：**

| | 改前（`704c161`） | 现在 |
|---|---|---|
| **totalMs** | **71,937 ms** | **51,665 ms（−28.2%）** |
| packagePublishMs | 36,536 | 16,851（−53.9%） |
| sliceProcessingMs | 35,169 | 34,579（−1.7%） |

三条独立算法互证：内部总时长差 20,272 ms ≈ 进程墙钟差 20,430 ms ≈
分项之和 20,708 ms。

⚠ **两处此前发布过的数字已作废，勿再引用：**

```text
MF-14a 的「compose 窗口 -19.0%」  -> 实为约 -3%（约 1 s）
                                    那是 4~5 个 MSBuild 下的产物
MF-13 单独的「totalMs -29.8%」    -> 该轮 allbase totalMs 为 96,754 ms，
                                    而真正空窗只有 71,937 ms，即那轮也不够空
```

**收益几乎全部来自「包发布的读回校验」这一处**；compose 侧（MF-14）只贡献约 3%。
详见任务卡 §14.6.11 与 §14.5.7。

### 0.3 耗时线累计结果（2026-09-07 收口）

在 §0.2 之后又落地了 MF-13c（校验不再物化整幅面像素）与 MF-13d（逐层校验
并行化，4 线程）。**专项耗时线起点 `704c161` 到现在，生产口径：**

| | 起点 | 现在 |
|---|---|---|
| **totalMs** | 78,963 ms | **41,117 ms** |
| packagePublishMs | 39,567 ms | **5,120 ms（约 8 倍）** |
| sliceProcessingMs | 38,698 ms | 35,770 ms |
| **峰值内存** | **971 MB** | **971 MB（不变）** |

**总时长降幅的诚实区间是 −43%~−48%**（取决于基线用哪一轮：本轮 `orig` 开跑时
机器上有 7 个 MSBuild，min 为 78,963；真空窗下同一基线只有 71,937）。
**publish 的约 8 倍不受基线选择影响。**

**峰值内存不变**是硬要求：MF-13d 的 4 线程各持一份 44 MB 文件缓冲（约 176 MB），
但发生在 compose 之后、落在既有峰值之下被吸收。**日后若调高并发上限，
必须重测峰值** —— 本专项的主指标是内存。

**零漂移：** 本专项此前只有 CLI 口径的逐字节判据，而 CLI 根本不走
`SceneLayerComposer`。MF-14a 顺带给 bench 加了场景口径的层哈希
（`BENCH_LAYERS digest=`），实测**改动前后全部为
`143:579bf5b2db951cde30bcb...`，跨 MF-13a/13b/14a 逐字节不变**。

**MF-14 已到 a 档收口，理由写在任务卡 §14.6.8/14.6.9：**
b 档（流式单实例直接移交源层通道）**实测不等价** —— `WriteOwnedPixel` 是按归属
重新推导像素而非逐字节搬运，Model 归属会丢掉支撑通道，直接拷贝会让模型像素
底下多打一层支撑；c 档余量仅 1~2 s。compose 窗口剩下的是生产者真实算力
（约 15 s，只能靠已 DEFERRED 的 MF-11）、盘速（约 7 s）、判据本身要求的一趟
闭合检查（约 7 s）与不可绕过的逐像素推导（约 4 s）。

**再往下压需要改协议或重启并行化，属接口/架构改动，不在本专项范围，
应另立专项并由用户裁定。**

**详细结论与验收口径见** `REPORT_16C_06_MEMFLOW_主循环接线可行性探查_2026_09_04.md`
（v1.15）与任务卡 v4.7，此处不重复。

## 1. 当前状态

用户已授权针对大幅面稀疏模型开启根治专项，并允许准备完成后进入开发。MF-00 已完成：决策、
技术设计、实施准备、原子任务和执行边界已建立。

当前生产代码仍以 Retained Dense 为主。16C-05 的 Mask 按需分配、统计融合和单实例 Buffer 移动
继续有效；MF-01 提供路由纯合同，MF-02 消除了 `run_slicer -> LegacySceneLayerAdapter` 的层交接
复制，MF-03A 提供 compact occupancy range 和单层物化 API，MF-03B1 提供 range-derived
pre-shape 支撑需求和 caller-owned 单层物化。上述能力均未把 Production Service
切换到 Bounded 路由，不能表述为完整流式化。

MF-03B2 现已提供 bounded outer-boundary 与 unsupported discovery 顺序扫描器、compact 需求和
逐源层事件摘要；MF-03B3 进一步提供 InternalVoid/Shape、post-shape footprint、compact report sink
和逐层 replay digest。两者都只属于非生产能力，尚未包含 BaseProjection/final varnish/material
重放，也没有连接 Production Service。

2026-08-21 的准备审计已把 MF-03B4 拆为 B4A 支撑最终重放与 B4B 材料/闭合最终重放。B4A 已按
冻结合同完成 plan-bound verified replay、Base/outer-varnish 最终化与逐层 compact connectivity sink；
B4B 仅解除依赖等待，尚未开工。生产仍为 Retained Dense。

## 2. 当前数据事实

> ⚠ **`123.stl` 这项资产已不可用**（2026-09-03 查证：仓库内无 STL、
> `E:\项目资料` 下亦未找到，且要求保存的可复现快照从未形成）。
> **以下数字仅可作容量规划参考，不可再引用为实测基线。**
> 替代基线见 `REPORT_16C_06_MEMFLOW_替代基线资产与调查结论_2026_09_03.md`，
> 8 个替代资产已入库并脱敏（`model/stl/suoguo-baseline/`）。

`123.stl` 的容量压力来自约 10.39～11.00 亿 pixel-layer 样本，而不是约 4.1 万三角形。六通道
Dense 输出本身约 5.8～6.1 GiB；历史非空占比约 3.06%。因此：

```text
Dense Streaming 首先解决峰值内存/分页；
Sparse Tile/Span 才进一步减少空白 CPU 扫描；
两者必须分开验证。
```

## 3. 已冻结边界

```text
协议、通道、极性、默认采样、姿态、材料和支撑语义不变；
Writer staging/strict validation/atomic publish 不放松；
小作业保留 Retained Dense；
Sparse 未过独立 Gate 前不自动选择；
设备 SLA 未提供，最终 production Gate 保持 INPUT_OPEN。
```

## 4. MF-01 实际结果

新增 `src/slicer_core/pipeline/RasterMemoryBudget.*`：

```text
route = retained_dense | bounded_dense_stream | blocked
预算未设置 -> retained_dense
预算内 -> retained_dense
超预算 + Bounded 能力可用 + 窗口在预算内 -> bounded_dense_stream
否则 -> blocked
```

所有 Raster 字节乘加均检查 `uint64_t` 溢出。`123.stl` 观测 Grid 的合同测试得到：

```text
pixelsPerLayer              7,369,346
retainedRgbwsvBytes         6,234,466,716
retainedRasterBytes 下界   10,390,777,860
boundedWindowBytes          221,080,380  （3 层、4 B/pixel Mask）
4 GiB 预算路由              bounded_dense_stream
```

该值是主要 Raster 下界，不含模型、纹理、allocator、JSON 和 TIFF 库，不能冒充实际 Peak Working
Set。

## 5. MF-02 / MF-03A / MF-03B1 实际结果

```text
MF-02  Legacy owned layer 同步背压；RGBWSV/semantic 在 producer 最后读取后移动
       const callback 保持兼容；双 callback、owned+写出、Global owned 均提前拒绝
       Adapter 取消/失败/异常清空部分层；非法状态稳定映射 Failed；独立用例 7/7 PASS

MF-03A primary/四 subsample compact ranges；caller-owned 单层 materializer
       DTO 构造后不可变并仅在 Build 时完整校验；Materialize 热路径不重复扫描全部 ranges
       S0/S3/S4 对独立 Retained 实现全层逐字节相等；阈值空洞不被 min/max 填平
       GeneralMesh 候选边界、层号、buffer、input kind 均 fail closed；257x32 生成对照 PASS

MF-03B1 move-only range-derived support demand；双 Mask caller-owned pre-shape materializer
         Bottom/Full/Upper/Unsupported 开闭区间和优先级与 retained oracle 全层零差异
         错误事实、别名、非二值 Mask 和 GeneralMesh fail closed；逐层热路径无分配
```

MF-02 只减少层交接复制，Adapter 仍保留完整 Raster；MF-03A/03B1/03B2 目前只建立能力，不接生产。因此实际
Peak Working Set 和 `123.stl` 总耗时尚未形成新 A/B 结论。

## 6. MF-03B2 实际结果

```text
P1 逐层 outer varnish / upper boundary 对独立 retained 公式全等；封闭孔不误判为 outer
P2 preliminary previous base 只含 model + Bottom/Full/Upper，明确排除 Unsupported/Shape/Base/Varnish
compact unsupported top、逐源层事件和 totals 对 dense retained oracle 全等
4/8 连通、严格 overlap/area 边界、层序、提前 Finish、错误尺寸/二值/别名/GeneralMesh fail closed
upper boundary 必须包含 model；错误调用不推进状态，可由同一层正确重试
P1/P2 ConsumeLayer 分配计数均为 0；新 scanner 未接生产路径
```

## 7. 验证状态

MF-03B3/B4A 当前结果：

```text
B1 final demand -> InternalVoid -> Shape -> type sync 顺序与 retained 对照全等
post-shape footprint 为全层 support OR；compact report 不保留 component pixels
SHA-256 replay digest 重复确定，并覆盖 upper-boundary 与 policy/report 域
mask-only ConsumeLayer 零分配并复用 caller/scratch buffer；固定 digest golden 冻结 canonical 编码
错误输入不推进状态；sink 异常不提交 caller output，并永久终止 scanner
新 scanner 仅由 support 模块自身和 Stage 16 测试引用，生产零接线
B4A 构造期绑定 B1 plan digest；逐层 digest 在任何 sink/caller output 提交前校验
Base disabled/overlay/prepend/clamp、model priority 与 outer-varnish 次序对 dense/fixed oracle 通过
final support/type totals 与 4/8 connectivity component area/bbox 通过同步 sink 交接
错误输入可重试；digest/sink 失败不提交当前层并永久终止；caller buffers 跨层地址不变
```

| Gate | 状态 |
|---|---|
| 文档和上下文互链 | PASS |
| MF-01 独立用例 | 8/8 PASS |
| MF-01 Release CTest | 1/1 PASS |
| MF-02 独立用例 / Release CTest | 7/7 / 1/1 PASS |
| MF-03A S0/S3/S4 等价与错误 Gate | PASS |
| MF-03B1 retained oracle / 错误与别名 Gate | PASS |
| MF-03B2 retained oracle / 状态机 / 零分配 Gate | 7/7 PASS |
| MF-02/03A/03B1/03B2/03B3/03B4A + outer varnish Release CTest | 7/7 PASS |
| MF-03B3 retained oracle / digest golden / scratch / 错误 Gate | 6/6 PASS |
| MF-03B4A/B 准备完整性审计 | PASS；B4A COMPLETE，B4B PREPARED |
| MF-03B4A verified final support replay | 10/10 PASS；COMPLETE |
| 既有 support shape / `slicer_cli` 链接 | 11/11 PASS / PASS |
| Router / Global 兼容定向测试 | 2/2 PASS |
| Scene Adapter 现有套件 | 11/12 PASS；修改前已存在的平移断言仍失败 |
| Dense Streaming 逐层 hash | **PASS**（四判据逐字节全等，digest 与长期基线一致） |
| 单实例 Package/RIP | **范围已重定义**：CLI 本就逐层写 TIFF，原子发布由 writer session 化覆盖 |
| 多实例 Barrier | **PASS**（MF-05 COMPLETE，峰值与层数脱钩） |
| Sparse candidate | **已跳过**（用户 2026-09-04 同意；MF-03X3 的稀疏列剪枝已取走其核心收益） |
| ~~`123.stl` 同请求 A/B~~ | **资产已不可用，该 Gate 作废**；替代基线 8/8 已采齐并入库 |
| 峰值预估器对实测点禁止低估 | **PASS**（四点全覆盖，大作业裕度 1.26~1.38x） |
| 全量回归 | 11 失败 / 229，全部为既有失败；其中 1 项是 MAX_PATH 边界抖动 |

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-06 | v1.8 | **信息同步**：本文档正文自 2026-08-21 起未随开发更新，与实际状态相差三周。新增 §0 现状摘要（两项原始阻塞均已实测解除、生产已切到 Bounded + 逐层流式），并明确历史正文保留不删但须先读 §0。按替代基线报告 §6.2 的建议，给 §2 的 `123.stl` 数字加上「资产已不可用、仅作容量参考」标注 —— 该建议自 2026-09-03 提出后一直未执行。Gate 表更新四项过时状态：多实例 Barrier 由 NOT STARTED 改为 PASS、Dense Streaming 逐层 hash 改为 PASS、Sparse candidate 标为已跳过、`123.stl` 同请求 A/B 因资产不可用而作废；并补入峰值预估器与全量回归两行。 |
| 2026-08-21 | v1.7 | MF-03B4A 完成：plan-bound verified support replay、Base/outer-varnish 最终化、compact connectivity 与错误/生命周期 Gate 通过；生产仍为 Retained Dense，B4B 未开工。 |
| 2026-08-21 | v1.6 | MF-03B4 准备完成并拆为 B4A/B4B；冻结支撑最终化与材料闭合边界。B4A 可开工，B4B 等待；生产仍为 Retained Dense。 |
| 2026-08-21 | v1.5 | MF-03B3 完成：非生产 InternalVoid/Shape/footprint/compact report/replay digest Gate 通过；生产仍为 Retained Dense。 |
| 2026-08-20 | v1.4 | MF-03B2 完成：bounded P1/P2 scanner、retained oracle、错误恢复和热路径零分配 Gate 通过；生产仍为 Retained Dense。 |
| 2026-08-19 | v1.3 | MF-03B1 完成：move-only 支撑需求计划、双 Mask caller-owned 单层物化、retained oracle 和 fail-closed Gate 通过；仍未接生产，B2..B4 继续部分准备。 |
| 2026-08-18 | v1.2 | MF-02/03A 完成：owned 层交接、取消/失败清理、Global fail-closed、compact occupancy range 和单层物化 Gate 通过；生产仍为 Retained Dense，MF-03B 仅部分准备。 |
| 2026-08-18 | v1.1 | MF-01 完成：预算/路由纯合同、123 大 Grid 精确下界、场景 RGBWSV/逐实例 Mask 分离计数、溢出和 fail-closed 用例通过；MF-02 转 PREPARED，未接入生产。 |
| 2026-08-18 | v1.0 | 建立专项状态报告；MF-00 完成，MF-01 启动，其余 Gate 保持未开始。 |
