# INT_17 base/engine 改造计划与设计同步台账

> 目录：`docs/claude/INTEGRATION/`。日期：2026-08-03。
> 回答：① 新模型是否已同步到全部设计（同步台账）；② 按新模型的改造计划；③ Stage 14 文件是否齐备、能否开工。
> 证据等级：A=已核实事实，P=本方计划。

---

## 第一部分：设计同步台账

## 1. 新模型的五条变更点

```text
V1 Worker 定位：DLL 承载壳 → 【可独立迭代替换的切片引擎】
V2 核心库：单一 slicer_core → 【slicer_base（DLL+Worker）+ slicer_engine（仅 Worker）】
V3 切片路径：可选进程内/子进程 → 【只在 Worker，取消 backend=inprocess】
V4 版本约束：必须同一次构建 → 【file_contract_v1 major.minor 启动协商】
V5 一致性验证：双后端 SHA-256 比对 → 【引擎一致性套件 E-01..08】
```

## 2. 逐文档同步状态

| 文档 | 受影响内容 | 状态 |
|---|---|---|
| `INT_16` | 新模型来源文档 | ✅ 权威 |
| `INT_10` §标题栏 | 加 v1.1 修订横幅 | ✅ 已改 |
| `INT_10` §1.1 | 产物表拆为 base/engine；撤销"同一次构建"；补拆分理由 | ✅ 已改 |
| `INT_10` §3.3 | 版本绑定 → 契约协商 + 可替换三条件 | ✅ 已改 |
| `INT_10` §3.5 | 取消双后端 SHA 比对；改为 verify 自检 + E-03；补 Worker 独立调试 | ✅ 已改 |
| `INT_10` §4 | 归属表按 base/engine 重判（含 `system/`、`third_party/`、`tests/` 补全）| ✅ 已改 |
| `INT_07` §2.2 | CMake 目标改四目标；DLL 只链 base；移除 InProcessBackend | ✅ 已改 |
| `README` 术语块 | 产物与交付边界改为 base/engine 版 | ✅ 已改 |
| `DOC_DECISION_14` §3 | 三层结构 → 四目标 + 五条边界规则 + 三契约版本轴 | ✅ 已改 |
| `DOC_DECISION_14` §5 | G-7 改为引擎一致性套件；新增 G-9 单向依赖 | ✅ 已改 |
| `DOC_DECISION_14` §5.1 | 记录 D-1/D-2/D-3 三项裁定 | ✅ 已改 |
| `TASKS_14` 14B | 新增 14B-00 分层可行性、14B-01A 分层落地 | ✅ 已改 |
| `TASKS_14` 14D | 03 改契约协商；06 改取消 inprocess；07 改引擎一致性套件；08 新增独立调试入口 | ✅ 已改 |
| **`INT_02` §8** | 承载策略表仍写 `backend: auto/inprocess/subprocess` | 🔶 见 §3 定点修订 |
| **`INT_06` §2.4/§6** | `slice.rgbwsv` 承载描述、M1 交付物"不含 core" | 🔶 见 §3 |
| **`INT_09` §3** | v2 能力清单的"承载"列 | 🔶 见 §3 |
| **`INT_12` §2.3** | v0.1 交付物写"内含静态链接的 core" | 🔶 见 §3 |
| **`INT_13` §8.1** | 产物裁定表为 v1.0 三层版 | 🔶 见 §3 |
| **`INT_15` §3.3** | 结论"当前不能独立替换" | 🔶 见 §3 |
| **`CLAUDE_13` §1.4** | "DLL 门面 + EXE 工人"+`backend=auto` | 🔶 见 §3 |
| **`CLAUDE_09` §3.3** | 目标目录结构未含 base/engine 分层 | 🔶 见 §3 |

## 3. 定点修订说明（统一口径，避免逐篇重写）

以下六处**表述仍为 v1.0**，但均已被本篇与 `INT_16` 取代。为保持"单一差异真源"原则（与 `VERIFICATION/CLAUDE_08` 同法），**不逐篇重写正文**，改为统一声明：

```text
凡我方文档中出现以下 v1.0 表述，一律以 INT_16 / INT_10 v1.1 为准：

× "slicer_core 静态链接进 DLL 与 Worker 各一份"
  → ✅ slicer_base 链入两者；slicer_engine 仅链入 Worker

× "DLL 与 Worker 必须来自同一次构建/成对替换"
  → ✅ file_contract_v1 major.minor 启动协商；Worker 可独立替换

× "backend = auto | inprocess | subprocess"
  → ✅ 切片只在 Worker；轻量能力天然在 DLL 进程内，无需 backend 选项

× "双后端产物 SHA-256 比对"
  → ✅ 引擎一致性套件 E-01..08（E-03 为 golden checksum 比对）

× "Worker 是 DLL 的私有实现细节，不可独立替换"
  → ✅ 协议仍为内部实现（宿主不直连），但 Worker 本身可独立替换

× "交付物：DLL + Worker（各内含 core）"
  → ✅ 交付物不变（DLL + Worker），但内含物改为 base / base+engine
```

**执行约定**：后续任何一篇被实际编辑时，顺手把该篇的 v1.0 表述改掉；不为同步而单独开 PR。

## 4. 一致性自检：新模型是否引入新矛盾

我逐条检查了新模型可能与既有设计冲突的地方（P）：

| 检查项 | 结论 |
|---|---|
| `geometry.preflight` fast 在 DLL、full 在 Worker，会不会判定不一致？ | ⚠️ **需一条新不变量**：见 §4.1 |
| `package.render_layer_preview` 需要 tiff 读取，base 够吗？ | ✅ 够。读取侧在 base，写入侧才在 engine |
| `model.import` 若拆不进 base 怎么办？ | ⚠️ 已列为 14B-00 待验证，退路是改 Worker 承载（`INT_10` §4 已注）|
| 取消 `backend=inprocess` 后，`INT_05` 的双后端测试用例失效？ | ✅ 应删除；改为引擎一致性套件用例 |
| 三车道的 Commit 在 DLL 进程内，会不会需要 engine？ | ✅ 不会。变换求值/碰撞/越界全在 base |
| `ICancelToken` 在 base 侧 facade 也要吗？ | ✅ 要。`PreflightFacade::RunFast` 虽快但可能遇大模型，统一带 token 更安全 |
| Worker 独立替换后，`profileEcho` 追溯是否够？ | ⚠️ **需补 `engineVersion` 回写**（`INT_16` §5.2 已含）|

### 4.1 新增不变量：权威判定必须发生在 Worker 内（重要）

这是 base/engine 分层引出的**唯一真正的新风险**（P）：

```text
问题：DLL 的 fast preflight 与 Worker 的 full preflight 来自【可能不同版本】的代码
      （base 版本随 DLL，engine 版本随 Worker）
      若 fast 说"通过"而 full 说"阻断"，用户会看到"点了切片才被拒"

不变量（必须写入契约）：
  ① fast preflight 结果【只是提示】，不构成准入结论
  ② 任何进入生产的准入判定【必须】在 Worker 内重新执行 full preflight
  ③ fast 只允许漏报（说通过但其实阻断），不允许误报为阻断
     —— 保证 UI 不会错误地阻止用户
  ④ DLL 返回 fast 结果时必须标注 `authoritative: false`
```

→ 已作为 `TASKS_14` 的 14A-04 契约字段要求补入（见 §6）。

---

## 第二部分：base/engine 改造计划

## 5. 改造总览

```mermaid
flowchart LR
  P0["P0 分层可行性验证<br/>14B-00"] --> P1["P1 建立两库骨架<br/>14B-01A"]
  P1 --> P2["P2 迁移 base 内容"]
  P2 --> P3["P3 迁移 engine 内容"]
  P3 --> P4["P4 CI 单向依赖门禁"]
  P4 --> P5["P5 DLL 只链 base 验证"]
  classDef k fill:#fff3cd,stroke:#d90
  class P0,P4 k
```

**总原则（沿用绞杀式纪律）**：

```text
① 行为零漂移：每步以"生产 TIFF 逐字节不变 + RIP strict"为硬门
② 先建骨架再迁移：两库先空建 + 接线通过，再逐目录搬
③ 一步一回退：每个目录迁移独立提交
④ 单向依赖：每步后跑依赖检查，禁止 base → engine
```

## 6. 分步计划

### P0 分层可行性验证（14B-00，**先做，决定后续路径**）

| 项 | 内容 |
|---|---|
| 目标 | 判定每个目录能否干净归入 base 或 engine；重点验证三个高风险项 |
| 高风险项 | ① `model.cpp`(1970 行) 与几何耦合；② `geometry/` 需按"查询 vs 分析"切开；③ `reports/`/`diagnostics/` 需按"读取展示 vs 切片期生成"切开 |
| 方法 | 用包含关系分析（谁 include 谁）+ 符号引用分析，画出目录级依赖图；标出跨界引用 |
| 出口 | 出结论文档：每目录归属 + 跨界引用清单 + 不可拆项的退路方案 |
| 估算 | **2–3 人日** |
| 退路 | 若 `model.cpp` 拆不动 → `model.import` 改 Worker 承载（接受导入的进程往返延迟）|

### P1 建立两库骨架（14B-01A）

```cmake
# 骨架阶段：base 先空，engine 承接现有全部 core 源文件
add_library(slicer_base STATIC ${SLICER_BASE_SRCS})     # 初始为空/极少
add_library(slicer_engine STATIC ${SLICER_ENGINE_SRCS}) # 初始 = 现有全部 core
target_link_libraries(slicer_engine PUBLIC slicer_base) # 单向：engine → base
# 现有消费者（slicer_cli / tests / UI）暂时链 slicer_engine，行为不变
```

出口：全量构建通过 + 全测试绿 + TIFF 逐字节不变（此步**零逻辑改动**，只改构建图）。估算 1–2 人日。

### P2 迁移 base 内容（按风险从低到高）

| 序 | 迁移目录 | 风险 | 出口门 |
|---:|---|:--:|---|
| 1 | `system/` · `config/` · `config.*` | 低 | 构建 + 全测试 |
| 2 | `scene/` · `layout/` | 低 | 场景/排版单测 |
| 3 | `output/`（读取侧：`rip_reader`）· `third_party/miniz` | 低 | `rip_reader_test` 通过 |
| 4 | `preview/` | 中 | 13C 预览用例 |
| 5 | `geometry/`（查询子集：bbox / 点在网格内 / 基础拓扑体检）| **中高** | 需先在 P0 明确切分线 |
| 6 | `importers/` · `model.*` | **高** | 依 P0 结论；不可拆则跳过 |

每步：迁移 → 构建 → 全测试 → 依赖检查 → 提交。估算 **5–8 人日**。

### P3 迁移 engine 内容

剩余全部归 engine（`slicer.cpp` / `steps/` / `materials/` / `material/` / `support/` / `raster/` / `geometry/repair/` / `output/` 写入侧 / `pipeline/`）。因 P1 已把它们放在 engine，**本步实际是"确认没有遗留 base 内容"** + 处理 P2 暴露的跨界引用。估算 2–3 人日。

### P4 CI 单向依赖门禁（**关键，防回退**）

```text
检查一：slicer_base 的编译单元不得 include engine 侧头文件
检查二：slicer_module 的链接闭包中不得出现 engine 符号
        （dumpbin /SYMBOLS 或 link map 分析）
检查三：新增源文件必须显式归入 base 或 engine，不允许游离
违反即 CI 失败
```

估算 1–2 人日。**这一步不做，分层会在几个月内退化回单库。**

### P5 DLL 只链 base 验证

```text
构建 slicer_module.dll，target_link_libraries 只写 slicer_base
若链接失败 → 说明某轻量能力实际需要 engine → 回到 §3.1 规则③：
  该能力必须改为 Worker 承载，不得把 engine 拉进 DLL
```

出口：DLL 成功链接且只依赖 base；`dumpbin /DEPENDENTS` 无 Qt / PrintSDK（C-SPI-17）。估算 1–2 人日。

### 6.1 改造总估算

| 步 | 人日 |
|---|---:|
| P0 可行性验证 | 2–3 |
| P1 两库骨架 | 1–2 |
| P2 迁移 base | 5–8 |
| P3 迁移 engine | 2–3 |
| P4 CI 门禁 | 1–2 |
| P5 DLL 验证 | 1–2 |
| **合计** | **12–20 人日** |

> 这 12–20 人日**包含在** `INT_12` 中期计划的 M-B 步骤化预算内，不是新增成本——因为分层与步骤化触及同一批文件，应合并推进。

---

## 第三部分：Stage 14 文件齐备度与开工判断

## 7. 已创建 / 缺失清单

| 文档类型 | 路径 | 状态 |
|---|---|:--:|
| 决策 | `docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md` | ✅ **已建**（v1.1）|
| 任务清单 | `docs/codex_task/current/TASKS_14_切片能力包封装与打印软件集成任务清单.md` | ✅ **已建**（v1.0，40+ 卡）|
| 执行指令 | `docs/codex_task/current/CODEX_PROMPT_14_切片能力包封装与打印软件集成执行指令.md` | ✅ **已建**（v1.0）|
| 需求 | `docs/slice/PRD/PRD_14_切片能力包封装与打印软件集成.md` | ✅ **已建**（v1.0，12 FR / 9 NFR / 9 AC）|
| 设计 | `docs/slice/DEV/DEV_14_切片能力包封装与打印软件集成.md` | ✅ **已建**（v1.0）|
| 验收 | `docs/slice/DEMO/DEMO_14_切片能力包封装与打印软件集成验收方案.md` | ✅ **已建**（v1.0，62 用例）|
| 状态 | `docs/slice/REPORT/REPORT_14_切片能力包封装与打印软件集成准备状态.md` | ✅ **已建**（v1.0）|
| 路线 | `docs/slice/ROADMAP/ROADMAP_14_*.md` | ⏸ 可选（决策 §6 已含 14A–14F 阶段划分，暂不单立）|
| 主状态登记 | `REPORT_12X` 补 Stage 14 行 | ❌ 缺（= 14A-09 顺带办）|
| 契约物料 | `contracts/`（4 份）| ❌ 缺（= 14A-01..03 的产出，属实现范畴）|

**齐备度：7 / 7 必需文档 ✅（2026-08-03 补齐）**。文档准入完成，`DOCUMENTATION_GATE = PASS`；`IMPLEMENTATION_GATE` 仍为 `NOT_AUTHORIZED`。

## 8. 能否开启准备工作？—— **可以，但分两类**

### 8.1 现在就能启动的（不受文档齐备度阻塞）

这些任务**本身就是"补齐准备"**，不需要等 PRD/DEV/DEMO：

| 卡 | 任务 | 理由 | 估算 |
|---|---|---|---:|
| **14B-06** | CI 行数门禁 G1..G5 | 独立止血项，不依赖任何契约 | 2–3 人日 |
| **14A-01** | `contracts/` + `print_module_spi.h` 落盘 | 打印侧 `CLD_10` 已定稿，我方只同步 | 1 人日 |
| **14A-02** | `p0.rgbwsv.2` JSON Schema | 协议已冻结，用真实 manifest 校验 | 1–2 人日 |
| **14A-07** | 第三方依赖再分发合规（assimp/miniz/libtiff）| 纯审查，无代码 | 1 人日 |
| **14A-08** | 对 RIP 六项确认清单发出 | 清单已写好（`INT_16` §10），只需发出 | 0.5 人日 |
| **14A-09** | `REPORT_12X` 补 03E 行 | 事实补录 | 0.2 人日 |
| **14B-00** | base/engine 分层可行性验证 | 纯分析，产出结论文档 | 2–3 人日 |

**合计约 8–12 人日的准备工作可立即启动**，且全部不与 12E-09D 抢文件。

### 8.2 需要先补文档才能启动的

| 卡 | 阻塞于 |
|---|---|
| 14A-03/04/05/06（契约细化）| 需 `DEV_14` 定设计基线，否则容易反复 |
| 14B-01 及以后（写代码）| 需 `PRD_14`（验收口径）+ `DEV_14`（设计）+ `CODEX_PROMPT_14`（执行指令）|
| 全部 14C/14D/14E/14F | 同上 |

### 8.3 我的建议（P）

```text
本周并行三件事：
  ① 我补齐 PRD_14 / DEV_14 / DEMO_14 / CODEX_PROMPT_14 / REPORT_14（约 1–2 人日文档工作）
  ② 你或主线开发启动 §8.1 的七张卡（14B-06 门禁优先，先止血）
  ③ 把 INT_16 §10 的 RIP 六问发出（不发出，14F 永远开不了工）
下周：
  ④ 文档齐备后 REPORT_12X 补 Stage 14 行，专项状态从 PROPOSED 转 READY
  ⑤ 按 14A → 14B → 14C 推进
```

**一句话回答第 3 问**：**文件未齐备（2/8），但"准备工作"可以现在就开**——因为最有价值的七张卡恰好都不依赖尚缺的那六份文档。补齐文档与启动准备可以并行，不必串行等待。

---

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。出具新模型五条变更点与逐文档同步台账（已改 12 处 / 定点声明 8 处）；一致性自检发现并补入新不变量"权威判定必须在 Worker 内、fast 只允许漏报"；给出 base/engine 改造 P0–P5 六步计划（12–20 人日，含在 M-B 预算内）；判定 Stage 14 文件齐备度 2/8，并列出可立即启动的七张准备卡 |
