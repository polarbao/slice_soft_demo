# REPORT_14 切片能力包封装与打印软件集成准备状态

> 文档状态：✅ **ACTIVE / IMPLEMENTATION AUTHORIZED**（2026-08-04 激活）
> 版本：v1.8 ｜ 更新日期：2026-08-05
> 本文是 Stage 14 的状态入口；Stage 12 总状态仍以 `REPORT_12X` 为准
> **S2 权威条款：`docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md`**

---

## 1. 门禁状态（v1.2 · 2026-08-04）

```text
DOCUMENTATION_GATE     = PASS          （10/10 必需文档齐备，见 §2）
IMPLEMENTATION_GATE    = AUTHORIZED    ← 用户于 2026-08-04 授权
STAGE15_PRECEDENCE     = CLEARED       （Stage 15 COMPLETE / PRODUCTION ENABLED）
EXTERNAL_EVIDENCE_GATE = CLOSED_ON_PAPER
                         RIP 六问两轮闭合、14A-08 COMPLETE；
                         外部 RIP【实机】互操作仍由 14F 关闭
CURRENT_NEXT_TASK      = 14A-06（14A-03 打印侧回签仍待取得）
```

### 1.1 激活前置清单

| 前置 | 状态 | 证据 |
|---|---|---|
| Stage 15 优先级让位 | ✅ CLEARED | `REPORT_15` COMPLETE / PRODUCTION ENABLED |
| RIP 六问回签 | ✅ CLOSED | `DOC_CHECKLIST_14` v1.4 §4.4，两轮闭合 |
| S2 条款收敛 | ✅ DONE | `DOC_DECISION_14_S2` v1.0 |
| 作废方案清理 | ✅ DONE | `DOC_ANALYSIS_14_Q2` 降级为档案；8 项禁止实现已登记 |
| 用户授权 | ✅ 2026-08-04 | 本次激活 |

### 1.2 本轮 RIP 问答对 Stage 14 的净影响

```text
✅ Q1=方案A  → 切片与打印软件零改动，14F 具备开工条件
✅ Q3.1      → 12G-TCWS 保持冻结，p0.rgbwsv.3 不需要
✅ Q4        → 03E-02 由 NO_GO_DEFAULT 转 GO_ON_DEMAND
➕ 14A-10    → manifest 新增 whiteSemantics（本轮唯一新增实现工作）
⛔ 8 项作废  → 见 DOC_DECISION_14_S2 §4，禁止实现
🔶 S2-R1     → 极性映射表转 RIP↔打印软件双边，【不阻塞切片侧】
```

### 1.2b UI 层时序（2026-08-04 用户决策）

```text
现阶段主干 UI 布局与功能【保持原样，不做任何改造】。
14E 前置为里程碑 M-MVP = 14C-06 全绿 + 14D-05 完成。
M-MVP 判据：宿主仅通过 11 个 pm_* 导出完成【导入→变换→切片→取包→校验】一次闭环。
```

新增 UI 需求（均落在 `apps/slicer_ui_host_sim/`，主干不动）：
3D 视角与相机操作（14E-04c）、俯视⇄3D 切换设置项（14E-04d）。
`slicer_ui_host_sim` 同时是**交付给打印侧的参考实现**，代码质量按对外交付物要求。

> 🔴 **新增高风险项 UI-R4**：`scene.get_viewdata` 的网格 DTO 从未定义，
> 而 3D 视角需要它。**必须在 14A-04 契约冻结时一并定死**，否则将迫使已交付 ABI 二次变更。
> 该要求已写入 14A-04 验收。

### 1.3 仍然开放的外部项

| 项 | 归属 | 阻塞范围 |
|---|---|---|
| 极性映射表书面确认（S2-R1） | RIP ↔ 打印软件双边 | RIP 实现、S2 校验器；**不阻塞切片侧** |
| 外部目标 RIP 实机互操作 | 14F 三方联调 | 14F 出口 |
| 实物工艺验证 | 工艺侧 | 发布授权 |

## 2. 文档齐备度

| 类型 | 路径 | 状态 |
|---|---|:--:|
| 主决策 | `docs/slice/DOC/DOC_DECISION_14_…` | ✅ v1.4 |
| S2 合同 | `docs/slice/DOC/DOC_DECISION_14_S2_…` | ✅ v1.0 / SETTLED |
| UI 决策 | `docs/slice/DOC/DOC_DECISION_14_UI_…` | ✅ v1.2 |
| ViewData DTO | `docs/slice/DOC/DOC_SCHEMA_14_SceneViewData…` | ✅ v1.1 / 供 14A-04 冻结 |
| 需求 | `docs/slice/PRD/PRD_14_…` | ✅ v1.1 |
| 设计 | `docs/slice/DEV/DEV_14_…` | ✅ v1.0 |
| 验收 | `docs/slice/DEMO/DEMO_14_…` | ✅ v1.0 |
| 状态 | `docs/slice/REPORT/REPORT_14_…`（本文）| ✅ v1.3 |
| 任务与执行指令 | `docs/codex_task/current/TASKS_14_…`、`CODEX_PROMPT_14_…` | ✅ v1.2 / v1.2 |
| 分析底稿 | `docs/claude/INTEGRATION/INT_06..17` | ✅（背景证据，不覆盖正式合同）|

**结论：文档准入完成。** 但"文档齐备"**不等于**代码完成、不等于第三方依赖已就绪、不等于发布授权。

## 3. 实现状态（A 级核实）

```text
src/slicer_core/api/        不存在
src/slicer_module/          不存在
apps/slicer_worker/         不存在
contracts/                  ✅ 已建立；SPI、错误码与三份 JSON Schema 已落盘（14A-01/02）
slicer_base / slicer_engine 未分层（当前仍为单一 slicer_core，CMakeLists.txt:29 默认 STATIC）
slicer_module* / .def       全仓库零命中
```

**Stage 14 当前完成 14A-01/02/04/05 与 14A-03 切片侧合同；能力 facade、DLL、Worker 与宿主模拟仍未实现。**

### 3.1 已完成原子任务

| 卡号 | 状态 | 交付物 | 验证 |
|---|---|---|---|
| 14A-01 | ✅ COMPLETE（2026-08-05） | `contracts/print_module_spi.h`、`contracts/slicer_error_codes.json`、C/C++ 合同编译探针 | 打印侧头文件逐字匹配；C/C++ Debug 编译通过；11 个声明与 19 个唯一错误码测试通过 |
| 14A-02 | ✅ COMPLETE（2026-08-05） | manifest、scene、Profile 三份 Draft 2020-12 Schema；Schema 自动验证脚本 | 真实 UI smoke manifest、既有 p0 manifest、既有 scene、Stage 15/旧 Profile 正向通过；通道顺序、whiteSemantics、zLimitMm 与 W 空值负例被拒绝；Debug/Release CTest 3/3 |
| 14A-03 | 🟡 SLICER-SIDE COMPLETE（2026-08-05） | `file_contract_v1.md`、请求/结果/协商 Schema、退出码表 | Python 合同正负例通过；Debug/Release CTest 2/2；打印侧书面确认待取得，因此不标最终 COMPLETE |
| 14A-04 | ✅ COMPLETE（2026-08-05） | 15 项能力字段级 JSON 合同、人工可读合同、ViewData 网格/LOD/blob 子操作 | Python 合同测试通过；能力数量、字段、错误码、生产协议与 Worker/ABI 边界均有漂移门禁 |
| 14A-05 | ✅ COMPLETE（2026-08-05） | 三车道机器合同与人工合同；同步补齐 Commit `currentSceneRevision` 字段 | 幂等、原子 revision、Stale 回读回滚、Production sceneHash/full preflight 门禁通过 |

14A-01 尚无 DLL，因此 `dumpbin /EXPORTS` 不在本卡伪造执行；实际 11 符号导出表由 14C-01 / 14C-06 关闭。

## 4. 已裁定事项

| 编号 | 事项 | 结论 | 日期 |
|---|---|---|---|
| D-1 | 优先级插入方案 | **乙 并行插入**（12E-09D 走既有序列，14A/14B/14C 并行）| 2026-08-03 |
| D-2 | TIFF Writer 后端 | **手写 Writer 保持默认、LibTIFF 保持可选**；默认切换未授权，PackBits 仅按需开启 | 2026-08-05 基线收口 |
| D-3 | 白区语义传递 | **S2 路径 D**：白色使用 `255/255/255/0/255/255` 的 W 真实材料语义；`whiteSemantics` 为作业级声明；WSV 哨兵、sidecar、`p0.rgbwsv.3` 均禁止 | 2026-08-04 S2 定案 |
| D-4 | Worker 定位 | **可独立迭代替换的切片引擎**；core 拆 base/engine；切片只在 Worker | 2026-08-03 |

## 5. 未决项

| 编号 | 事项 | 需谁答 | 阻塞 |
|---|---|---|---|
| OPEN-14-06 | 三个必需 OBJ 处置 | **产品** | 真实模型 E2E（可用 7 个 strict-PASS 资产解耦）|
| OPEN-14-07 | S2-R1 极性映射表 | **RIP 侧 + 打印侧** | 不阻塞切片侧；阻塞最终物理映射验收 |

`OPEN-14-03/04/05` 已由 `DOC_DECISION_14_S2` 关闭；旧问题及候选方案仅保留在档案文档中。

## 6. 可立即启动的准备任务（不受未决项阻塞）

| 卡 | 任务 | 估算 |
|---|---|---:|
| 14B-06 | CI 行数门禁 G1..G5 | 2–3 人日 |
| 14A-07 | 第三方依赖再分发合规审查 | 1 人日 |
| 14A-09 | `REPORT_12X` 补 03E 行 | 0.2 人日 |
| 14B-00 | base/engine 分层可行性验证 | 2–3 人日 |
| **剩余首批合计** | | **5–8 人日** |

以上五卡均不与已完成阶段抢文件（所有权见 `TASKS_14` §8）；14A-08 已完成，不得重复执行。

## 7. 与其他阶段的边界

```text
12E-09D / 12E-10  并行，不阻塞（Stage 14 首版能力面语义已由 12E-09B/09C 收口）
12F               Stage 14 的步骤/分层边界是其前置；仍按实测证据逐项授权
13F-R1            独立并行；其 Cancelling≠Cancelled 已被 Stage 14 采纳为契约条款
12G-TCWS          保持冻结；Stage 14 不实现，仅把白区语义列为对 RIP 确认项
03D               已 COMPLETE / GO_OPTIONAL；手写 Writer 保持默认，LibTIFF 可选
03E               03E-02 GO_ON_DEMAND；PackBits 可显式开启，默认仍为 none
```

## 8. 风险提示

```text
① 默认 Writer 仍为手写实现；LibTIFF 仅为可选后端，禁止在 Stage 14 内静默切换默认值；
② RIP 书面合同已闭合，但目标 RIP 实机互操作仍必须由 14F 留证；
③ base/engine 分层的三个高风险切分点（model.cpp / geometry / reports）需 14B-00 先验证；
④ 若无 CI 单向依赖门禁（14B-06 + P4），分层将在数月内退化回单库；
⑤ ViewData 网格传输须在 14A-04 冻结 DTO，但不得增加第 12 个 ABI 导出符号。
```

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。文档齐备度 8/8、实现量 0；记录 D-1..D-4 四项裁定与 OPEN-14-03..08 六项未决；列出可立即启动的七卡（8–12 人日）|
| 2026-08-03 | v1.1 | 同步 Q2 深度审查：撤回当前阶段 sidecar 推荐；记录完整配置/贴图碰撞范围；将 OPEN-14-04 改为确认既有 WSV=000 或 W-only Profile 六通道路径 |
| 2026-08-05 | v1.3 | Stage 14 开工基线收口：文档门更新为 10/10；D-2/D-3 对齐 03D/03E 与 S2 权威结论；移除已关闭开放项和已完成 14A-08；登记独立宿主模拟与 ViewData DTO 风险 |
| 2026-08-05 | v1.4 | 完成 14A-01：建立 contracts、同步 11 函数 SPI 头文件、登记 19 项错误码并通过 C/C++ 合同测试；下一任务推进为 14A-02 |
| 2026-08-05 | v1.5 | 完成 14A-02：形式化 manifest、scene、Profile 三份 JSON Schema，覆盖 Stage 15 三字段、`whiteSemantics`、`zLimitMm`，并通过真实/既有样例兼容与负向合同测试；下一任务推进为 14A-03 |
| 2026-08-05 | v1.6 | 完成 14A-03 切片侧 `file_contract_v1` 合同与自动验证；因打印侧书面确认尚未回签保持 SLICER-SIDE COMPLETE，切片侧下一任务推进为 14A-04 |
| 2026-08-05 | v1.7 | 完成 14A-04：冻结 15 项能力字段级 DTO，并把 Scene ViewData 的 local 网格、LOD、双身份缓存和既有 ABI blob 分块纳入合同；下一任务推进为 14A-05 |
| 2026-08-05 | v1.8 | 完成 14A-05：冻结三车道交互、operationId 幂等、SceneRevisionStale 回滚与 Production 准入；下一任务推进为 14A-06 |
