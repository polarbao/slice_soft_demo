# CODEX_PROMPT_14 切片能力包封装与打印软件集成执行指令

> 文档状态：✅ **ACTIVE / DEVELOPMENT READY**（2026-08-04 授权激活）
> 版本：v1.1 ｜ 日期：2026-08-03 ｜ 激活：2026-08-04
> 适用：Stage 14 全部原子任务（14A–14F）

---

## 0. 开工前三条最重要的事（2026-08-04 新增）

### 0.1 S2 条款只看一份文档

```text
✅ docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md   ← 权威，实施只看这里
❌ docs/slice/DOC/DOC_CHECKLIST_14_...md                  ← 往来记录档案，含【被否决】的候选
❌ docs/slice/DOC/DOC_ANALYSIS_14_Q2_...md                ← 推导过程档案，已降级
```

清单与分析文档里保留了完整的方案比选过程，其中大部分**已被否决**。
照着它们实现会写出作废代码。

### 0.2 ⛔ 以下内容禁止实现

```text
✗ Writer 断言：写出前扫描 W==0 && S==0 && V==0 的哨兵检查
✗ manifest 字段 ripBoundIntermediate { whiteRegionSentinel: "WSV=000", ... }
✗ 白区路径 A / B / C / E 的任何形式
✗ 逐层 1-bit sidecar
✗ p0.rgbwsv.3 协议扩展
```

原因：RIP 侧已选定**路径 D** —— 废弃 `WSV=000` 哨兵，用 `W=0` 真实材料语义表达
需要打印的白。**不存在哨兵，就不需要保护哨兵的机制。**
完整作废清单见 `DOC_DECISION_14_S2` §4。

> ⚠️ **不要误删**：错误码 `PM-SLICER-CONTRACT-0060` 本身**有效** —— 它是 SPI 既有通用错误码
> （「自检发现产物不符合 `p0.rgbwsv.2`」，见 `INT_07` §错误码表、`INT_02` §6）。
> 作废的只是「用它承载 `WSV=000` 哨兵扫描」这一用途；**错误码表整体保留，不因本轮 Q2 定案变更。**

### 0.3 首批可并行开工

```text
14A-01  contracts/ 目录 + print_module_spi.h
14A-02  p0.rgbwsv.2 JSON Schema（须覆盖 Stage 15 三字段 + 14A-10 whiteSemantics）
14A-07  第三方依赖再分发合规审查
14A-09  REPORT_12X 补 03E 行（03E-02 现为 GO_ON_DEMAND）
14B-06  CI 行数门禁
14B-00  核心库分层可行性验证
```

**14A-08 已 COMPLETE，不要重新发 RIP 问卷。**

---

## 1. 执行前必读（按顺序）

```text
1. AGENTS.md
2. .agents/AGENTS.md
3. docs/slice/REPORT/REPORT_12X_阶段计划与完成度总览.md        ← 当前主状态
4. docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md
5. docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md        ← 【S2 权威条款】
6. docs/slice/PRD/PRD_14_切片能力包封装与打印软件集成.md
7. docs/slice/DEV/DEV_14_切片能力包封装与打印软件集成.md
8. docs/slice/DEMO/DEMO_14_切片能力包封装与打印软件集成验收方案.md
9. docs/codex_task/current/TASKS_14_切片能力包封装与打印软件集成任务清单.md
10. 目标任务对应的 docs/claude/INTEGRATION/INT_* 细节（见下表）
```

| 任务组 | 细节文档 |
|---|---|
| 14A 契约 | `INT_09`（缺口与补齐）、`INT_16` §5/§6（DTO 与 facade）|
| 14B facade / 分层 | `INT_16` §3.1、`INT_17` §6（P0–P5）|
| 14C DLL 薄壳 | `INT_07` §2 |
| 14D Worker / 取消 | `INT_16` §3.2/§3.3/§3.4、`INT_10` §3.3/§3.5 |
| 14E 交互与拆分 | `INT_15` §4、`INT_11` |
| 14F 打包联调 | `INT_12`、`INT_08` |

## 2. 执行规则

```text
一次只执行【一个】明确指定的原子任务；不得合并多卡；
任务状态推进：PREPARED → READY → IN PROGRESS → COMPLETE，不得跳级；
开始前执行：git branch --show-current 与 git status --short；
只修改属于本任务的文件；不得覆盖用户未提交的改动；
同一文件同一时间只能有一个任务 Owner（见 TASKS_14 §8 所有权表）；
单次提交不得同时改变【算法】【公共合同】【文件布局】三者中的两项以上。
```

## 3. 红线（违反即回滚）

```text
R1 不修改 p0.rgbwsv.2 / RGBWSV 顺序 / uint8 / black_is_print / printValue=0 / emptyValue=255；
R2 legacy 保持默认；OpenVDB 保持 optional 且默认 OFF；禁止静默回退；
R3 跨 ABI 只允许 C 基本类型、const char*、不透明句柄；禁 STL / Qt / 异常跨界；
R4 slicer_base 不得依赖 slicer_engine；slicer_module 链接闭包不得出现 engine 符号；
R5 切片只在 Worker 执行，不得新增进程内切片路径；
R6 DllMain 只允许 return TRUE；初始化放 pm_create + std::call_once；
R7 fast preflight 结果不构成准入结论，必须标注 authoritative: false；
R8 12G-TCWS 冻结：不得实现其配置 / resolver / composer / UI / RIP 合同；
R9 不得采用与合法内容碰撞的带内像素哨兵（0/0/0/255/255/255 已否决）；
R10 切换默认 TIFF Writer 后端需独立 Gate 与用户单独授权；
R11 confirmed self-intersection 必须 fail-fast；manual_repair_required 不算 production PASS。
```

## 4. 统一验证门

每个涉及生产路径的任务完成后**必须实际执行**：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
.\scripts\run_material_closure_tests.ps1 -Mode RepairDisabled   # 30 层 TIFF SHA-256 不变
```

按任务类型追加：

| 任务类型 | 追加验证 |
|---|---|
| facade / 分层 | 对应 facade 单测 + CI 单向依赖检查 |
| DLL 薄壳 | `test_spi_conformance`（C-SPI-01..18）+ `dumpbin /EXPORTS` + `/DEPENDENTS` |
| Worker / 取消 | 各阶段取消用例 + `.staging` 残留检查 + `--contract-info` 协商 |
| 引擎替换 | 引擎一致性套件 E-01..08 |
| UI 改动 | `slicer_debug_ui --self-test` + overlay smoke |
| 打包 | 干净机装载 + `EnumProcessModules`（AC-28-04）|

**禁止**：把历史报告中的 PASS 当作本轮结果；未运行的验证不得写 PASS。

## 5. 回答格式（代码改动前必须先给）

```markdown
## Implementation Plan
### Problem Type
### Layer(s) Involved            （base / engine / module / worker / apps / docs）
### Official Documents
### Current Code Reality         （带文件:行号）
### Current State
### Target State
### Pending Confirmation
### Risk Points
### Files To Change              （逐一列出，确认无 Owner 冲突）
### Verification Plan            （将实际执行的命令）
```

大改动**先停下等用户确认**再实现。

## 6. 完成后必须同步

```text
1. TASKS_14：该卡状态、完成日期、实际证据、下一任务；
2. REPORT_14：修改文件、实际命令、结果、剩余风险；
3. REPORT_12X：若阶段完成度变化则更新 Stage 14 行；
4. 若改变契约：同步 contracts/ 物料并通知打印侧/RIP 侧；
5. 若验证未运行或失败：状态不得写 COMPLETE。
```

## 7. 当前可执行入口

```text
状态：Stage 14 = ✅ ACTIVE（2026-08-04 用户授权激活）

首批可并行开工（互不依赖）：
  14A-01  contracts/ 目录 + print_module_spi.h
  14A-02  p0.rgbwsv.2 JSON Schema（覆盖 Stage 15 三字段 + 14A-10 whiteSemantics）
  14A-07  第三方依赖再分发合规审查
  14A-09  REPORT_12X 补 03E 行（03E-02 现为 GO_ON_DEMAND）
  14B-06  CI 行数门禁
  14B-00  核心库分层可行性验证

已完成，不要重做：14A-08（RIP 六问两轮闭合）
不得作为起点：14C 及以后（需 14A 契约冻结完成）
14F 三方联调：切片侧可推进，但外部 RIP 实机互操作与 S2-R1 极性映射表由双边关闭
```

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。必读顺序、执行规则、11 条红线、统一验证门与按类型追加、回答格式、完成后同步项 |
