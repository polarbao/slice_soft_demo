# REPORT_14B-06 CI 行数与结构门禁

> 状态：**COMPLETE / GATE ACTIVE**  
> 日期：2026-08-05  
> 任务：14B-06  
> 依据：`INT_11_文件拆分与结构治理专项.md` §2.1

## 1. 交付结论

新增 `ValidateSourceSizeGuard.py` 与机器配置 `SourceSizeGuardConfig.json`，并接入
`run_ci_quick.ps1` 和 CTest。门禁采用增量比较，不要求为历史长文件做机械拆分：

| 规则 | 行为 | 结果 |
|---|---|---|
| G1 | 新建 `.c/.cpp/.h` 等源文件不超过 500 行 | 违反即失败 |
| G2 | 基线已超过 1000 行的文件只减不增 | 净增长即失败 |
| G3 | 新建头文件不超过 200 行 | 违反即失败 |
| G4 | `.cpp > 800` 且同名头不足 100 行 | 告警 |
| G5 | 文件不超过 19 行且 30 天无增长 | 告警 |

CI/PR 必须把目标分支或 merge-base 通过 `--base-ref` 或
`SLICESOFT_LINE_GUARD_BASE` 传入。未指定时，本地快速回归使用 `HEAD^`，用于检查最近
原子提交；显式 `--base-ref HEAD` 可只检查未提交改动。

## 2. 白名单

白名单条目必须同时包含规则、理由和到期条件。受保护的新目录禁止加入白名单：

```text
src/slicer_core/api/
src/slicer_module/
apps/slicer_worker/
apps/slicer_ui_host_sim/
```

初始白名单只登记两个既有 UI 大文件，且仅豁免 G4 比例告警；G2 的只减不增仍然生效：

| 文件 | 当前实测 | 到期条件 |
|---|---:|---|
| `apps/slicer_debug_ui/MainWindow.cpp` | 4267 行 | 14E-05 COMPLETE |
| `apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp` | 7401 行 | 14E-05 COMPLETE |

文档中原先的 3659/6963 是 2026-08-02 历史测量值，不再作为当前基线。14E-05 完成时
必须删除这两个条目，脚本会继续按普通规则检查拆分后的文件。

## 3. 验证

```powershell
python scripts/ValidateSourceSizeGuard.py --self-test
python scripts/ValidateSourceSizeGuard.py --base-ref HEAD^
ctest --test-dir build-slicesoft/main -C Debug -R slicer_source_size_guard --output-on-failure
powershell -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1 -SourceGuardBaseRef <merge-base>
```

自测包含 G1/G2/G3 正向触发和 G2 缩减放行。当前仓库全树扫描产生 G4/G5 告警但不
阻断；这些告警是后续按边界治理的输入，不得通过扩大白名单消音。

## 4. 边界

- 本任务未拆分任何生产文件，也未修改算法、TIFF、ABI 或 Qt UI。
- 新增 api/module/worker/参考宿主从第一天起受 G1/G3 约束。
- 门禁检查提交增量；正式 CI 必须使用目标分支 merge-base，不能固定用 `HEAD` 绕过。

