# CODEX_PROMPT_14 切片能力包封装与打印软件集成执行指令

> 文档状态：**PREPARED / NOT ACTIVE**
> 版本：v1.0 ｜ 日期：2026-08-03
> 适用：Stage 14 全部原子任务（14A–14F）

---

## 1. 执行前必读（按顺序）

```text
1. AGENTS.md
2. .agents/AGENTS.md
3. docs/slice/REPORT/REPORT_12X_阶段计划与完成度总览.md        ← 当前主状态
4. docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md
5. docs/slice/PRD/PRD_14_切片能力包封装与打印软件集成.md
6. docs/slice/DEV/DEV_14_切片能力包封装与打印软件集成.md
7. docs/slice/DEMO/DEMO_14_切片能力包封装与打印软件集成验收方案.md
8. docs/codex_task/current/TASKS_14_切片能力包封装与打印软件集成任务清单.md
9. 目标任务对应的 docs/claude/INTEGRATION/INT_* 细节（见下表）
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
状态：Stage 14 = PROPOSED / NOT ACTIVE（待用户授权）
授权后建议起点：14B-06（CI 行数门禁）与 14A-01（contracts/ 落盘）—— 二者无外部依赖
不得作为起点：14C 及以后（需 14A 契约冻结完成）
```

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。必读顺序、执行规则、11 条红线、统一验证门与按类型追加、回答格式、完成后同步项 |
