# DOC_ANALYSIS MATVOL-T 部分合并可行性评估

> 文档状态：**可行性分析 / 未执行任何合并操作**
> 版本：v1.0 ｜ 日期：2026-08-26
> 提问：`codex/matvol-t-channel-protocol` 尚未完成全部任务（当前 T-08 阶段），
> 能否先做部分合并？
> 配套件：`DOC_ANALYSIS_MATVOL_T_合并对接面与冲突清单.md`（v1.1）

## 0. 结论速览

| 问题 | 结论 |
|---|---|
| 现在能否做 git 意义上的部分合并？ | **不能**，因为该分支领先 0 个提交，没有可合并的对象 |
| 未完成是否构成阻碍？ | **不构成**。缺的是验证与回签，不是实现 |
| 该按层切片合并吗？ | **不该**。全量冲突面仅 2 处，切片的成本远高于收益 |
| 合并前必须补什么？ | 3 项前置条件，见 §4 |
| 当前最紧急的事 | **让对方先提交**——102 个文件裸奔在工作树里 |

## 1. 决定性事实：该分支领先 0 个提交

```
git rev-list --count product/packaged-slicer..codex/matvol-t-channel-protocol  →  0
git worktree list  →  slice_soft_demo_matvol_t  90a59ba [codex/matvol-t-channel-protocol]
```

分支 HEAD 就是两者的 merge-base `90a59ba` 本身。T-00~T-08 的**全部成果只存在于未提交的工作树**：75 个修改 + 27 个新增，共 102 个文件。

由此直接得出两个结论：

**其一，`git merge` / `git cherry-pick` / `git rebase` 全部无从谈起。** 「部分合并」在 git 语义下需要挑选提交，而这里一个提交都没有。当前唯一可行的形式是按路径导出补丁（`git diff -- <paths>`）再套用，那是**按文件挑选**，不是按提交挑选。

**其二，这是一个高风险状态，且与合并无关地紧急。** 未提交的改动没有 reflog 保护。一次 `git checkout`、`git reset --hard`、`git stash` 误用或工具崩溃，T-00~T-07 七张已 COMPLETE 的卡即全部灭失。**无论是否合并，都应先让对方把工作提交成若干逻辑提交（建议按 T-01~T-08 分卡）。** 这一步同时也是使部分合并成为可能的前提。

## 2. 实测冲突面：只有 2 处接触点

以 `90a59ba` 为共同基准，逐 hunk 求行区间交集（Git 默认 3 行上下文，间距 ≤ 6 视为同一冲突块）：

| 文件 | 判定 | 说明 |
|---|---|---|
| `src/slicer_core/slicer.cpp` | **硬冲突 ×1** | 见 §2.1 |
| `src/slicer_core/config.cpp` | **邻接 ×1** | 见 §2.2 |
| `src/slicer_core/config.h` | 无交集 | 我方 208 区，对方 19 / 220 / 358 / 409 |
| `src/slicer_core/slicer.h` | 无交集 | 我方 90 区，对方 78 / 83 |
| `src/slicer_core/materials/volume/MaterialVolumePlan.cpp` | 无交集 | 我方 160–170，对方 138 / 183 |
| `apps/slicer_ui_host_sim/HostPackageReviewPanel.cpp` | 无交集 | 我方 169 / 179 / 482，对方 24 / 91 / 187 / 287 / 324 / 471 |

其余 96 个文件两侧不重叠。注意 `contracts/slicesoft.material_volume_report.1.schema.json` 与
`src/slicer_core/reports/MaterialVolumeReport.cpp` **不在冲突面内**——MATVOL 在 `90a59ba`
之后未再触碰它们，对方的改动可直接落地。

### 2.1 唯一的硬冲突（`slicer.cpp` 4458–4470）

对方把局部的 `MaterialVolumeGrid` 构造整段上提并改名，删除 4458–4466 九行，并把
`matvolRequest.grid = matvolGrid;` 改为 `= materialVolumeGrid;`。

MATVOL 则在紧邻的 4470 之后插入一行取消点透传：

```cpp
matvolRequest.cancellationRequested = options.cancellationRequested;
```

两者语义完全独立。**解法**：采用对方的上提与改名，同时保留 MATVOL 这一行。属分钟级手工处置。

### 2.2 唯一的邻接（`config.cpp` 462 / 464）

MATVOL 在 462 行的 `topology` 块内新增 `maxBoundaryEdges` 读取；对方在 464 行、即该块闭合之后新增
`LoadTransferChannelPolicy(root, config.transfer_channel_policy);`。两者顺序相邻但不重叠，**保留双方、维持先后即可**。

## 3. 「未完成」是否构成阻碍：不构成

任务卡实况：

| 卡 | 状态 |
|---|---|
| T-00 ~ T-07 | 全部 **COMPLETE** |
| T-08 生产矩阵与准入验证 | **PREPARED** |
| T-09 用户生产 opt-in 回签与收口 | INPUT OPEN |

**缺的是验证与回签，不是实现。** 且 T-08 的产物实际已经存在——`tests/matvol_t/MatvolTProductionMatrixTests.cpp`
与 `scripts/run_matvol_t_t08_gate.ps1`（208 行）都已写好，只是尚未标记完成。`tests/matvol_t/` 下共 11 个测试文件。

暴露面方面：双协议设计为**二选一的 opt-in**——策略关闭时走 `p0.rgbwsv.2` 六通道旧路径，策略开启才产出
`p0.rgbwsvt.1` 七通道，且旧工艺文件逐字保留、每个 T 化工艺另出 `_rgbwsvt` 副本。**理论上对既有生产路径零暴露**。

但「理论上」不能作为合并依据。该性质恰恰应由 T-08 矩阵里的「无 T 路径零漂移」用例来证明，故 §4(b) 把它列为前置条件。

## 4. 合并前必须补的三项

### (a) 冻结契约的授权留痕（最该补的一项）

对方修改了冻结面枚举 12 个文件中的 **3 个**：

- `contracts/file_contract_v1.request.schema.json`
- `contracts/file_contract_v1.result.schema.json`
- `contracts/file_contract_v1.contract_info.schema.json`

改动本身是**加性**的，旧值全部仍然合法：`minor` 由 `const 0` 放宽为 `enum [0, 1]`；
`kind` 增加 `slice.rgbwsvt`；`output.contract` 由 `const "p0.rgbwsv.2"` 放宽为
`enum ["p0.rgbwsv.2", "p0.rgbwsvt.1"]`。

**关键事实：`scripts/Run14F05StageClosureGate.ps1` 只把这 12 个文件的 SHA-256 记录进证据，
并不与任何固化基线比对**（全仓检索无比对逻辑，仅在文件缺失时抛错）。因此这些改动**不会让门禁变红**。

这不是可以略过的理由，而是必须补文档的理由：**门禁不拦，就意味着改动会静默通过**。按本仓既有惯例
（MATVOL 自身的 MQ-05、MQ-06 放宽均先出授权文档再改代码），此处应补一份授权件，写明放宽范围、
向后兼容性论证与复核时点。同类问题另见 `file_contract_v1.md` 与三条「不增加第七通道」红线
（详见配套件 §4）。

### (b) T-08 至少跑一次并留证

重点是**「无 T」路径的零漂移**用例。那是「合并不影响现有生产」的唯一凭据，也是 §3 中
「理论上零暴露」由推断转为事实的唯一途径。

### (c) 补 `maxBoundaryEdges` 与黄色 Kd

配套件 §3 已详列。不补则 `08/09` 在 T 路径上仍被拒（缺 `maxBoundaryEdges`），
且缩裹识别在 `08/09` 上不命中（`materialDiffuseRgbValues` 缺 `[255,255,0]`）。

## 5. 为何建议全量合并而非分层切片

设想的切片方式是「先合契约 + 核心层，暂不合宿主/UI 层」。该方案不划算：

- **构建不自洽**：`CMakeLists.txt` 的新源文件注册必须与源文件同批进入，否则要么少文件、要么少注册。
- **层间强耦合**：宿主层 17 改 + 8 新负责发射 profile，核心层负责解析。只取一侧会得到一个
  「能编译但无法端到端验证」的中间态——既不能跑 T-08，也不能跑你的 03/08/09 实测。
- **收益为零**：分层切片的目的是规避冲突，但**全量冲突面本来就只有 1 处硬冲突 + 1 处邻接**。
  为规避 2 处冲突去设计一个能自洽编译的子集，是拿大成本换小成本。

⇒ **一旦对方完成提交，直接全量合并，代价低于任何切片方案。**

## 6. 建议次序

1. **对方把 102 个文件提交成若干逻辑提交**（按 T-01~T-08 分卡）。此步兼具止损与使合并可行两重作用。
2. 对方补 §4(a) 冻结契约授权件。
3. 对方跑一次 T-08 门禁并留证，重点是无 T 零漂移。
4. 对方补 §4(c) 的 `maxBoundaryEdges` 与黄色 Kd。
5. **由 T 向 `product/packaged-slicer` 全量合并**。MATVOL 侧负责处置 §2.1 / §2.2 两处接触点，
   并复核四项语义：取消点透传是否保留、`warnings` 披露是否保留、`maxBoundaryEdges` 是否生效、
   `08/09` 在两条路径上是否一致。
6. 合并后跑 MATVOL 回归（基线：216 项 / 既有失败 7 项，零新增）与 `03/08/09` 生产实设实测。
7. **逐模型工艺指定**留到最后单独立卡——它与对方 T-06 已改动的 Scene/Worker/Host 透传链路重合。

## 7. 本次分析的操作边界

按用户要求「先不操作」，本次**未执行任何合并、未创建分支、未修改任何一侧的代码**。
所有结论均由只读检查得出（`git rev-list`、`git diff -U0` 的 hunk 区间求交、门禁脚本审读、任务卡审读）。

唯一的写操作是更正配套件 `DOC_ANALYSIS_MATVOL_T_合并对接面与冲突清单.md` §2.2 的一处错误结论
（v1.0 曾预测 `MaterialVolumePlan.cpp` 高冲突风险，实测互不相交），已随本次一并转 v1.1。

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.0 | 建立评估。核心发现：该分支领先 0 提交致 git 级部分合并不可行且工作处于无保护状态；实测全量冲突面仅 1 硬冲突 + 1 邻接；未完成的是验证与回签而非实现；冻结面门禁只记录不比对故需补授权留痕；建议全量合并而非分层切片。 |
