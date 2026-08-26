# DOC_DECISION MATVOL-T 冻结契约 file_contract_v1 变更处置

> 文档状态：**待用户回签 / 尚未获得授权**
> 版本：v1.0 ｜ 日期：2026-08-26
> 撰写方：MATVOL 专项（代为留痕，本文内容未经 MATVOL-T 会话确认）
> 上级依据：`DOC_DECISION_14F_外部验证延期与接口冻结.md`

## 1. 为什么需要这份文件

MATVOL-T 修改了 `scripts/Run14F05StageClosureGate.ps1:138-151` 枚举的 12 个冻结契约文件中的 **3 个**：

- `contracts/file_contract_v1.request.schema.json`
- `contracts/file_contract_v1.result.schema.json`
- `contracts/file_contract_v1.contract_info.schema.json`

**该门禁只把这 12 个文件的 SHA-256 记录进证据，并不与任何固化基线比对**（全仓检索无比对逻辑，
仅在文件缺失时抛错）。因此这些修改**不会让门禁变红，会静默通过**。

门禁不拦，恰是必须留痕的理由，而不是可以略过的理由。

## 2. 逐项差异

### 2.1 `request.schema.json` — 加性，向后兼容

| 字段 | 变更前 | 变更后 |
|---|---|---|
| `minor` | `const 0` | `enum [0, 1]` |
| `kind` | 三值枚举 | 增加 `slice.rgbwsvt` |
| `output.contract` | `const "p0.rgbwsv.2"` | `enum ["p0.rgbwsv.2", "p0.rgbwsvt.1"]` |

并新增条件分支：`kind == slice.rgbwsv` 时 `minor` 仍钉死为 `0`。**旧请求逐字合法。**

### 2.2 `result.schema.json` — 加性，向后兼容

| 字段 | 变更前 | 变更后 |
|---|---|---|
| `minor` | `const 0` | `enum [0, 1]` |
| `capability` | 三值枚举 | 增加 `slice.rgbwsvt` |

并新增两条条件分支：`capability == slice.rgbwsvt` ⇒ `minor == 1`；
`capability ∈ {slice.rgbwsv, geometry.preflight.full, geometry.repair}` ⇒ `minor == 0`。
**旧结果逐字合法。**

### 2.3 `contract_info.schema.json` — **不是加性，是破坏性收紧**

这一份与前两份性质不同，包含**三处独立的收紧**：

| 字段 | 变更前 | 变更后 | 后果 |
|---|---|---|---|
| `minor` | `{"type":"integer","minimum":0}` | `{"const": 1}` | 声明 `minor: 0` 的 worker **校验失败** |
| `produces` | 必须包含 `p0.rgbwsv.2` | `allOf` 要求**同时**包含 `p0.rgbwsv.2` **与** `p0.rgbwsvt.1` | 只产出六通道的 worker **校验失败** |
| `capabilities` | items 限于三值 | `allOf` 要求**同时**包含 `slice.rgbwsv` **与** `slice.rgbwsvt` | 不支持 T 的 worker **校验失败** |

**净效果：`contract_info` 把「支持 T 通道」由可选变成了对每个 worker 的强制要求。**

## 3. 为什么这一处特别值得停下来看

### 3.1 与本专项自身的设计前提相矛盾

MATVOL-T 的双协议被设计为**二选一的 opt-in**：策略关闭走 `p0.rgbwsv.2` 六通道旧路径，
开启才产出 `p0.rgbwsvt.1`。`DOC_DECISION_MATVOL_T` 亦以「旧工艺文件逐字保留」为承诺。

而 `contract_info` 强制每个 worker 都必须声明 T 能力与 T 产物，**与 opt-in 前提直接冲突**：
在协议层面，T 已不再可选。

### 3.2 破坏被本仓自身的改动掩盖了

`apps/slicer_worker/WorkerApplication.cpp` 同批被改为：

```diff
- << "\"major\":1,\"minor\":0,"
- << "\"produces\":[\"p0.rgbwsv.2\"],"
+ << "\"major\":1,\"minor\":1,"
+ << "\"produces\":[\"p0.rgbwsv.2\",\"p0.rgbwsvt.1\"],"
```

即本仓 worker 已改为无条件宣称同时具备两种能力，**因此仓内校验照常通过**。
这使得该破坏在本仓的任何绿灯里都观察不到——绿灯不构成兼容性证据。

### 3.3 影响面触及对外交付

`contract_info.schema.json` 出现在 `scripts/Prepare14F02PrintM1Handoff.ps1:123`，
即**外置打印模块 M1 交付包**的组成部分。收紧后的该 schema 一旦随交付包外发，
对方据以做符合性校验时，其不支持 T 的 worker 将被判不合规。

同时 `product/legacy-slicer` 分支的 worker 亦不满足新约束。

## 4. 两个可选处置

**方案 A（建议）：把 `contract_info` 改回向后兼容，T 作为可选能力声明。**

- `minor` 放宽为 `enum [0, 1]`，与 request/result 一致；
- `produces` 恢复为只强制 `p0.rgbwsv.2`，`p0.rgbwsvt.1` 允许出现但不强制；
- `capabilities` 恢复为只强制 `slice.rgbwsv`，`slice.rgbwsvt` 加入 items 枚举但不强制。

如此，支持 T 的 worker 与不支持 T 的 worker **都合法**，与双协议 opt-in 前提一致，
对外交付面不变。代价是：宿主不能再从 schema 层面假定对端一定支持 T，需按 `capabilities`
实际内容协商——而这本就是能力协商应有的做法。

**方案 B：明示接受该破坏并回签。**

适用于「本产品线自此要求所有 worker 必须支持 T」的场景。若选此，需一并处置：
外置打印模块交付面的通知、`product/legacy-slicer` 的处置、以及 `DOC_DECISION_14F` 冻结承诺的修订。

## 5. 建议

**建议取方案 A。** 理由不是保守，而是**该收紧与 MATVOL-T 自己的 opt-in 设计前提相矛盾**；
在 opt-in 尚未被推翻的前提下，协议层强制 T 属于设计不自洽，而非有意的产品决策。
前两份 schema（request / result）的加性放宽则建议**直接批准**，它们与 opt-in 前提一致。

## 6. 待回签事项

| 编号 | 事项 | 状态 |
|---|---|---|
| FC-01 | `request.schema.json` 加性放宽 | 待回签（建议批准） |
| FC-02 | `result.schema.json` 加性放宽 | 待回签（建议批准） |
| FC-03 | `contract_info.schema.json` 破坏性收紧 | **待裁定：方案 A 或 B** |
| FC-04 | 三条「不增加第七通道」红线的引用与处置 | 待补（见 `DOC_ANALYSIS_MATVOL_T_合并对接面与冲突清单.md` §4） |

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.0 | 建立。核实三份冻结契约的逐项差异；发现 `contract_info` 并非加性而是三处独立收紧，使 T 由可选变为强制，与本专项 opt-in 前提矛盾；记录该破坏被同批 worker 改动掩盖、且影响面触及 M1 对外交付包；给出方案 A/B 与建议。 |
