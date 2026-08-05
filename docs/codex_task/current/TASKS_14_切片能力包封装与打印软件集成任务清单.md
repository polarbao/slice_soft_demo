# TASKS_14 切片能力包封装与打印软件集成任务清单

> 文档状态：✅ **ACTIVE**（用户于 2026-08-04 授权激活）
> 版本：v1.1 ｜ 日期：2026-08-03 ｜ 激活：2026-08-04
> 作者：Claude 起草；执行由主线开发（codex）接管
> 决策依据：`docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md`
> **S2 权威条款：`docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md`（实施只看该文）**
> 详细设计：`docs/claude/INTEGRATION/INT_06..15`
> 打印侧对接：`ry_print_demo/docs/claude/INTEGRATION/CLD_04,05,06,10,27` + `PLANNING/CLD_07`

---

## 0.0 开工须知（2026-08-04 激活时新增）

```text
① 首批可并行开工：14A-01、14A-02、14A-07、14A-09、14B-06、14B-00（互不依赖）
② 14A-08 已 COMPLETE —— RIP 六问两轮闭合，不要重新发问卷
③ 新增 14A-10（manifest whiteSemantics），依赖 14A-02
④ Stage 15 已 COMPLETE，其新增的 texture.unprintableWhite* 三字段与
   whiteSemantics 是【不同层级】的东西：前者是像素级写入策略，后者是作业级语义声明。
   14A-02 的 Schema 必须同时覆盖两者，不要混为一谈。
```

**⛔ 禁止实现（路径 A 配套，已随路径 D 定案作废）**

```text
✗ Writer 断言：写出前扫描 W==0 && S==0 && V==0 的哨兵检查
✗ manifest ripBoundIntermediate { whiteRegionSentinel: "WSV=000", ... }
✗ 路径 A / B / C / E 的任何形式
✗ 逐层 1-bit sidecar
✗ p0.rgbwsv.3 协议扩展
完整清单：DOC_DECISION_14_S2 §4。若在旧文档中见到上述内容，以合同定案为准。

⚠️ 不要误删：错误码 PM-SLICER-CONTRACT-0060 本身【有效】，它是 SPI 既有通用错误码
   （「自检发现产物不符合 p0.rgbwsv.2」，见 INT_07 / INT_02 错误码表）。
   作废的只是「用它承载 WSV=000 哨兵扫描」这一用途，错误码与错误码表整体保留。
```

---

## 0. 使用规则

```text
一次只执行一个明确指定的原子任务；
任务状态推进：PREPARED → READY → IN PROGRESS → COMPLETE，不得跳级；
每个任务完成必须记录：实际命令、build type、结果、剩余风险；
未运行的验证不得写成 PASS；
同一文件同一时间只能有一个任务 Owner。
```

**统一出口门（所有涉及生产路径的任务）**：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
.\scripts\run_material_closure_tests.ps1 -Mode RepairDisabled   # 30 层 TIFF SHA-256 不变
```

---

## 1. 14A 契约冻结（门禁：未完成不得进入 14B）

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14A-01 | 建立 `contracts/` 目录；落盘 `print_module_spi.h`（与打印侧 `CLD_10` 同源，我方只同步不改）| — | C 与 C++ 编译器分别编过；`dumpbin /EXPORTS` 预期恰好 11 个 `pm_*` | 🟢 **READY（首批）** |
| 14A-02 | `p0.rgbwsv.2` 形式化 JSON Schema（**须同时覆盖 Stage 15 的 `texture.unprintableWhite*` 三字段与 14A-10 的 `whiteSemantics`**）| — | 用真实 manifest 样例校验通过 | 🟢 **READY（首批）** |
| 14A-03 | `file_contract_v1` 完整规格（请求/结果 JSON schema、进度行、退出码表、超时、僵尸回收、staging 清理时序）| 14A-01 | 打印侧确认可满足 | PREPARED |
| 14A-04 | 能力 DTO 字段级规格（15 项能力的请求/响应字段与类型）| 14A-01 | 打印侧据此可编码 | PREPARED |
| 14A-05 | 三车道交互契约固化（`operationId` 幂等、`expectedSceneRevision`、`SceneRevisionStale` 回滚）| 14A-04 | 与打印侧 `CLD_04` §4.3 一致 | PREPARED |
| 14A-06 | 取消语义写入契约（`Cancelling ≠ Cancelled`、≤2s、staging 清理）| 14A-01 | 与 13F-R0-03 实现一致 | PREPARED |
| 14A-07 | 第三方依赖再分发合规审查（assimp / miniz / libtiff 许可证 + NOTICE）| — | 成文，可随包分发 | 🟢 **READY（首批）** |
| 14A-08 | **对 RIP 统一确认清单发出并回签** | — | RIP 侧按模板回填并回传 | ✅ **COMPLETE（2026-08-04，两轮均已闭合）** |
| | ↳ 往来记录：`docs/slice/DOC/DOC_CHECKLIST_14_对RIP侧技术确认清单.md`（v1.4，已转档案）| | | |
| | ↳ **权威条款：`docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md` —— 实施只看该文** | | | |
| 14A-09 | `REPORT_12X` 补 03E 行（03E-02 现为 **`GO_ON_DEMAND`**，见 `REPORT_03E_02` §5.1）| — | 主状态表完整 | 🟢 **READY（首批）** |
| **14A-10** | **manifest 新增 `whiteSemantics`（`opaque` \| `transparent`）**：manifest 为权威、Profile 仅提供默认值；两处不一致时 **fail-closed** | 14A-02 | Schema 覆盖新字段；不一致用例 fail-closed；无该字段的既有包仍可读（向后兼容） | **NEW（合同定案 N1 产生，2026-08-04）** |

**14A 出口**：`contracts/` 四份物料齐备；打印侧与 RIP 侧书面确认；`OPEN-14-03/04/05` 关闭。

> **14A-10 溯源**：`DOC_DECISION_14_S2` §1.4。Q3.1 确认「同层不需混用两种白」，故白色语义为
> **作业级**声明而非逐像素，不需要 `p0.rgbwsv.3`。这是本轮 RIP 问答产生的**唯一**切片侧新实现工作。

> ⛔ **禁止实现**（路径 A 配套，已随路径 D 定案作废）：Writer 断言 `PM-SLICER-CONTRACT-0060`、
> manifest `ripBoundIntermediate` 字段。完整作废清单见 `DOC_DECISION_14_S2` §4。

---

## 2. 14B 核心 facade（Qt-free）

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14B-00 | **核心库分层可行性验证**：能否把 `scene/ layout/ config/ 几何查询/ 包读取/ preview` 拆为 `slicer_base`，其余入 `slicer_engine`；重点验证 `model.cpp`(1970 行) 能否进 base | 14A-04 | 出结论文档；若不可行则 `model.import` 改 Worker 承载 | 🟢 **READY（首批，14A-04 未完成前先出可行性结论）** |
| 14B-01 | 新建 `src/slicer_core/api/`；定义 facade 接口与 DTO（含强制 `ICancelToken`）| 14A-04 | 接口单测；不含 Qt/ABI 类型 | PREPARED |
| 14B-01A | **落地 base/engine 两库拆分 + CI 单向依赖检查** | 14B-00, 14B-01 | `slicer_base` 不含 engine 符号；构建图正确 | PREPARED |
| 14B-02 | `ModelFacade` + `PackageQueryFacade` 实现（复用既有能力）| 14B-01 | 行为与既有 CLI 一致 | PREPARED |
| 14B-03 | `SceneFacade`（变换/碰撞/越界权威求值 + revision）| 14B-01 | 与 `layout/` 既有判定逐条一致 | PREPARED |
| 14B-04 | `SliceFacade`（提交/进度/取消）| 14B-01 | 生产 TIFF 逐字节不变 | PREPARED |
| 14B-05 | `slicer_cli` 改走 facade | 14B-02..04 | full 回归通过 | PREPARED |
| 14B-06 | **CI 行数门禁 G1..G5 + 白名单机制** | — | 门禁生效（`INT_11` §2.1）| 🟢 **READY（首批）** |

---

## 3. 14C DLL 薄壳

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14C-01 | 新建 `src/slicer_module/`；`PM_API`/`PM_CALL __cdecl`/`.def`（11 符号）| 14A-01, 14B-01 | `dumpbin /EXPORTS` 恰好 11 个，无 C++ 修饰名 | PREPARED |
| 14C-02 | 缓冲三态协议 `WriteOut()` 单一实现 | 14C-01 | C-SPI-05a/b/c | PREPARED |
| 14C-03 | `HandleRegistry` 句柄生命周期 + `pm_last_error`（TLS）| 14C-01 | C-SPI-04/12/13/14/15 | PREPARED |
| 14C-04 | 同步轻能力接线（`syncCapabilities[]` 声明）| 14C-02, 14B-02..03 | 首次 `pm_poll` 即返回终态 | PREPARED |
| 14C-05 | `pm_module_info` + `module.json` + 版本/运行时自述 | 14C-01 | C-SPI-01/02/03 | PREPARED |
| 14C-06 | `test_spi_conformance` 自测套件 | 14C-01..05 | **C-SPI-01..18 全绿** | PREPARED |
| 14C-07 | `DllMain` 红线 + `std::call_once` 初始化 + 无 Qt/PrintSDK 依赖 | 14C-01 | C-SPI-16/17 | PREPARED |

**14C 出口**：`slicer_module.dll` 可被独立套件全绿验证 = 打印侧 M1-07 门禁满足。

---

## 4. 14D Worker 与取消

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14D-01 | 新建 `apps/slicer_worker/`（由 `slicer_cli` 演进）| 14A-03 | 与 CLI 行为一致 | PREPARED |
| 14D-02 | `WorkerClient`（DLL 侧）：启动/进度解析/退出码映射/僵尸回收 | 14D-01, 14C-01 | 子进程后端可用 | PREPARED |
| 14D-03 | **`file_contract_v1` 版本协商**：`slicer_worker.exe --contract-info` + major/minor 兼容规则 + 不匹配 fail-closed | 14D-02, 14A-03 | 篡改 major 被拒绝；minor 向后兼容可用 | PREPARED |
| 14D-04 | **切片链路 cancel token 贯穿**（step 边界 + 逐层循环协作式取消，经 `ICancelToken`）| 14B-04 | 各阶段取消 ≤2s | PREPARED |
| 14D-05 | staging→自检→原子发布 + 取消/崩溃清理双保险 | 14D-01 | C-SPI-09；无残留 | PREPARED |
| 14D-06 | 取消 `backend=inprocess` 切片路径；`options.backend` 收敛为 `worker` | 14D-02 | 无第二条切片路径 | PREPARED |
| 14D-07 | **引擎一致性套件 E-01..08**（Worker 独立替换的准入门）| 14D-03, 14D-05 | 套件可对任意 Worker 版本运行 | PREPARED |
| 14D-08 | Worker 独立调试入口：`slicer_worker.exe --spi-request <req.json>` | 14D-01 | 可脱离 DLL 单独运行并附加调试器 | PREPARED |

---

## 5. 14E 交互验证与拆分

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14E-01 | 轨一：`apps/slicer_host_sim/`（控制台，纯 C 调用参考实现）| 14C-06 | 可进 CI；演示 fail-closed | PREPARED |
| 14E-02 | 轨二：UI 模拟分支——`ModuleClient` + `SceneInteractionController` | 14C-06 | **禁 include `slicer_core/**`**，CI 依赖检查强制 | PREPARED |
| 14E-03 | 轨二：`TransformCommitPolicy`（三车道 + Stale 回滚）| 14E-02 | `mouse-move` 零跨 DLL 调用（可测）| PREPARED |
| 14E-04 | 轨二：`TopViewRenderPolicy` + `MoveOptimizationPolicy` | 14E-03 | 手感不回归；策略可配置 | PREPARED |
| 14E-05 | **据轨二边界拆分 UI 大文件**（`MainWindow` 3659 / `UiSmokeTestRunner` 6963）| 14E-04 | 各降至 <1500 行；self-test 绿 | PREPARED |
| 14E-06 | 产出"可移植模块清单"交打印侧 | 14E-04 | 打印侧确认可直接移植 | PREPARED |

> **顺序判断**：14E-05 必须在 14E-02..04 **之后**——先让使用场景长出"可移植 / 切片专有"的边界，再据此拆分，比按文件大小机械切分可靠。

---

## 6. 14F 打包与联调

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14F-01 | `modules/slicer/` 打包（DLL + Worker + module.json + 依赖 DLL）| 14C-06, 14D-07 | 干净机可装载 | PREPARED |
| 14F-02 | 与打印侧 M1 联调（装载 + 能力清单 + 自检）| 14F-01 | 打印侧 M1 出口 | **外部依赖** |
| 14F-03 | 与打印侧 M2 联调（单模型 → S1 校验）| 14F-02 | S1 正/负例通过 | **外部依赖** |
| 14F-04 | 与 RIP 联调（S2 契约 + `rip_output_validator`）| 14A-08 | S2 C1–C7 通过 | **外部依赖** |
| 14F-05 | 端到端到 Ready + 阶段收口报告 | 14F-04 | E2E 通过；出 `REPORT_14` | **外部依赖** |

---

## 7. 关键路径与并行

```text
可立即并行（不依赖外部）：
  14B-06 门禁  ·  14A-01/02/03/04/05/06/07/09  ·  TIFF 缺陷处置（OPEN-14-02 授权后）

阻塞于外部：
  14A-08（RIP 回签）→ 14F-04
  OPEN-14-01（优先级裁定）→ 全阶段启动时点
  OPEN-14-06（模型资产）→ 真实模型 E2E
    ↳ 解耦手段：用已有 7 个 strict-PASS 资产先跑通 14F-02/03
```

## 8. 与 12E-09D 的文件所有权隔离

| 文件/目录 | Owner |
|---|---|
| `src/slicer_core/config.*` | **12E-09D**（Stage 14 不动）|
| `src/slicer_core/api/`、`src/slicer_module/`、`apps/slicer_worker/` | **Stage 14**（新建，零冲突）|
| `apps/slicer_debug_ui/**` | Stage 14（14E），但**只在模拟分支上**，主干不动 |
| `src/slicer_core/slicer.cpp` | 暂无（拆分随 `CLAUDE_09` R-B，本阶段不启动）|

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版，PREPARED。6 个子阶段共 40 张原子任务卡；标注外部依赖与并行项；给出与 12E-09D 的文件所有权隔离 |
