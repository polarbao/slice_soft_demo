# TASKS_TIER1 验证工装税消除专项

**分支**：`codex/feature-p0fix-contract-robustness`
**立项依据**：`analysis/04_问题清单与改动空间.md` 的「下一阶段优先级」第一梯队，
以及 `analysis/06_改进路线图与验证方案.md` 重排后的「时序建议」。
**基线**：P0FIX 收口回归 250 项 / 7 失败 / 1561.51 s（`output/p0fix/p0-07-final2-ctest.log`）。

## 为什么这三条排在最前

这三条**没有一条在修 bug**，用户也看不见它们。排在最前的理由是 P0FIX 专项把代价实测出来了：

P0-07 的爆炸半径四轮才收敛，贵不在问题本身，而在 F-44 让「跑一轮回归看它点名」
这个最自然的做法不可用——坏 fixture 挂死吃满超时，一轮回归作废重跑；被迫转向静态枚举，
而静态枚举给出 87 个可疑、实际只有 5 个真坏，**过拟合 17 倍**，两头都不准。
F-42 同期复现 3 次，每次都伪装成真回归先当真问题查了一轮。

不修这三条，第二至第四梯队每一条都要多花约一倍时间；而第四梯队的 F-09
（`slicer.cpp` 拆解，8 步每步独立验证）在没有安全网的情况下根本不该开工。

## 任务状态

| 卡号 | 状态 | 完成日期 | 验证结果 |
|---|---|---|---|
| T1-01 F-42 版本重配监视失效（根因修复） | COMPLETE | 2026-09-15 | 见下 |
| T1-02 F-41 超时余量不足 + 内部期限（根因定位 + 修复） | COMPLETE | 2026-09-15 | 见下 |
| T1-03 F-17 + F-44 共享 `Expect.h` + 试点迁移 | COMPLETE | 2026-09-15 | 见下 |
| T1-04 收口：全量回归对照 P0FIX 基线 | COMPLETE | 2026-09-15 | 250 项 / 7 失败 / 1660.75 s，失败集与基线**逐条一致、零新增零消失** |

## T1-01 F-42：版本重配监视失效

**原描述不准确，已更正。** 原文写「构建版本计数器」，暗示构建会递增版本；
实测版本第三段由 `git rev-list --count <最近 v* 标签>..HEAD` 派生，**递增的触发是提交而非构建**。

**根因**（`cmake/SliceSoftVersion.cmake`）：`CMAKE_CONFIGURE_DEPENDS` 监视
`.git/HEAD` 与 `.git/refs`，两条都不生效——

1. 普通提交**不改** `.git/HEAD`，它存的是分支指针 `ref: refs/heads/<分支>`，内容不变。
   实测本分支 `.git/HEAD` 停在 09-14 18:42（建分支时），其后 6 次提交无一触碰。
2. `.git/refs` 是**目录**。MSBuild 的自定义生成依赖只接受文件，对目录报
   MSB8064「指定的依赖项不存在」并使依赖失效——构建日志里每个目标刷一条。

于是提交后无任何监视项变化 → 不重新配置 → PATCH 停在旧值；此后只重建部分目标
就会新旧版本并存，跨二进制比对版本的测试变红，而红灯与真回归无法区分。

**修复**：改为监视三个**文件**——`.git/logs/HEAD`（HEAD 的 reflog，提交/切分支/reset/merge
均追加，是唯一覆盖「提交」的一项）、`.git/HEAD`（切分支）、`.git/packed-refs`（标签打包，
PATCH 由 `describe --tags` 派生故须覆盖）。三者均以 `if(EXISTS)` 守护。

**验证**：
- 改前已生成版本 `0.2.474-dev`；重配后 `0.2.480-dev`——**正好差 6，与自上次配置以来的
  6 次提交逐一对应**，陈旧量被精确量化。
- MSB8064 警告由「每个目标一条」降为 **0 条**。
- **端到端**：随后提交一次，再跑 `cmake --build` —— CMake **自动重跑配置**
  （普通构建里出现 `Configuring done`，修复前不会发生），版本 480 → **481**，恰好 +1。

**同时落的纪律**（`AGENTS.md` 新增 8b / 8c）：根因虽已修、配置会重算，但只重建部分目标
仍会新旧并存，故保留「提交后判定回归前必须全量重建」；另加「回归对照比失败**集合**
而非失败**数量**」——本仓已发生过把新增失败误认成基线项的情况。

## T1-02 F-41：超时余量不足与内部期限

**内部期限已定位**，落在上一轮预测的 117–154 s 窗口内。

`apps/slicer_ui_host_sim/HostUxSceneSmoke.h`：等切片作业 **120 000 ms**、等结果加载 **130 000 ms**。
空载实测整轮需 108~117 s——余量不足 1.1 倍。超过 120 s 后循环退出，走 `OnCancelSlice()`
并 **`return 6` 且不打印任何原因**，对外表现为「154 s 失败、无消息」：
既不是 ctest 超时（那是 180 s），也不像真回归，每次都要定向复跑才能定责。

**修复三处**：
1. 内部期限 120 s / 130 s → **480 s / 520 s**（约 4 倍余量）。
2. ctest `TIMEOUT` 180 s → **600 s**（`apps/slicer_ui_host_sim/CMakeLists.txt`）。
3. **让失败自述原因**：超时分支现在打印
   `SCENE_UI_SLICE_TIMEOUT waited_ms=… limit_ms=… reason=slice_job_still_active`。

**次序是刻意的**：内部期限（480/520 s）必须**小于** ctest 超时（600 s），
这样先触发的是带自述消息的那一条；让 ctest 先超时只会得到一条无信息的红灯。

## T1-03 F-17 + F-44：共享 `Expect.h`

**两条是同一个修法**，故合并为一项。新增 `tests/support/Expect.h`（头文件，无新依赖）：

- **断言宏**打印表达式原文与**双方实参**，解决 F-17（仓内约 85 处断言只打印一句文字）。
- **`RunCases` 逐例 try/catch** 并继续下一例，解决 F-44。实测本仓 **191 个含 `main()`
  的测试源文件中 99 个（52%）完全没有任何 catch**，遇未捕获异常挂住等 ctest 超时。

**四点设计取舍**：
- 断言失败**不抛异常**，用例跑完并报告全部失败，而非停在第一条。
- 每条 `RUN` 用 `std::endl` 强制刷新——万一真被 SEH 带走，日志里最后一条 RUN 就是罪魁。
- 写 stderr 前先 `std::cout.flush()`，否则 ASSERT/FAIL 会整体排到收尾汇总行之后、与 RUN 错位。
- **边界**：`catch(...)` 接不住 Windows 结构化异常（访问违例、`EXCEPTION_STACK_OVERFLOW`），
  那类崩溃仍会带走进程；本头只解决 C++ 异常这一类。已写进头文件注释。

**试点迁移两个文件**（均为 P0FIX 期间我自己新写、且都带着 F-44 缺陷）：
`tests/unit/worker_protocol_bounds/Main.cpp`（纯 C++）与
`tests/hostflow/HostSliceProtocolRouteTests.cpp`（Qt）。两者覆盖了两类构建形态。

**证伪（八项全过）**：临时注入三条探针用例——一条断言必败、一条必抛、一条验证跑法未中断：

```
ASSERT …Main.cpp:49  deliberate: …  |  actual = 2048, expected std::size_t{4096} = 4096
FAIL DELIBERATE_assertion_failure_must_print_actuals 1 assertion(s)
FAIL DELIBERATE_throw_must_not_hang_and_must_continue exception=deliberate: thrown from inside a case
PASS case_after_the_throw_must_still_run        ← 抛异常后【继续】跑下一例
```

实参、期望值、表达式原文、文件行号齐全；异常被捕获并带 `what()`；跑法未中断；退出码非零。

**过程中的一处自伤已记录**：还原探针时用 `shutil.copy2` 保留了旧 mtime，
MSBuild 判定「已是最新」跳过重编，跑的仍是带探针的二进制，一度误以为迁移引入了真失败。
判别法：源文件 mtime 早于对应 `.obj` 即是此症。

**gitignore 陷阱已规避**：`tests/support/` 原本不在 `.gitignore` 的放行名单内（F-46），
新建的 `Expect.h` 本会被静默忽略——本地全绿、他人拉取后因文件不存在而编译失败。
已加 `!tests/support/` 放行并复核。

## T1-04 收口

见文末「回归对照」。

## 回归对照

**全量构建**退出码 0、0 error；**MSB8064 警告 0 条**（F-42 的修复在整个构建范围内成立，
修复前是每个目标一条）。日志 `output/p0fix/tier1-build.log`。

**全量回归 250 项 / 7 失败 / 1660.75 s**（`output/p0fix/tier1-ctest.log`）。
对照口径为比**失败集合**而非失败数量（`AGENTS.md` 8c）：

```
基线（P0FIX 收口 1561.51 s）                本轮（1660.75 s）
scene_layer_adapters_unit_tests               同
slicer_stage14b_layering_feasibility_test     同
slicer_stage14c04_sync_capability_safety_test 同
slicer_stage14e02_qt_host_boundary_test       同
slicer_stage14e04d_dual_view_contract_test    同
stage14f03_single_model_s1_gate               同
stage16c06_bounded_support_shape_unit_tests   同

新增失败：0    消失失败：0
```

**F-41 的余量实测**：两条 hostux 测试本轮耗时 **76.92 s** 与 **75.98 s**。
相对新的 480 s 内部期限余量约 **6.2 倍**、相对 600 s ctest 超时约 **7.8 倍**；
修复前分别是约 1.6 倍与 2.4 倍。

**F-44 的剩余面实测**：`tests/` 下含 `main()` 的 191 个测试源文件中，
经 `Expect.h` / `RunCases` 保护 **2** 个（本次试点）、自带 catch **92** 个、
**仍无任何保护 97 个**。收益按迁移进度兑现，当前只兑现了一小部分。

> 注：统计判据必须同时认「自带 catch」与「经 RunCases 保护」两种。
> 迁移后的文件自身不再出现 `catch(`，只按 `catch` 计数会把它们误判为无保护——
> 首次统计就踩了这个，数字一度停在 99 没动。

## 边界与未做

- **只做了两个试点文件**，余下 97 个无 catch 的测试 main 未迁移。批量迁移不是机械改写
  （`AGENTS.md` PC-12 记录「批量改写试过并回退」），应逐个随手改，不另立批量任务。
- F-42 的根因已修，但**残余风险仍在**：配置会重算，只重建部分目标仍会新旧并存，
  故 `AGENTS.md` 8b 的纪律保留。
- F-41 抬高了期限，但**没有解决「为什么这一轮要跑 110 秒」**——那是性能问题，不在本专项。
- 第二梯队（TEXFAIL、F-46 余下 24 个目录、F-45、F-43）未开工。

## 修订记录

- 2026-09-15：立项并完成 T1-01~T1-04。F-42 由「写纪律绕过」升级为根因修复；
  F-41 内部期限定位成功；F-17 与 F-44 合并为一个修法并完成两个试点。
