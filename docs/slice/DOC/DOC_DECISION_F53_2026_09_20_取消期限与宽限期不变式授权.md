# DOC_DECISION 取消期限与宽限期不变式授权范围

> 文档状态：**ACTIVE / AUTHORIZED**
> 版本：v1.0 ｜ 日期：2026-09-20
> 授权：用户 2026-09-20「关于`阈值`问题，可根据你的建议进行处理」。
> 该授权承接 2026-09-18 的定位结论（用户当时选择「先查 F-53 取消不到终态」，
> 并明确阈值改动需另行授权）。
> 上游：`analysis/04_问题清单与改动空间.md` 的 **F-53 根因定位**与 **F-56**
> 相关先例：`DOC_DECISION_MATVOL_MV_07_Q1_宿主白区门放宽授权.md`（授权文档形状）
> 涉及专项：**stage14e**、**hostflow**、**stage14d_07**（门禁归属方）；
> 执行方：**P0FIX**（发现登记方）

---

## 1. 这不是「放宽门禁」，是纠正一个假断言

本仓纪律要求门禁放宽须先出授权文档。**本次改动需要先说清它不属于放宽**，
否则纪律本身会被这次改动稀释。

门禁断言的是「取消在 2000ms 内到达终态」。而模块的设计**明确保证的是相反的事**：

```cpp
// src/slicer_module/WorkerClient.cpp:344
if (cancellationStarted.has_value() && now - *cancellationStarted >= options.cancelGracePeriod)
{
    result.forcedTermination = true;
    TerminateJobObject(job.Get(), CancelledProcessExitCode);
}
```

worker 不自行退出时，模块**先等满 `cancelGracePeriod` 才强杀**；
而作业状态要等 `client->Run()` 返回才转终态（`WorkerJobService.cpp:474`）。
所以取消到终态的设计上界是 **`cancelGracePeriod` + 强杀与上报开销**。

而 `cancelGracePeriod` 恰好也是 **2000ms**，且被 `WorkerClient.cpp:228` 的校验
**锁死在 2000 这个上限**（`> 2000` 即拒绝）。

**于是门禁要求的期限等于设计保证的下界，余量为负。**
它断言的是系统从未承诺过的事。**把它改对不是放宽，是修正。**

## 2. 证据：读数是双峰的，中间一次都没有

同一二进制、隔离连跑，读数取自各次 `capability_coverage.json`：

| 路径 | `latencyMs` | `terminalState` | 门禁结果 |
|---|---|---|---|
| worker 自行退出 | **12** | `cancelled` | 通过 |
| worker 未自行退出（走强杀） | **2053 ~ 2190** | `cancelling` | 失败 |

**没有任何一次落在 12 与 2053 之间。** 这两段不是同一个量的抖动，
而是两条不同的代码路径：前者命中
`apps/slicer_worker/runtime/WorkerJobDispatcher.cpp:89` 的早期取消检查，
后者错过检查窗口、由模块强杀收场。

初版诊断「余量约 1%」正是把这两段读数混在一起算分布得出的，**已作废**。

## 3. 授权范围（只改这些）

| 文件 | 改动 |
|---|---|
| `src/slicer_module/WorkerClient.h` | 字面量 `2000` 提为具名常量 `kDefaultCancelGracePeriod`，并写明「任何取消期限门禁都必须大于它」。**零行为变化**，值不变 |
| `src/slicer_module/WorkerJobService.cpp` | 显式设置点改为引用该具名常量，不再各写一个 `2000` |
| `tests/stage14d_07/EngineConformanceGate.cpp` | 期限**从宽限期推导**：`kCancelDeadlineMs = kCancelGraceMs + kForcedTerminationBudgetMs`，并加 `static_assert` 锁死大小关系；两处 `<= 2000.0` 改为 `<= kCancelDeadlineMs` |
| `apps/slicer_ui_host_sim/CapabilityCoverageRunner.cpp` | `kCancelLatencyLimitMs` 2000 → **3000**，注释写明不变式与为何不能 include 模块头 |
| `tests/contracts/ValidateCancelDeadlineInvariant.py` | **新增**契约门禁，见 §5 |
| `CMakeLists.txt` | 注册 `cancel_deadline_invariant_test` 与 `cancel_deadline_invariant_self_test` 两条 ctest |

> **比初稿多做了一步**：初稿只打算把两处数字各自抬高。实际实现时发现
> `tests/stage14d_07/EngineConformanceSupport.h` 已经 include 了
> `slicer_module/WorkerClient.h`，**那一侧可以直接从宽限期推导**，
> 于是把字面量提成具名常量，让 stage14d_07 的期限在编译期跟随宽限期。
> 这样即使将来有人调整宽限期，该侧也不会再漂移。宿主侧做不到（见 §5），仍靠脚本看守。

### 3000 这个数的来历

```
2000   cancelGracePeriod（WorkerClient.h:79，且被校验锁死在此上限）
+ 190  实测强杀路径的最大附加开销（2190 − 2000）
+ 810  余量
─────
3000
```

余量取得比实测开销大得多，有两条具体理由：

1. **测得的 `latencyMs` 里混入了无关耗时**：`cancelTimer` 在测试自己的
   残留递归遍历期间**仍在计时**（`CapabilityCoverageRunner.cpp` 的 `QDirIterator`）。
   而该遍历的耗时随证据目录累积而增长——见 F-56，**该缺陷尚未修复**。
   在它修好之前，余量必须能吸收这部分漂移。
2. 本机实测 F-56 累积到 1690 条目时单次总耗时从 15 秒涨到 41 秒。
   余量若只取 300ms，累积一段时间后会重新变红，等于没修。

**F-56 修复后应重新收窄该余量**，并把依据一并更新到本文。

## 4. 明确不改的部分

| 项 | 为什么不动 |
|---|---|
| `cancelGracePeriod` 本身（2000ms） | 缩短它会减少 worker 收尾时间，而**同一条门禁还检查 `residues.isEmpty()`**，可能直接把残留检查打红。要改须先测残留率，不在本次授权内 |
| 门禁判据的形态 | 「区分自行退出与强杀两条路径、分别给期限」语义最正确，但跨三个专项、改动最大，留待后续 |
| F-56 的证据目录清理 | 归 stage14e，本次不代为承接 |
| 取消路径本身的任何生产代码 | 本次只动门禁常量与新增校验脚本 |

## 5. 新增不变式门禁（防止两个数再次各改各的）

根因之所以能长期存在，是因为**宽限期与门禁期限写在三个互不相干的文件里**，
没有任何机制保证它们的关系。宿主经 `LoadLibrary` + C ABI 加载模块，
**不能 include 模块内部头**（那会破坏三进程拓扑），所以无法共享编译期常量。

改用本仓既有的契约门禁脚本形态（同 `ValidateConfigConsumption.py`）：

`tests/contracts/ValidateCancelDeadlineInvariant.py` 解析三处常量并断言

```
门禁期限 > cancelGracePeriod
```

任一处被改动而破坏该关系时，这条门禁变红并指出是哪一处。

## 6. 验证结果（2026-09-20 实测）

**① 三条受影响测试由红转绿** —— 全部通过：

```text
cancel_deadline_invariant_self_test ...........   Passed   0.29 sec
cancel_deadline_invariant_test ................   Passed   0.17 sec
slicer_stage14d07_r2_engine_conformance_test ..   Passed  36.40 sec
slicer_stage14e04b_capability_coverage_test ...   Passed  14.11 sec
hostflow_ha03_qt_end_to_end ...................   Passed  11.20 sec
```

**② 新门禁已被证伪**（`--self-test`，四种违规形态逐一确认被挡住）：

| 构造的违规 | 结果 |
|---|---|
| 门禁期限等于宽限期（F-53 原始形态） | 已被挡住 |
| 门禁期限小于宽限期 | 已被挡住 |
| stage14d_07 退回字面量比较 | 已被挡住 |
| 强杀预算被改为 0 | 已被挡住 |
| 未破坏的仓库 | 判绿（证明它有区分力，不是恒红） |

该自测已注册为 ctest，**每次回归都会重跑**，不是一次性人工检查。

**③ 门禁未失去检出能力** —— 故障注入验证。
在 `WorkerClient.cpp` 的强杀路径后临时插入 1500ms 延迟并重建模块，UI-M5 读数：

```json
{"latencyLimitMs":3000, "latencyMs":3567, "passed":false, "terminalState":"cancelling"}
```

**门禁变红**。抬到 3000ms 之后它仍然挡得住真实的取消路径劣化。
注入已撤销，`WorkerClient.cpp` 与 HEAD **逐字节一致**、无残留。

**④ 闸门**：合入集成线走快集档；合入 `main` 走全量档 + 字节级基线。

---

## 7. 后续（不在本次授权内）

- **F-56 修复后应重新收窄 §3 的余量**：现在的 810ms 余量里有一部分是替
  「`latencyMs` 混入残留遍历耗时」买的单。证据目录清理落地后，
  这部分可以收回，届时更新本文 §3 的推导。
- **宽限期本身是否该缩短**（选项 ②）仍未评估，须先测残留率。
- **门禁判据是否该区分两条路径**（选项 ③）语义最正确，留待后续。
