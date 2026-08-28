# DOC_DECISION MATVOL-T 冻结契约 file_contract_v1 变更处置

> 文档状态：**已裁定（用户 2026-08-26 授权由本会话决定，取方案 A）/ 已实施 / 已收口至打包门禁与协议正文**
> 版本：v1.2 ｜ 日期：2026-08-26
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
| FC-01 | `request.schema.json` 加性放宽 | **已批准并保留** |
| FC-02 | `result.schema.json` 加性放宽 | **已批准并保留** |
| FC-03 | `contract_info.schema.json` 破坏性收紧 | **已取方案 A 并实施，见 §8** |
| FC-04 | 三条「不增加第七通道」红线的引用与处置 | 待补（见 `DOC_ANALYSIS_MATVOL_T_合并对接面与冲突清单.md` §4） |
| FC-05 | 方案 A 未落到打包门禁与协议正文的两处残留 | **已批准并实施，见 §9** |

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.2 | 收口 FC-05：方案 A 未落到 `PrepareSliceSoftRuntime.ps1` 打包门禁与 `file_contract_v1.md` 协议正文两处，前者停留在 T 之前的单通道基线并使 Runtime 部署中止，后者停留在方案 B 的「必须双声明」口径；已分别改为「基线必含 + 白名单 + 跨组件一致性」与「T 为可选加性声明」，并在 `runtime_manifest.json` 增 `fileContract` 留痕。脚本改动未实跑。 |
| 2026-08-26 | v1.0 | 建立。核实三份冻结契约的逐项差异；发现 `contract_info` 并非加性而是三处独立收紧，使 T 由可选变为强制，与本专项 opt-in 前提矛盾；记录该破坏被同批 worker 改动掩盖、且影响面触及 M1 对外交付包；给出方案 A/B 与建议。 |

## 8. 裁定与实施（v1.1）

用户 2026-08-26 授权由本会话决定，并说明「后续协议要升级支持 T 通道生成」。**取方案 A。**

**理由**：方案 A 并不妨碍将来升级——它只是不在**现在**强制。当下强制的代价是
`Prepare14F02PrintM1Handoff` 的对外交付面与 `product/legacy-slicer` 双双失效，
却换不到任何东西：现有功能没有一处需要 `contract_info` 强制 T。
将来产品线真要全面要求 T，再收紧一次成本很低；而把已外发的破坏收回来成本很高。

**实施内容**：

- `minor`：`const 1` → `{"type":"integer","enum":[0,1]}`，与 request / result 口径一致；
- `produces`：恢复为只强制 `p0.rgbwsv.2`，`p0.rgbwsvt.1` 允许出现但不强制；
- `capabilities`：恢复为只强制非空且 items 受限，`slice.rgbwsvt` 加入 items 枚举但不强制；
- 删除引入强制的整个 `allOf` 块。

相对**变更前**（`90a59ba`），本文件的净差异只剩两处纯加性内容：
capabilities 的 items 枚举增加 `slice.rgbwsvt`；`minor` 由 `minimum: 0` 改为 `enum [0,1]`
（后者是把无上界收窄到实际存在的两个取值，无任何真实 worker 受影响）。

**测试同步**：`tests/contracts/ValidateFileContract.py` 中三条「缺 T 即非法」的负例
（`missingTransferProduce`、`missingTransferCapability`、`staleInfo`）在方案 A 下不再成立。
已**转为正例**而非删除——它们正是本方案要保住的向后兼容性，转正例才能把该性质钉死。
`ValidateFileContract.py` 实测 PASS。

**FC-04（第七通道红线的引用与处置）仍未处置**，留待后续。

## 9. 方案 A 的两处残留与收口（v1.2）

用户 2026-08-26 在打包失败排查中要求补此留痕。**FC-05 已批准并实施。**

### 9.1 触发现象

`scripts/PrepareSliceSoftRuntime.ps1 -DeployOnly -Config Release` 抛
`Deployed worker discovery contract or version drifted.`（原 1196 行）而中止，Debug 同样会中止。
这不是编译错误，也与 VisualStudio / Release 配置无关。

### 9.2 定性：方案 A 落地不完整，不是新的门禁放宽

`b5e6073` 把 `contract_info.schema.json` 改回向后兼容时，**只改了 schema 与其直接用例**，
两处同样表达该协议的位置没有跟着改：

| 残留位置 | 残留内容 | 与方案 A 的关系 |
|---|---|---|
| `scripts/PrepareSliceSoftRuntime.ps1:1181-1224` | 门禁写死 `minor -ne 0`、`produces.Count -ne 1`、`produces[0] -ne "p0.rgbwsv.2"`；module-info 侧同样写死 `Count -ne 1` | 停留在 **T 之前**的单通道基线，连方案 A 都不到 |
| `contracts/file_contract_v1.md:37-40` | 「It must validate against … **and declare both package contracts and both slice capabilities**」 | 停留在 **方案 B** 的强制口径 |

两处方向相反，但同属一类：`8a9fb95` 改了 worker 的 `--contract-info` 实际输出
（`minor` 0→1、`produces` 增 `p0.rgbwsvt.1`），而表达该协议的其它位置各自滞后。
这与 `4f697cc` 已修的三处用例是**同一类缺陷**——把「新增能力」误判为「冻结漂移」。
`ValidateVersionContracts.py` 当时已按包含式断言修正，本次是同一批中被遗漏的最后一处。

因此 FC-05 是**把方案 A 补齐到未覆盖的位置**，不是对已冻结门禁的新一次放宽。

### 9.3 实施内容

**`scripts/PrepareSliceSoftRuntime.ps1`** —— 由「逐字相等」改为「基线必含 + 白名单 + 一致性」，
净效果是三收一放：

- `produces` 必须包含 `p0.rgbwsv.2`（基线不可丢，与方案 A 同口径）；
- `produces` **只允许** `p0.rgbwsv.2` / `p0.rgbwsvt.1`，且不得重复——**新增的收紧**，
  未知生产合同仍判漂移；
- 声明 `p0.rgbwsvt.1` 时 `minor` 必须 ≥ 1，未声明时保持旧基线要求——**新增的收紧**；
- module-info 侧同样白名单化，每项 `kind` 必须为 `package`，并**新增**
  「模块与 worker 的 T 通道声明必须一致」的跨组件校验，防止只改一边的半截漂移；
- 放开的只有一条：不再要求 `produces` 恰好一项、`minor` 恰好为 0。

该门禁只校验**本仓自己部署出来的** worker/module，不是对第三方 worker 的互操作校验，
故其白名单严于 `contract_info.schema.json` 是有意为之：schema 定互操作下限，
打包门禁定本产品线的出厂身份。二者不冲突。

**留痕**：`runtime_manifest.json` 的 `capabilityPackage` 下新增 `fileContract`
（`major` / `minor` / `produces` / `transferChannel`），每次部署把协商到的合同集写进产物清单，
使「这份 Runtime 是几通道的」可从产物本身读出，不必回查脚本版本。

**`contracts/file_contract_v1.md` §2** —— 删除「must declare both」的方案 B 口径，改为：
`slice.rgbwsvt` / `p0.rgbwsvt.1` 是**可选的加性声明**；两者都不声明的 worker 仍是合规的
`file_contract_v1` worker，发现阶段不得拒绝，只在 `slice.rgbwsvt` 作业的能力检查处失败；
本仓 worker 声明双合同四能力，是**一个合规实例，不是合同下限**。

该文件在 `Prepare14F02PrintM1Handoff.ps1:122` 随 **M1 交付包外发**（它不在
`Run14F05StageClosureGate.ps1` 的 12 个固化 hash 之列，因此这处残留同样不会让门禁变红）。
本次修正的方向与 §3.3 的风险一致：删去的正是那句会让对方不支持 T 的 worker 被判不合规的话。

### 9.4 一并核对、确认无需改动的位置

| 位置 | 结论 |
|---|---|
| `scripts/TestSlicerModulePackage.ps1:211-222` | 只校验 `contract` / `major` / `engineVersion`，本就宽松 |
| `tests/stage14d_07/RunEngineConformance.py:65-71` | 已是包含式 + 子集式 |
| `tests/version/ValidateVersionContracts.py:185-190` | `4f697cc` 已修 |
| `src/slicer_module/WorkerContract.h` 的 `WorkerContractRequirement` 默认值 | `minor=0`、`requiredProduces={p0.rgbwsv.2}`，与方案 A 一致 |
| `INT_10` §3.3 / `INT_16` §3.3 / `DEV_14` / `DOC_DECISION_14` | 规则本身写的就是「produces 仍含 p0.rgbwsv.2」的包含式；`INT_16` 的示例 JSON 虽为 `minor:0` 单产物，但在方案 A 下**仍是合法返回**，不构成失同步。且属对接双方共识文档，不在本次单方修订范围 |

### 9.5 未完成事项

本次**未能实跑**验证：重跑打包脚本被当前会话的权限策略拦截，故 §9.3 的脚本改动只经静态审查，
`-DeployOnly` 的实跑结论由用户侧补。`contracts/file_contract_v1.md` 为文档修改，无需执行验证。
**FC-04 仍未处置。**
