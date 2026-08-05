# Slice Soft Demo Codex Instructions

## Project Identity

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Current branch/ref: `feature/12e-08c-mesh-repair` as of 2026-07-21; verify with `git branch --show-current` before each task
- Main implementation paths: `src/slicer_core`, `apps/slicer_cli`, `apps/slicer_debug_ui`
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
- `03D-01..07` are complete as of 2026-08-03. The optional LibTIFF 4.7.1 build/Runtime lane, dual Writer, stripped/tiled output, compatibility and performance gates, isolated Runtime Package/RIP smoke, and default-lane full regression are executable. The final result is `GO_OPTIONAL`: handwritten remains the default because LibTIFF did not meet the 15% p50 improvement gate. Reopening a default switch requires new Gate evidence and explicit authorization.
- `03E-01` is complete and `03E-02` is internally complete as of 2026-08-03. PackBits is an explicit experimental `output.tiffCompression.algorithm` option across Legacy/Global/Scene, manifest, strict Reader, native preview, and Qt; the default remains `none`. External target RIP/control-software interoperability is still pending, so the decision is `NO_GO_DEFAULT_EXTERNAL_INTEROP_PENDING`.
- `12E-09D-01..06` and `12E-10A/10B/10C/10D` are complete as of 2026-08-03. 10B adds a reproducible 17-row real OBJ/3MF final-closure matrix: xiao_ma/yecan Legacy/Global minimum/intermediate/all_texture and Texture2D checker 3MF are 14/14 production PASS; aishen/meigui/titian are 3/3 `BLOCKED_EXPECTED`; RIP strict is 14/14 and fallback is zero. 10C passes 36/36 measured Release samples and RIP strict; Global is 1.826x-2.562x core, 2.244x-3.161x total, and 3.079x-4.304x peak memory versus Legacy. 10D closes the final report and user guide. Stage 12E is complete within the approved scope; Legacy remains default and Global remains explicit candidate.
- Stage 15 is complete as of 2026-08-04 (`19/19`, production Profile enabled). Its scope remains Legacy full-volume RGB same-layer W carrier output only; do not change `p0.rgbwsv.2`, closure rules, S/V ownership, the existing strict RGB Profile, or Global Surface Shell behavior without a new decision.
- Stage 14 is active and implementation-authorized. Start with one atomic task at a time from `14A-01 / 14A-02 / 14A-07 / 14A-09 / 14B-06 / 14B-00`; S2 implementation follows only `DOC_DECISION_14_S2_RIP接口合同定案.md`. The handwritten TIFF Writer remains default, LibTIFF remains optional, and PackBits is on-demand. Do not start 14E UI work before M-MVP (`14C-06` green + `14D-05` complete).
- 12G has partial RIP facts but remains frozen: one full-RGB package may be reused by RIP for transparent or opaque-white output, while current white-region `WSV=000` is a private downstream signal that conflicts with physical `black_is_print` channel semantics. Do not implement it until the RIP contract and collision policy are resolved; texture underbase is explicitly out of scope.
- `12D-R0/R1/R2/R3` is complete.
- `12E-01/02/03/04/05/06/07`, `12E-08A/08B/08C`, `12E-08C-R1-01..04`, `12E-08C-R2-01..04`, and `12E-08C-R3-01..04` are complete. R3-04 records the historical `NO-GO / FROZEN`. `12E-08C-R4-01..07`, R4-07-R1, R4-07-R2, Quick-CI-R1, and R4-08-R2 are complete; the two-family candidate matrix, versioned reference-machine candidate budget, and current Quick CI are PASS. R4-08-R2 is `GO` after explicit authorization. aishen/meigui/titian remain a 0/3 complex-relief coverage gap. `12E-09A-01..06`, `12E-08D-01..06`, `12E-09B-01..06`, `12E-09C-01..06`, `12E-09D-01..06`, and `12E-10A/10B/10C/10D` are complete. Stage 12E is COMPLETE in its approved scope.
- `12D-R0` documentation admission is complete and the 12C gate is satisfied.
- `global_surface_shell_restricted_candidate` and `global_surface_shell_material_parity_candidate` are admitted as explicit opt-in Profiles at 0.01 mm. xiao_ma/yecan TIFF and RIP strict pass. In the 2026-07-24 09B closure matrix, Global remains 4.09x-5.92x slower and uses 8.19x-8.74x peak memory versus Legacy, so Legacy remains the default and no silent fallback is permitted.
- The repair prerequisite must remain explicit and disabled by default. `repair_then_strict` must re-run strict diagnostics; `manual_repair_required` must never count as a production PASS.
- The formal product direction is tracked in `docs/slice`; operational Codex tasks are tracked in `docs/codex_task/current`.

## Always-On Rules

1. Answer in Chinese unless the user explicitly asks for English.
2. Execute only the task explicitly requested by the user; do not start the next task without explicit instruction.
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
- Formal docs index: `docs/slice/README.md`
- Codex task index: `docs/codex_task/README.md`
- Completed 12D task list: `docs/codex_task/current/TASKS_12D_横截面材料无缝闭环任务清单.md`
- Next prepared task list: `docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md`
- Next prepared execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md`
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
