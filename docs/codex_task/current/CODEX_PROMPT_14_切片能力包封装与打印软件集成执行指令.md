# CODEX_PROMPT_14 切片能力包封装与打印软件集成执行指令

> 文档状态：✅ **ACTIVE / DEVELOPMENT READY**（2026-08-04 授权激活）
> 版本：v1.3 ｜ 日期：2026-08-03 ｜ 激活：2026-08-04 ｜ 双视图纹理修订：2026-08-05
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
14A-02  p0.rgbwsv.2 / scene JSON Schema（覆盖 Stage 15 三字段、whiteSemantics、zLimitMm）
14A-07  第三方依赖再分发合规审查
14A-09  REPORT_12X 补 03E 行（03E-02 现为 GO_ON_DEMAND）
14B-06  CI 行数门禁
14B-00  核心库分层可行性验证
```

**14A-08 已 COMPLETE，不要重新发 RIP 问卷。**

### 0.4 UI 层整体后置，先做能力包（2026-08-04 用户决策）

```text
现阶段主干 UI（apps/slicer_debug_ui/）布局与功能【保持原样，一行不改】。
14E 分成“关闭 Gate 的控制台验证”和“Gate 后的 Qt UI”：

  M-MVP-CANDIDATE = 14C-06 全绿 + 14D-05 完成
  M-MVP = M-MVP-CANDIDATE + 14E-01 纯 C 宿主闭环 PASS

Candidate 达成前不要启动 14E；Candidate 达成后只启动 apps/slicer_host_sim/。
14E-01 PASS 形成 M-MVP 后，才新建 apps/slicer_ui_host_sim/。
```

### 0.5 🔴 14A-04-R1 已受控修订双视图纹理合同

原 14A-04 只冻结几何网格，不足以满足用户确认的硬标准：**top 与 three_d 都必须显示模型纹理**。
用户已授权修改冻结文件，受控修订由 `DOC_DECISION_14A_04_R1_双视图纹理ViewData合同修订.md`
定案，并已同步到能力 DTO 1.2：

```text
top       surfacePreview（带纹理 +Z 正交投影）
three_d   mesh + texcoord0 + submeshes + materials + texture blobs
失败      缺纹理、解码失败、UV/材质绑定无效必须显式失败，禁止“成功 + 灰模”
不变      PM_SPI_VERSION=1、11 个导出、15 项能力、p0.rgbwsv.2
```

**合同不是实现。** 14B-03A 必须实现 `TexturedSceneViewDataProvider`，并成为 14C-04、
14E-04 与 14E-04c 的硬前置；UI 不得 include core 或自行旁路加载纹理。

### 0.6 🔴 三车道调用逻辑已修正

```text
Transient  只用宿主本地矩阵和 bbox；碰撞只是 non-authoritative 视觉反馈；mouse-move 0 次 DLL
Commit     正常成功直接采用 apply_operation 响应，不强制追加 get_snapshot
Recovery   只有 SceneRevisionStale、显式刷新或恢复流程才调用 get_snapshot
Camera     orbit/pan/zoom 与 top/three_d 切换均为 0 次 DLL
```

### 0.7 🔴 另一处独立的生产风险：`buildVolume` 没有 Z 限高

`MultiModelScene.h:149` 的 `SceneBuildVolume` 注释明写 “Optional printable **XY** volume”，
只有 `widthmm`(X) / `heightmm`(**Y，不是 Z**)。后果：

```text
模型超高无法判定 —— 切片会成功，但实物打不出来。
```

由 **14A-11** 修复：新增 `zLimitMm`（`std::optional`，缺省时行为与现状逐字节一致）。
默认设备幅面 **230 × 100 × 60 mm**。

⚠️ 不得拿 `autoOrient.maxHeightMm`（现配置 9.0 / 6.0）当限高 —— 那是**自动定向目标高度**，
与"设备物理最高能打多高"是两回事，混用会同时错两处。

详见 `DOC_DECISION_14_UI_宿主模拟改造专项.md` §6.4 / §6.6。

---

## 1. 执行前必读（按顺序）

```text
1. AGENTS.md
2. .agents/AGENTS.md
3. docs/slice/REPORT/REPORT_12X_阶段计划与完成度总览.md        ← 当前主状态
4. docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md
5. docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md        ← 【S2 权威条款】
5b. docs/slice/DOC/DOC_DECISION_14_UI_宿主模拟改造专项.md      ← 【14E 权威设计】做 14E 前必读
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
| UI 改动 | 独立 `slicer_ui_host_sim` smoke + 双视图纹理 fixture + 调用计数/FPS/设置持久化；仅 14E-05 修改主干时才跑 `slicer_debug_ui --self-test` + overlay smoke |
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
  14A-02  p0.rgbwsv.2 / scene JSON Schema（覆盖 Stage 15 三字段、whiteSemantics、zLimitMm）
  14A-07  第三方依赖再分发合规审查
  14A-09  REPORT_12X 补 03E 行（03E-02 现为 GO_ON_DEMAND）
  14B-06  CI 行数门禁
  14B-00  核心库分层可行性验证

已完成，不要重做：14A-08（RIP 六问两轮闭合）
已受控修订：14A-04-R1（切片侧合同已完成，打印侧 DTO 1.2 回签待取得）
UI 数据前置：14B-03A TexturedSceneViewDataProvider
不得作为起点：14C 及以后（需 14A 契约冻结完成）
14F 三方联调：切片侧可推进，但外部 RIP 实机互操作与 S2-R1 极性映射表由双边关闭
```

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。必读顺序、执行规则、11 条红线、统一验证门与按类型追加、回答格式、完成后同步项 |
| 2026-08-05 | v1.2 | Stage 14 开工基线收口：明确 14A-02 Schema 范围，保持 ViewData DTO 归 14A-04，禁止借 Stage 14 静默切换默认 TIFF Writer |
| 2026-08-05 | v1.3 | 修正 M-MVP 自循环；登记 14A-04-R1 双视图纹理合同与 14B-03A Provider；冻结 Transient/Commit/Recovery 调用边界，并补充独立 UI 宿主验证口径 |
