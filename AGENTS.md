# Slice Soft Demo Codex Instructions

---

## ⚡ Active Work Entry（开工前先看这里）

```text
▶【当前主线】16-00-01..04 Stage 16 准入复核（2026-08-11）
  ✅ R-F-01 平滑顶点法线已完成
  ✅ R-F-02 基线重固化与证据刷新（含 R-C-00）已完成
  ▶ 16-00-01..04 纯审计，出口 GO / DEFER / NO-GO
  卡 docs/codex_task/current/TASKS_16_切片几何采样甲片接触姿态与性能专项任务清单.md
  裁决 docs/slice/DOC/DOC_DECISION_16_00_Stage16准入Gate口径与R_F线排期裁定.md

⏸【可延后】每项均有触发条件，不满足不得开工 —— 完整表见上述裁决文 §3.5
  T-A-05B-02+03 捆绑 ← 等【用户删除确认】     T-A-04      ← 等外部 RIP 证据
  R-C-01/02          ← 等 R-F-02 数据+回签    R-D / R-E   ← 无触发迹象
  H-G 组             ← 等 5 项产品 Gate       CI 组       ← 等解除暂缓+定 runner
  16C-10             ← 13B 产品输入，恒 INPUT_OPEN

  🔀 R-C-00 已并入 R-F-02（同一次测量，拆开等于跑两次 Release 测量周期）

🔴 Stage 16 准入 Gate 已裁定取【读法甲】= Stage 14【切片侧】收口，
   不等 14A_EXTERNAL_ACK 外部回签。当前报告已确认 14D-05/06/07/08 与
   14C-06B 全部完成，因此 16C-08 不再被 Stage 14 内部边界阻塞；
   16C-10 仍因设备 buildVolume/SLA 等产品输入保持 INPUT_OPEN。

各专项状态（均不占阶段编号，状态以各任务卡内的状态列为准）

RENDER    ✅ R-A / R-B / R-F 收口（含 meshoptimizer 1.1、平滑法线与真实资产预算重测）
          ⏸ R-C / R-D 判定【不进入下一步】；低成本入口是 R-C-00（纯测量）
          卡 docs/codex_task/current/TASKS_RENDER_模型显示与LOD修复补充任务清单.md
HOSTFLOW  ✅ H-A..H-F 全组完成（2026-08-11）；H-G 已准备并延期实施（等 5 项产品输入）
          卡 docs/codex_task/current/TASKS_HOSTFLOW_宿主业务流程与场景生命周期补齐任务清单.md
TIFF      ⏸ 默认后端已切 libtiff，风险已关死（fail-closed+弃用告警+无静默回退）
          当前【无待办】：05B-02/03 延后并捆绑（等删除确认）、T-A-04 外部阻塞
          卡 docs/codex_task/current/TASKS_TIFF_默认后端切换与对齐根治任务清单.md
CI        ⏸ 用户 2026-08-10 裁决【暂缓】，清单保留不开工
          卡 docs/codex_task/current/TASKS_CI_冻结面工程保护任务清单.md

⛔ 外部阻塞（切片侧做不了）：14A_EXTERNAL_ACK 待打印侧书面回签
   → 它阻塞 14D-05..08、14C-06B 与 Stage 14 的 14F-02..05 外部验收
   → 但【不再阻塞】Stage 16 的 16-00 准入复核（见上方读法甲裁定）

⚠️ Stage 14 之前的全部 Release 性能基线因 T-A-03 切换 LibTIFF 已作废
   （Writer-only p50 变化 +1.086%~+48.775%）。13F-R1-06 的「完整写包 6516.322 ms」
   等旧数【不可与新基线比较】，16C-02 必须整套重跑。
```

**「下一张卡是什么」不在本文件维护，去任务卡里读状态列。**
用户说「下一个任务是什么」时，读上述任务卡，报出第一张状态为
`PROPOSED` / `PREPARED` 且前置已满足的卡，**不要自行开工**。

⚠️ 本文件下方的 `Current Phase` 与 `Mandatory Reference Docs` 含大量 Stage 09–13 时期的
历史条目，**不代表当前待办**。以 `Active Work Entry` 与任务卡为准。

---

## Project Identity

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Current branch/ref: `feature/14-slicer-capability-package` as of 2026-08-09; verify with `git branch --show-current` before each task
- Main implementation paths: `src/slicer_core`, `apps/slicer_cli`, `apps/slicer_debug_ui`, `apps/slicer_ui_host_sim`（参考宿主）, `apps/slicer_host_sim`（纯 C 宿主）
- Formal docs: `docs/slice`
- Codex task docs: `docs/codex_task`
- Archived historical docs: `docs/archive/2026-06-30_slicer_legacy`
- Tech stack: C++20, Qt 5.15 Widgets, CMake, Windows x64/MSVC, optional OpenVDB via vcpkg
- Default test command: `ctest --test-dir build -C Debug --output-on-failure`

## Current Phase

- `12A` material fill, support, and varnish semantics have completed the current P0/P1 scope.
- `12B-R0/R1/R2` performance evaluation and OpenVDB SDF utility positioning are complete.
- `12C-R0` Qt workbench build compatibility and baseline admission is complete.
- `12C-R1` Profile and Settings closure is complete.
- `12C-R0/R1/R2` Qt workbench is complete; final fresh build, UI Smoke, and CTest passed.
- The latest completed production-mainline task is `12E-10D Stage 12E final closure`. `12E-10A..D` cover same-layer production TIFF semantics, the real OBJ/3MF dual-mode matrix, Release phase/memory evidence, and final report/user-guide closure.
- Stage 13 original P0 PRD/DEV/DEMO and all 17 near-term tasks are complete. `13A-01..05`, `13B-01..07`, `13C-01..05`, scene-aware `12E-09A-02`, and the inserted `13B-08-01..04`, `13D-01..04`, `13E-01..05` are implemented and verified. `13E` freezes deterministic standard-nail auto orientation (`rotate_x_90`, front toward scene `+Z`), the product default `maxHeightMm=9`, right-side “预检与诊断”, and the mutually exclusive right-side “任务详情”. `12E-09A-01..06` diagnostic UI and `12E-10A` same-layer final consistency are complete and PASS. 13B production remains `INPUT_OPEN` because device buildVolume/origin/axes and the 22-instance production budget are unresolved.
- Inserted `13F-R0` interaction/cancel stabilization is complete. `13F-R1-06` fixed enabled-auto-orient grounding for source meshes already below the height limit and closed the first Reality Release benchmark; `13F-R1-01..05` remain active preparation/implementation work.
- `13E-R1-01` planar heading normalization is complete: enabled auto-orient rotates flat X-major nail footprints by a deterministic Z quarter-turn, checks already Y-major footprints for a reversed narrow end, and makes the nail tip face scene `+Y`; explicit auto-orient disable still preserves source heading.
- Inserted `13G` support base projection and layer-continuity specialty is functionally complete (`13G-00..07`). Reality 5/5 source models were confirmed face-down, front-up correction selects `rotate_x_180_rotate_z_minus_90`, and corrected segment_105 keeps S support continuous through the former layer 20/21 break. `support.baseProjection` now provides configurable maximum-footprint S-channel base layers; production UI defaults to 30 layers while legacy configs with the field absent remain disabled. The Release segment_105 package passes RIP with exact base range `layerIndex 0..29`.
- The candidate texture-carrier/white-separation/RIP-underbase specialty (`12G-TCWS`) is frozen as of 2026-07-27. Do not implement its config, resolver, composer, UI, or RIP contract until its product/RIP questions and G1-G8 are explicitly closed.
- `03D-01..07` remain the 2026-08-03 historical `GO_OPTIONAL` baseline. The user-authorized TIFF T-A-01..03 follow-up completed on 2026-08-11 and supersedes only its default-Writer conclusion: LibTIFF 4.7.1 is now the default Writer, handwritten is an explicit legacy validation lane, and default compression remains `none`. T-A-05A and T-A-05B-01 are complete; T-A-05B-02 is the next non-destructive migration card. T-A-04 remains externally blocked, while T-A-05B-03 requires explicit deletion confirmation.
- `03E-01` is complete and `03E-02` is internally complete as of 2026-08-03. PackBits is an explicit experimental `output.tiffCompression.algorithm` option across Legacy/Global/Scene, manifest, strict Reader, native preview, and Qt; the default remains `none`. External target RIP/control-software interoperability is still pending, so the decision is `NO_GO_DEFAULT_EXTERNAL_INTEROP_PENDING`.
- `12E-09D-01..06` and `12E-10A/10B/10C/10D` are complete as of 2026-08-03. 10B adds a reproducible 17-row real OBJ/3MF final-closure matrix: xiao_ma/yecan Legacy/Global minimum/intermediate/all_texture and Texture2D checker 3MF are 14/14 production PASS; aishen/meigui/titian are 3/3 `BLOCKED_EXPECTED`; RIP strict is 14/14 and fallback is zero. 10C passes 36/36 measured Release samples and RIP strict; Global is 1.826x-2.562x core, 2.244x-3.161x total, and 3.079x-4.304x peak memory versus Legacy. 10D closes the final report and user guide. Stage 12E is complete within the approved scope; Legacy remains default and Global remains explicit candidate.
- Stage 15 is complete as of 2026-08-04 (`19/19`, production Profile enabled). Its scope remains Legacy full-volume RGB same-layer W carrier output only; do not change `p0.rgbwsv.2`, closure rules, S/V ownership, the existing strict RGB Profile, or Global Surface Shell behavior without a new decision.
- Stage 14 slicer-side work is complete as of 2026-08-07. The current status is `SLICER PACKAGE READY / INTERFACES FROZEN / EXTERNAL ACCEPTANCE DEFERRED`: 14A..14F local gates are closed, including the Release package, M1 intake, S1 positive/negative flow and S2 C1-C7 contract gate. Printing-side, target RIP, clean-machine and physical-print evidence remain deferred and must not be described as PASS or production-ready. Any frozen ABI, Worker, S1, S2 or ViewData change requires a controlled revision and rerunning the Stage 14 gates.
- 12G has partial RIP facts but remains frozen: one full-RGB package may be reused by RIP for transparent or opaque-white output, while current white-region `WSV=000` is a private downstream signal that conflicts with physical `black_is_print` channel semantics. Do not implement it until the RIP contract and collision policy are resolved; texture underbase is explicitly out of scope.
- `12D-R0/R1/R2/R3` is complete.
- `12E-01/02/03/04/05/06/07`, `12E-08A/08B/08C`, `12E-08C-R1-01..04`, `12E-08C-R2-01..04`, and `12E-08C-R3-01..04` are complete. R3-04 records the historical `NO-GO / FROZEN`. `12E-08C-R4-01..07`, R4-07-R1, R4-07-R2, Quick-CI-R1, and R4-08-R2 are complete; the two-family candidate matrix, versioned reference-machine candidate budget, and current Quick CI are PASS. R4-08-R2 is `GO` after explicit authorization. aishen/meigui/titian remain a 0/3 complex-relief coverage gap. `12E-09A-01..06`, `12E-08D-01..06`, `12E-09B-01..06`, `12E-09C-01..06`, `12E-09D-01..06`, and `12E-10A/10B/10C/10D` are complete. Stage 12E is COMPLETE in its approved scope.
- `12D-R0` documentation admission is complete and the 12C gate is satisfied.
- `global_surface_shell_restricted_candidate` and `global_surface_shell_material_parity_candidate` are admitted as explicit opt-in Profiles at 0.01 mm. xiao_ma/yecan TIFF and RIP strict pass. In the 2026-07-24 09B closure matrix, Global remains 4.09x-5.92x slower and uses 8.19x-8.74x peak memory versus Legacy, so Legacy remains the default and no silent fallback is permitted.
- The repair prerequisite must remain explicit and disabled by default. `repair_then_strict` must re-run strict diagnostics; `manual_repair_required` must never count as a production PASS.
- **HOSTFLOW 补充专项（不占阶段编号）**：H-A..H-F 已全部完成（2026-08-11）；H-F-02..05 收口相机连续性、排版边距、常用工艺/Profile 哈希和新版宿主耗时观测。H-G 生产 TIFF 三维层栈预览仅完成准备并延期实施，不属于当前开工项。
- **RENDER 补充专项**：R-A-01 已完成但口径需按 `DOC_ANALYSIS_RENDER_RD_B_前置复核` 更正（真实触发面为 35/36 而非 17/36）。**RD-B（引入 `meshoptimizer`）已推迟**，须先修 RB-P1/P2/P3 并由 R-A-02 重测后再裁决。
- The formal product direction is tracked in `docs/slice`; operational Codex tasks are tracked in `docs/codex_task/current`. 任务状态以各任务清单内的状态列为唯一真源。

## Always-On Rules

1. Answer in Chinese unless the user explicitly asks for English.
2. Execute only the task explicitly requested by the user; do not start the next task without explicit instruction.
2b. 完成一张卡后，必须在**该卡所属的任务清单**内更新其状态列、完成日期与实际验证结果，并追加修订记录。任务卡是任务状态的唯一真源；不得只写报告不更新状态列。
3. Before code, build, dependency, or architecture changes, read the relevant source and project docs first.
4. Do not invent command results, tests, builds, hardware validation, repository state, or implementation status.
5. Before each task, run `git status --short` and report unrelated dirty state instead of overwriting it.
6. Do not revert or delete user changes unless the user explicitly requests that operation.
7. For destructive operations, dependency upgrades, architecture migration, production-path changes, hardware/device control, or git history rewrite, give a plan and wait for confirmation.
8. After a minimal task, run task-specific validation. Before committing, run `git status --short` and `git diff --check`.
9. Commit only when the user asks or when the active task explicitly requires it; do not push unless explicitly instructed.
10. New commits must use `type(scope): 【功能分类】中文摘要`; use Chinese body items such as `【模块】`, `【验证】`, and `【边界】`. Do not rewrite published remote history solely to restyle old messages.

## Evidence Classification

- A: current code/config/tests/build scripts; safe implementation basis.
- B: formal `docs/slice` PRD/DEV/ADR/decision docs; target direction, not proof of implementation.
- C: archived demo docs, historical reports, chat logs, and completed Codex prompts/tasks; background only.
- D: deprecated or conflicting material; do not use as implementation basis.

When answering implementation-state questions, split the answer into `Current State`, `Target State`, `Historical State`, and `Pending Confirmation` when relevant.

## Skill Routing

- Slice feature planning and staged execution: `$slice-dev-workflow`
- Slice architecture boundaries and ADR/DOC_DECISION work: `$slice-architecture-guardrails`
- Slice build, dependency, CMake, packaging, and CI issues: `$slice-build`
- Slice code review and pre-merge checks: `$slice-code-review`
- Slice document-state conflict resolution: `$slice-doc-state-resolver`
- Slice context handoff: `$slice-context-handoff`
- Slice chat save/archive: `$slice-chat-save`
- Generic C++20/Qt/CMake guidance: `$cpp-coding-standards`
- Generic plan writing or project planning: `$writing-plans` / `$project-planner`

Project-level slice skills and `.agents/docs` facts override generic templates when they conflict.

## Mandatory Reference Docs

- AI collaboration rules: `.agents/AGENTS.md`
- Skill master: `.agents/docs/SLICE_AI_SKILL_MASTER.md`
- Project profile: `.agents/docs/project-profile.md`
- Architecture boundaries: `.agents/docs/architecture-boundary.md`
- Build and test: `.agents/docs/build-and-test.md`
- Code standards: `.agents/docs/code-standards.md`
- Commit style: `.agents/docs/commit-style.md`
- Document state: `.agents/docs/doc-state.md`
### ⚡ 当前有效（先看这几份，其余为历史条目）

- **TIFF 当前任务卡**：`docs/codex_task/current/TASKS_TIFF_默认后端切换与对齐根治任务清单.md`
- **T-A-03 默认 LibTIFF 准备**：`docs/slice/DOC/DOC_PREP_TIFF_T_A_03_默认LibTIFF切换准备.md`
- **HOSTFLOW 开工入口**：`docs/codex_task/current/CODEX_PROMPT_HOSTFLOW_宿主业务流程与场景生命周期执行指令.md`
- **HOSTFLOW 任务卡**：`docs/codex_task/current/TASKS_HOSTFLOW_宿主业务流程与场景生命周期补齐任务清单.md`
- **RENDER 任务卡**：`docs/codex_task/current/TASKS_RENDER_模型显示与LOD修复补充任务清单.md`
- **视图接线归属裁决**：`docs/slice/DOC/DOC_DECISION_HOSTFLOW_H_D_R1_视图接线归属与14E_04d延期作废.md`
- **目标水位裁决（HQ-09/HQ-10）**：`docs/slice/DOC/DOC_DECISION_HOSTFLOW_H_E_R1_参考宿主目标水位裁决.md`
- **RD-B 前置复核（推迟 meshoptimizer 的依据）**：`docs/slice/DOC/DOC_ANALYSIS_RENDER_RD_B_前置复核_预算膨胀三处根因.md`
- **ViewData 网格 DTO 规格**：`docs/slice/DOC/DOC_SCHEMA_14_SceneViewData网格DTO规格.md`
- **S2 合同定案（含 8 项作废方案禁止清单）**：`docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md`

### 📚 历史条目（Stage 09–13 时期，仅作背景，**不代表当前待办**）

- Formal docs index: `docs/slice/README.md`
- Codex task index: `docs/codex_task/README.md`
- Completed 12D task list: `docs/codex_task/current/TASKS_12D_横截面材料无缝闭环任务清单.md`
- Prepared 12E task list: `docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md`
- Prepared 12E execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md`
- Prepared 12E repair task list: `docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md`
- Prepared 12E repair execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md`
- Prepared 12E-R4 task list: `docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md`
- Prepared 12E-R4 execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_08C_R4_模型导入预检与修复资产准入执行指令.md`
- Approved 12E dual-mode decision: `docs/slice/DOC/DOC_DECISION_12E_Legacy与GlobalSurfaceShell双切片模式.md`
- Prepared 12E-08D dual-mode production task: `docs/slice/DOC/DOC_PREP_12E_08D_双模式生产写包准备.md`
- Prepared 12E-09B production UI task: `docs/slice/DOC/DOC_PREP_12E_09B_Qt双模式生产入口准备.md`
- Current 12E-09B task list: `docs/codex_task/current/TASKS_12E_09B_Qt双模式生产入口任务清单.md`
- Prepared 12E-09C X/Y DPI task: `docs/slice/DOC/DOC_PREP_12E_09C_XY_DPI准备.md`
- Current TIFF compression task: `docs/codex_task/current/TASKS_03E_TIFF压缩兼容与性能任务清单.md`
- Current TIFF compression decision: `docs/slice/DOC/DOC_DECISION_03E_TIFF压缩候选与性能Gate.md`
- Current TIFF compression status: `docs/slice/REPORT/REPORT_03E_02_TIFF生产压缩协议与RIP兼容当前状态.md`
- Prepared 12E-09D task: `docs/codex_task/current/TASKS_12E_09D_生产纹理厚度与单材料材质任务清单.md`
- Prepared 12E-09D execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_09D_生产纹理厚度与单材料材质执行指令.md`
- 12G current RIP strategy review: `docs/slice/DOC/DOC_REVIEW_12G_TCWS_现有RIP白区合同与六通道策略比对.md`
- Current 12E-09A diagnostic task list: `docs/codex_task/current/TASKS_12E_09A_诊断UI任务清单.md`
- Prepared 12E-09A-05 task: `docs/slice/DOC/DOC_PREP_12E_09A_05_同层语义Preview准备.md`
- Completed 12E-09A closure report: `docs/slice/REPORT/REPORT_12E_09A_诊断UI阶段收口.md`
- 12E-09A user guide: `docs/user_guides/SLICE_12E_09A_纹理填充诊断使用说明.md`
- Prepared 12E-10 task list: `docs/codex_task/current/TASKS_12E_10_双模式最终闭环任务清单.md`
- Prepared 12E-10 execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_10_双模式最终闭环执行指令.md`
- Completed 12E-10A status: `docs/slice/REPORT/REPORT_12E_10A_同层Preview最终一致性当前状态.md`
- Completed 12E-10B status: `docs/slice/REPORT/REPORT_12E_10B_真实OBJ_3MF双模式矩阵当前状态.md`
- Completed 12E-10C status: `docs/slice/REPORT/REPORT_12E_10C_Release性能与内存当前状态.md`
- Completed Stage 12E status: `docs/slice/REPORT/REPORT_12E_全局纹理壳层与模型填充当前状态.md`
- Stage 12E user guide: `docs/user_guides/SLICE_12E_双模式纹理壳层与模型填充验收说明.md`
- Stage 13 decision: `docs/slice/DOC/DOC_DECISION_13_模型场景排版与TIFF原生预览专项拆分.md`
- Stage 12/13 priority and freeze decision: `docs/slice/DOC/DOC_DECISION_12X_剩余任务优先级与专项冻结.md`
- Stage 13 task list: `docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md`
- Stage 13 execution prompt: `docs/codex_task/current/CODEX_PROMPT_13_模型场景排版联合切片与TIFF预览执行指令.md`
- Stage 12/13 cross-stage execution dashboard: `docs/codex_task/current/TASKS_12_13_后续开发计划总览清单.md`
- Stage 15 decision and preparation: `docs/slice/DOC/DOC_DECISION_15_纹理纯白区按需补白与材料闭合修复专项.md`, `docs/slice/DOC/DOC_PREP_15_纹理纯白区按需补白实施准备与依赖审查.md`
- Stage 15 task list and execution prompt: `docs/codex_task/current/TASKS_15_纹理纯白区按需补白任务清单.md`, `docs/codex_task/current/CODEX_PROMPT_15_纹理纯白区按需补白执行指令.md`
- Active Stage 14 decision and status: `docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md`, `docs/slice/REPORT/REPORT_14_切片能力包封装与打印软件集成准备状态.md`
- Active Stage 14 task list and execution prompt: `docs/codex_task/current/TASKS_14_切片能力包封装与打印软件集成任务清单.md`, `docs/codex_task/current/CODEX_PROMPT_14_切片能力包封装与打印软件集成执行指令.md`
- Stage 13 full atomic preparation: `docs/slice/DOC/DOC_PREP_13_全阶段原子任务实施准备与文件所有权.md`
- Current Stage 13B layout report: `docs/slice/REPORT/REPORT_13B_03_11x2规则排版当前状态.md`
- Prepared 13B-04 fixture admission: `docs/slice/DOC/DOC_PREP_13B_04_幅面碰撞与逐实例准入准备.md`
- 13B-04 status: `docs/slice/REPORT/REPORT_13B_04_幅面碰撞与逐实例准入当前状态.md`
- Prepared 13B-05 joint layer composition: `docs/slice/DOC/DOC_PREP_13B_05_全局Raster与联合层合成准备.md`
- 13B-05 status: `docs/slice/REPORT/REPORT_13B_05_全局Raster与联合层合成当前状态.md`
- Prepared 13B-06 single package and scene report: `docs/slice/DOC/DOC_PREP_13B_06_单Package与SceneReport准备.md`
- 13B-06 status: `docs/slice/REPORT/REPORT_13B_06_单Package与SceneReport当前状态.md`
- 13B-07 functional matrix status: `docs/slice/REPORT/REPORT_13B_07_真实模型矩阵与阶段收口当前状态.md`
- Approved 13B-08/13D UI workflow decision: `docs/slice/DOC/DOC_DECISION_13B_08_场景作业流与13D工作台收口优先级.md`
- Active 13B-08 task list: `docs/codex_task/current/TASKS_13B_08_场景作业流收口任务清单.md`
- 13B-08-01 implementation status: `docs/slice/REPORT/REPORT_13B_08_01_批量导入与主切片入口当前状态.md`
- 13B-08-02 implementation status: `docs/slice/REPORT/REPORT_13B_08_02_场景生产服务与CLI当前状态.md`
- 13B-08-03 implementation status: `docs/slice/REPORT/REPORT_13B_08_03_Qt当前场景切片当前状态.md`
- Prepared 13D task list: `docs/codex_task/current/TASKS_13D_Qt工作台布局收口任务清单.md`
- Completed 13E task list: `docs/codex_task/current/TASKS_13E_甲片自动定向与诊断工作流任务清单.md`
- Completed 13E status: `docs/slice/REPORT/REPORT_13E_甲片自动定向与诊断工作流当前状态.md`
- Active 13G task list: `docs/codex_task/current/TASKS_13G_支撑投影铺底与层间连续性任务清单.md`
- Active 13G execution prompt: `docs/codex_task/current/CODEX_PROMPT_13G_支撑投影铺底与层间连续性执行指令.md`
- 13G evidence audit: `docs/slice/DOC/DOC_AUDIT_13G_Reality模型朝向与内部支撑连续性.md`
- 13G current status: `docs/slice/REPORT/REPORT_13G_支撑投影铺底与层间连续性准备状态.md`
- Prepared 13C-03 unified production preview: `docs/slice/DOC/DOC_PREP_13C_03_UnifiedProductionPreview准备.md`
- Completed 13C report: `docs/slice/REPORT/REPORT_13C_TIFF原生统一预览阶段收口.md`

## Production Safety Rules

1. Do not enable OpenVDB by default.
2. Do not make OpenVDB a mandatory dependency for all builds.
3. Do not replace the legacy `slicer_cli` production path.
4. Do not write production RGBWSV TIFF from the experimental OpenVDB path unless a later task explicitly allows it.
5. Do not modify the `p0.rgbwsv.2` production package protocol.
6. Do not modify RGBWSV channel order.
7. Do not modify uint8 bit depth.
8. Do not modify `black_is_print` polarity.
9. Do not treat `warn_and_attempt` output as production-safe.
10. Confirmed self-intersection must fail fast.
11. Non-manifold, duplicate/opposite duplicate, and local winding issues must block strict production admission.

## Expected Workflow Per Task

For every task:

```powershell
git status --short
```

For documentation/config-only tasks, validate with targeted text/schema checks and `git diff --check`.
For C++/Qt/CMake changes, use the task-specific commands from `.agents/docs/build-and-test.md`.
