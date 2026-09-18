# Slice Soft Demo Codex Instructions

---

## ⚡ Active Work Entry（开工前先看这里）

```text
▶【当前主线】16C-06-MEMFLOW 有界流式内存根治专项已授权并开工（2026-08-18）
  ✅ R-F-01 平滑顶点法线已完成
  ✅ R-F-02 基线重固化与证据刷新（含 R-C-00）已完成
  ✅ 16-00-01..04 准入复核已完成，结论 PARTIAL GO
  ✅ 16A-01 合成 fixture 和差异 schema 已完成
  ✅ 16A-02 STL-only Policy/Provider 合同已完成，Golden 零漂移
  ✅ 16A-03 Layer Slab Candidate 已完成，候选 Package/RIP PASS
  ✅ 16A-04 固定 2x2 S3/S4 候选已完成，Package/RIP PASS，Legacy Golden 零漂移
  ✅ 16B-01 边界带与接触指标基线已完成，Reality 5/5 + 标准甲片 6/6 PASS
  ✅ 16A-05/06、16B-02/03、16C-01/02 已完成
  ✅ 16C-03 支撑统计扫描融合完成，三真实模型逐层 TIFF/RIP 零语义漂移
  ✅ 16C-05 Mask 按需物化、统计融合与单实例 Buffer 复用已完成，性能复测待补
  ✅ 16D-01 已完成 S0/S3 受限生产合同接入，S0 默认不变
  ✅ 16D-02-R1 已允许单材料 W/V 浮雕显式使用 S3，纹理关闭且 S0 默认不变
  ✅ MEMFLOW MF-00..03B4A 已完成：预算、Owned Layer、Occupancy、Support discovery/shape/final replay
  ▶ MEMFLOW MF-03B4B 已准备但未开工，尚未切换生产路径
  ▶ 16B-04 / 16D-05 仍需单独授权
  卡 docs/codex_task/current/TASKS_16_切片几何采样甲片接触姿态与性能专项任务清单.md
  内存专项 docs/codex_task/current/TASKS_16C_06_MEMFLOW_有界流式内存根治专项任务清单.md
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

LOGDUMP   SLICER DELIVERY COMPLETE / MERGED TO LOCAL PRODUCT（2026-09-15，G01..03完成）。
          product/packaged-slicer已从d28b6451快进至7354a616，后续仅提交状态文档；未推送。
          清洁源码0.2.471-dev全目标Release重建、完整包启动落盘通过；67项62通过/5条既有及依赖失败。
          新SDK23文件独立构建和真实打印后端消费2/2通过；E01B待打印P23入口，不等于生产全验收。
          ~~当前产品工作树slice_soft_demo-logdump；原slice_soft_demo仍为P0FIX~~【2026-09-18 更新】产品线已移回 slice_soft_demo，logdump 工作树改为 detached。**但它不是冗余残留、不要删**：E01B 未完成（缺 P23 loader），而 E01A 验收报告引用的 `slice_soft_demo-logdump/runtime/logdump/Release/slicer_module.dll` 仍在原位，是外部项目 ry_print_demo/PrintSolution 集成消费的那一个；另有 10 余份 LOGDUMP 文档引用该路径。
          最新 docs/slice/REPORT/REPORT_LOGDUMP_G_产品合入与复用交付收口.md
          以下为G之前各轮历史证据：
          2026-09-14 LD-00..03 / A00..A03（含A01D）/ B01..02 / C01..02 本地完成。
          独立分支 codex/feature-logging-dump 基于 e2546797，不包含原工作树未提交修改。
          DLL 回调 + 软件日志 + Worker IPC + EXE helper 已实现；Debug16/16，Release17/18（既有S1路径失败）。
          真实模型184层/fixture20层off-info字节一致；独立包中文路径/仅系统PATH验证通过，另复现既有ViewData期望失败。
          E01A 已完成真实 PrintAppLogging+DLL 验收1/1、父工程可选适配构建、切片兼容回归2/2。
          2026-09-15 E02-01/02 完成：23文件源码SDK独立构建、实际打印消费CTest2/2 PASS，成功3 TIFF/Reader/18条日志。
          源码交付含可选转储；F01已补齐旧模块包helper和C头，实际验包通过；超长输出路径仍为既有限制。
          E01B 缺正式几何切片loader；PrintApp GUI/干净机器/其他ACP/物理打印未验。
          F01..03完成：分组提交，product基线已整合进专项；Release全目标构建通过，67项62通过/5条既有及依赖失败。
          F当时限定诊断功能合入CONDITIONAL GO；完整Stage14全绿仍NO-GO。当时product未移动；G已按新授权合入。
          2026-09-15 E01B-P 准备完成：MOD-22/P23入口仍未实现；原树P0FIX持续演进，集成前需重新固定SHA。
          准备 docs/slice/DOC/DOC_PREP_LOGDUMP_E01B_业务挂接与新基线交付准备.md
          交付 docs/slice/REPORT/REPORT_LOGDUMP_E02_源码SDK交付与成功切片验收.md
          保持原 pm_* 签名/Worker/S1/S2；新增导出受控评审，不在 DLL 安装崩溃过滤器。
          卡 docs/codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md

P0FIX     ▶【P0层完成，已与产品线合并】分支 codex/feature-p0fix-contract-robustness（自 d28b6451 切出）
          来源 analysis/ 专项发现的 P0 层，九个原子任务 P0-00..P0-08 全部完成
          ✅ P0-00 基线固化（2026-09-14）：全量回归 246 项 / 7 项既有失败，作为对照基线
          ✅ TEXFAIL 修掉基线 7 项中的 2 项（14c04 同步能力安全、14e04d 双视图契约），
             稳定失败集降至 5 项；CTest 注册数 246 → 258
          ✅ F-09 引擎拆解：slicer.cpp 5888 → 1961 行，八个新编译单元，八次字节级基线全通过
          ✅ F-09 单测补齐：八个单元 35 个公开入口 / 49 条用例，22 处扰动逐条转红
          ✅ 输入加固：F-33 JSON 深度、F-34 模型尺寸、F-35/51 进程内读取、F-52 TIFF 解码膨胀比
          ✅ F-50 行数门禁补清点棘轮（基线 132 个超阈值文件，只防新增、不冻结清单）
          ⚠ 【本文两处旧认知已被基线更正】本文他处所记 CTest 注册数 234 已过期；
             分层门禁 slicer_stage14b_layering_feasibility_test 与宿主行数门禁
             slicer_stage14e02_qt_host_boundary_test 均已注册进 CTest 且【当前为红】，
             不是「只有 --self-test、全是静默债」
          ⏸ 待用户拍板：R-06 分层门禁白名单归属（两个新头由 FRAME/UNIPATH 引入，不属本专项）；
             F-45 v1 映射补全、F-35 第三层配额入 DTO（均为行为变更）
          ⏸ 用户暂缓：F-16 CI、R-11 RIP→SPI
          ⏭ 合入后开：R-08 测试目标批量化、R-13 配置面按车道重组（两者前置均为「无在飞分支」）
          卡 docs/codex_task/current/TASKS_P0FIX_分析专项P0契约一致性与输入加固.md
          分析 analysis/README.md｜analysis/04_问题清单与改动空间.md（F-01..F-52）
             ｜analysis/06_改进路线图与验证方案.md（R-01..R-13 分级与验证 Gate）

GITOPS    ▶【进行中】分支模型与发布规范治理
          规范 docs/git/Git 分支与发布规范.md（2026-09-18 按实测修订初版）
          初版 docs/git/Git 版本发布流程 + 测试分支规划.md（2026-08-05，保留供对照）
          ✅ 分支模型按实测重写：初版依赖的 develop 当时【一条都不存在】、main 落后产品线 481 条、版本派生方式与构建系统相反
          ✅ 2026-09-18 采纳 develop 并给出实测职责：三档验证映射三层分支闸门
          ✅ main 已快进至产品线（b5fc0fb3 → a0a4f742，489 条落差归零）并推送
          ✅ 约束落进本文第 11 条——规范放在 docs/ 里 agent 不会主动读
          ✅ 入口已定：保留 main 作 origin/HEAD 入口，但它**只跟随 product、绝不直接提交**；发布流程末尾把 main 快进到 product

FRAME     COMPLETE：2026-09-07 nail-Default 非打印定位素材专项 FRAME-00..04 完成；gubao05 多图层透明核心工艺 600 DPI/0.033 mm、148 层真实包与空区/RIP strict PASS，Release 部署/自检通过；GUI 人工交互和物理打印未验证；卡 docs/codex_task/current/TASKS_FRAME_非打印定位素材与输出画幅专项任务清单.md

RENDER    ✅ R-A / R-B / R-F 收口（含 meshoptimizer 1.1、平滑法线与真实资产预算重测）
          ⏸ R-C / R-D 判定【不进入下一步】；低成本入口是 R-C-00（纯测量）
          卡 docs/codex_task/current/TASKS_RENDER_模型显示与LOD修复补充任务清单.md
HOSTFLOW  ✅ H-A..H-F 全组完成（2026-08-11）；H-G 已准备并延期实施（等 5 项产品输入）
          卡 docs/codex_task/current/TASKS_HOSTFLOW_宿主业务流程与场景生命周期补齐任务清单.md
MEMFLOW   ⏸ 分支 codex/memflow-bounded-streaming（尖端 826a170）【暂缓合入】【开发已重启】
            用户 2026-08-24 裁定暂缓合入，理由三条：代码尚未接生产路径、无功能紧迫性、
            slicer.cpp 的 G2 冻结线违规应由该专项自行处理
          ▶ 用户 2026-09-04 授权【继续开发】并同意跳过 MF-06（Sparse）。
            注意这是开发授权，【不是合入授权】——上述暂缓合入的裁定继续有效，
            合入需用户另行裁定。三条理由中「无功能紧迫性」已失效（见下），
            「未接生产路径」与「G2 违规待处理」两条仍成立。
          ▶ 紧迫性来源（2026-09-04 实测，见 REPORT_16C_06_MEMFLOW_替代基线资产与调查结论）：
            finger_suoguo/a-2/0.2.obj 在 10um 层厚下 1429 层 × 7,795,500 列 = 111.4 亿
            pixel-layer，六通道 Dense 62.2 GB 对机器物理内存 31.6 GB；单模型实跑
            450/1429 层用时 593s（全程外推约 31 分钟），双模型直接内存不足失败。
            有界窗口目标 134 MB。该专项由预防性优化转为解除真实生产阻塞。
          ▶ 修订实施路径：MF-03B4B → MF-04 → MF-07（MF-06 跳过，MF-05 转条件项）。
            跳过 MF-06 的依据：上游 MF-07 依赖原文即写「MF-06 Gate 或明确跳过 Sparse」。
            后果：G-M6（123.stl 17 个连通分量）随之失效，重启 Sparse 须重建该 Gate，
            且 123.stl 资产已不存在，须改用缩裹测试-2 的 8 个 STL 替代。
          ⚠ 合入将带进 12 处行数门禁 ERROR（2026-09-04 复核，原记 9 处漏列 3 项）：
            G2 slicer.cpp 5423→5464（>1000 行只减不增）
            G1 超 500 行：BoundedSupportShapeScan.cpp 1171、BoundedSupportDiscovery.cpp 619、
               LayerOccupancyProvider.cpp 528、GlobalSurfaceShellProductionPipeline.cpp 502
               及 5 个 tests/stage16 用例（1024/1005/840/643/638）
            G3 超 200 行：BoundedSupportShapeScan.h 280、BoundedSupportDiscovery.h 223
            原记 9 处的数值本身逐项复核一致，遗漏的是 LayerOccupancyProvider.cpp、
            GlobalSurfaceShellProductionPipeline.cpp 与 LayerOccupancyProviderTests.cpp
          ⚠ 该门禁在 CTest 中只注册 --self-test，仓库全扫描未进 CTest，故上述为静默债而非红灯
          ▶ merge 冲突面已试算：仅 CMakeLists.txt 与 TASKS_16 两处，AGENTS.md 可自动合并
          ▶ 本分支工作树中曾存在的 MEMFLOW 残留已于 2026-08-24 证明为严格过时并剔除
            （41 文件逐一比对：24 逐字节一致、1 严格子集、其余独有行皆为更旧状态头）
          卡 位于分支内 docs/codex_task/current/TASKS_16C_06_MEMFLOW_*.md，本分支尚无该文件
RIPFLOW   ✅ 00 / A / B / C / D 全组完成（D-01..06），切片侧收口
          ⛔ E-01/E-02 外部分发与生产验收 BLOCKED_EXTERNAL
          ▶ D-06 新增 outputValidationMode=strict_s2|diagnostic_unvalidated，默认严格；
            诊断模式只放宽墨滴上限门，产出 rip_diagnostic/ 且恒不可 S2 发布
          卡 docs/codex_task/current/TASKS_RIPFLOW_切片后外置RIP集成专项任务清单.md
MATVOL    ✅ MV-00..03、MV-05..06 非生产语义栈完成；MV-07A/07B/07C 宿主接入完成（2026-08-24）
          ▶ 生产默认仍为 matvol 关闭，新预设 volumetric_nail_rgb_white_ondemand_lower_support 为显式 opt-in
          ⏸ MV-04 卡 MQ-01 壳层厚度（实测几何上限 0.30mm、推荐 0.228mm，未回签）
          ⏸ MV-08 生产接线依赖 MEMFLOW bounded/owned（MF-03B4/MF-04），
            且该分支按 2026-08-24 裁定暂缓合入，本分支暂无对应源文件
          卡 docs/codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md
TIFF      ⏸ 默认后端已切 libtiff，风险已关死（fail-closed+弃用告警+无静默回退）
          当前【无待办】：05B-02/03 延后并捆绑（等删除确认）、T-A-04 外部阻塞
          卡 docs/codex_task/current/TASKS_TIFF_默认后端切换与对齐根治任务清单.md
PRESET    ✅ PC-01..PC-04 全部完成（2026-09-04 全量回归 222 项 6 失败，全为既有，零新增）
            PC-01 摘 9 个纯别名 CTest 条目｜PC-02 T 通道派生收口＋两条门禁
            PC-03 补 RgbWhiteVarnish 工艺预设入口（六个材质策略里原先唯一没有预设的一个）
            PC-04 hd02_real_asset_matrix 改清单驱动并重固化 29/0/2 —— 自 2026-08-11 起首次转绿
          ⚠ 回归失败集由 7 降为 6，唯一变化就是 hd02；剩余 6 项全部为既有失败，见卡 §13
          ⚠ 全量 Debug 串行实测 992.5s，hd02 占 555.5s（56%），前 10 项占 867s（87%）。
            hd02 的 TIMEOUT 已由 900s 抬至 1800s（对实测 555/628s 原仅 1.43~1.62 倍余量）
          ⏸ PC-05..PC-12 待裁定；PC-07 开工前置已测（25 个文件 5 处不一致，≥2 处是误报）
            PC-12 断言实参求值顺序致失败不报原因，全仓 85 处；已证【非】机械改动，
            批量改写试过并回退，优先级低（只影响失败时的可诊断性）
          卡 docs/codex_task/current/TASKS_PRESET_工艺可配置面收敛与自测用例收口专项任务清单.md
          授权 docs/slice/DOC/DOC_DECISION_TEST_PRESET_2026_09_03_自测用例去别名与T派生收口授权.md
CI        ⏸ 用户 2026-08-10 裁决【暂缓】，清单保留不开工
          卡 docs/codex_task/current/TASKS_CI_冻结面工程保护任务清单.md
MEMFLOW   ▶ MF-00..03B4A COMPLETE；MF-03B4B PREPARED；生产仍为 Retained Dense
          卡 docs/codex_task/current/TASKS_16C_06_MEMFLOW_有界流式内存根治专项任务清单.md

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
- Canonical build directory: **`build-slicesoft/main`**（CMakePresets 预设 `slicesoft-main`）。
  构建 `cmake --build build-slicesoft/main --config Debug`，
  回归 `ctest --test-dir build-slicesoft/main -C Debug --output-on-failure`，当前共 **234** 项
  （generate 后按 CTestTestfile 实测。旧文写「213 项」已过期；2026-09-03 摘除 9 个
  纯别名条目后由 243 降为 234，见
  `docs/slice/DOC/DOC_DECISION_TEST_PRESET_2026_09_03_自测用例去别名与T派生收口授权.md`）。
  ⚠ 仓库根下的 `build/` 是**陈旧目录**：无 vcpkg toolchain、`meshoptimizer_DIR-NOTFOUND`、
  且不含任何 matvol 目标，重配置会直接失败（find_package(meshoptimizer CONFIG REQUIRED)）。
  它残留的 CTestTestfile 仍能让 ctest 跑起来并对陈旧二进制报出与基线不符的结果，
  务必不要用它验证任何改动。
- ⚠ 构建与回归必须分开判定退出码：`cmake --build ... | tail` 之类的管道会用管道末端命令的
  退出码掩盖真实的构建失败，而失败的配置会让随后的 ctest 对陈旧二进制报 PASS。
  先确认构建退出码为 0，再相信任何 ctest 结果。

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
8b. **提交之后、判定回归之前，必须全量重建**（`cmake --build <dir> --config <cfg>`，不加 `--target`）。版本号第三段由 `git rev-list --count <最近 v* 标签>..HEAD` 派生，提交即变；只重建部分目标会让已重建的拿新版本、未重建的留旧版本，跨二进制比对版本的测试随即变红，**而红灯与真回归长得一模一样**。若失败信息提到引擎版本、`--version` 漂移或 manifest 版本不一致，先全量重建复跑，再判定是否为真回归。
8c. 回归结果与基线对照要比**失败集合**，不要比失败**数量**。数量相同可能是「新增 N 条、消失 N 条」；本仓已发生过把新增失败误认成基线项的情况。
9. Commit only when the user asks or when the active task explicitly requires it; do not push unless explicitly instructed.
10. New commits must use `type(scope): 【功能分类】中文摘要`; use Chinese body items such as `【模块】`, `【验证】`, and `【边界】`. Do not rewrite published remote history solely to restyle old messages.
11. **新建分支必须遵守 `docs/git/Git 分支与发布规范.md`。** 硬约束五条，细节见该文：
    a. 命名 `codex/feature-<专项slug>-<短描述>` 或 `claude/feature-<专项slug>-<短描述>`；`<专项slug>` 取本文「各专项状态」里的 slug，**没有对应专项就先立一条**——分支不该比专项先存在。
    b. **功能分支从 `develop` 拉、合回 `develop`**；`develop` 在里程碑时合入 `product/packaged-slicer`。热修复从 `product/packaged-slicer` 拉，合回 product 与 develop 两侧。**不要从 `main` 拉**——它只是 `origin/HEAD` 指向的入口，跟随 product 而非领先它。**`main` 上永不直接提交**，它只由发布流程末尾的快进更新。
    c. **三层分支对应三档闸门**（耗时差一个量级，故分层）：功能分支自测跑 `slicesoft-debug-core`（155 项、12~18 秒）；合入 `develop` 跑 `slicesoft-debug-fast`（262 项、约 3 分钟）；**合入 `product/packaged-slicer` 必须跑全量**——`slicesoft-debug-full`（270 项、约 16 分钟）外加**全量重建**（见 8b）与**字节级基线 PASS**（`scripts/CaptureSliceOutputBaseline.py --verify`），并按**失败集合**比对（见 8c）。没有 CI 也没有 PR 评审，验证就是唯一闸门。**一条豁免**：改动完全落在 `*.md` / `docs/` / `analysis/` 之内时，进 `develop` 只需核心档；但 `develop` → `product` 那一跳**永不豁免**。**失败集合比对时，未归因的新失败挡住合入、已归因的已知抖动项放行但须留痕**；「归因」要有隔离重跑的分布、具体判据、以及为何与本次改动无关三样，缺一即按未归因处理（当前已知抖动项与门槛见规范第三节）。
    d. 改写历史前**必须建 `backup/pre-<原因>-<日期>` 备份**，改写后用`git diff --diff-filter=A --name-only HEAD <backup>` 验证零内容丢失（应为空）；合入并**推送成功后**才可删备份。**绝不 force-push 任何在 origin 上存在的分支。**
    e. 删分支先用 `git branch -d`（安全模式）。它在「已并入 HEAD 但未并入自身 origin 上游」时会拒绝——那正是需要人看一眼的情形；确要删则先逐条验证提交全部可达，再 `-D`。

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

## Stage 16D-02 Qt diagnostics context (2026-08-13)

- Reference Host exposes S0 production-default and S3 diagnostic-candidate geometry sampling; S3 remains restricted to `relief_heightfield` and never silently replaces S0.
- Workspace schema v5 persists the explicit strategy. Unknown or stale values fail safe.
- Qt only presents effective Profile, Worker timing, manifest statistics and package-rendered TIFF previews; it must not recompute geometry or posture.
- Result A/B means first production layer versus current production layer using the same package/channels. P0 remains production posture and P3 remains diagnostic-only until separately authorized.
