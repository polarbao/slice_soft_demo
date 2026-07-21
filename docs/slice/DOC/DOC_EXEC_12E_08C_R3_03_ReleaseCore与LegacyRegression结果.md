# DOC_EXEC_12E-08C-R3-03 Release Core 与 Legacy Regression 结果

> 文档状态：COMPLETE / NON-PRODUCTION
> 日期：2026-07-21
> 分支：`feature/12e-08c-mesh-repair`

## 1. 任务结论

R3-03 已完成 Release repair 证据、geometry-admitted global core、repair-disabled TIFF invariant、RIP
strict 与 Quick CI 归因。四个 required case 均有 Release 记录，但只有闭合 Texture2D 3MF 被允许执行
partition -> texture transfer -> raster mapping -> full closure。

三个真实 OBJ 均按 frozen strict 结论记录为 `skipped_due_topology`；未执行阶段使用 `null` 计时。任务证据
COMPLETE，Release budget 和 production admission 仍为 BLOCKED。

## 2. 实现内容

```text
Release benchmark 扩展为完整内存诊断链；
纹理传递、true-Z raster mapping、RGBWSV 内存闭环均不写生产包；
新增 R3-03 expectation golden、汇总 Schema 和自动化脚本；
固定 generated 3MF ZIP entry 时间，Quick CI 重建 fixture 后 source SHA-256 保持稳定；
脚本支持复用已验证 core/Quick CI 日志，但默认执行完整证据链；
Quick CI 已知差异单独分类，不静默改 golden。
```

global full closure 使用当前 `R G B W S V`、`0=打印/255=不打印` 语义构造内存证据。白色纹理或
model fill 的诊断占用使用 W 通道表示，不调用 TIFF writer。

## 3. Release 结果

| case | repair | global core | pre diag/eligibility | global core | peak working set |
|---|---|---|---:|---:|---:|
| `nai_you_new` | rejected self-intersection | skipped | 656.8189 ms | null | 95,154,176 B |
| `aishen_fudiao` | rejected self-intersection | skipped | 640.8506 ms | null | 69,832,704 B |
| `meigui_fudiao` | rejected self-intersection | skipped | 646.4811 ms | null | 97,263,616 B |
| Texture2D 3MF | strict no-op PASS | completed | 0.0733 ms | 17.0049 ms | 8,138,752 B |

3MF global core 分段：

```text
import/config/model/adapt：5.3994 ms；
repair no-op：0.0934 ms；
attribute/post-strict/hash：0.1608 ms；
partition core：2.2493 ms；
texture transfer：14.4905 ms；
raster mapping：0.0893 ms；
full closure：0.1758 ms；
fullClosurePass=true；
productionOutputWritten=false。
```

计时为本机单次 Release 证据，不是冻结 SLA。

## 4. Legacy 与协议结果

```text
repairEnabled default=false；
repair-disabled baseline/diagnostic TIFF SHA-256 invariant：PASS，30/30 层；
RIP Reader strict：2/2 PASS；
manifest：p0.rgbwsv.2；
channelOrder：R G B W S V；
bitDepth：8；
polarity：black_is_print；
OpenVDB：OFF。
```

Quick CI 实际执行后仍被既有 baseline 阻断：`material_process_top2 widthPx expected=48 actual=226`。
该差异记录为 `failed_known_baseline`，未更新 golden，也不冒充全回归 PASS。

## 5. 输出与验证

汇总：

```text
output/benchmarks/12e_08c_r3_03_release/release_core_summary.json
schema=slicesoft.mesh_repair_release_evidence.12e_08c_r3_03.1
```

已运行：

```powershell
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r3_03_release_evidence.ps1 -BuildDir build -Config Release
```

Release build PASS；CTest 37/37 PASS；R3-03 汇总完成并如实记录 Quick CI 已知 blocker。

## 6. 边界

```text
没有绕过 confirmed/coplanar self-intersection；
没有新增通用 remesh 或第三方修复库；
没有 global -> legacy 隐式回退；
没有 global production TIFF/package；
没有修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print；
repair 和 OpenVDB 仍默认关闭。
```

## 7. 下一步

R3-04 可进行 GO/NO-GO 决策。按当前证据，三个 required OBJ 未 strict PASS、Release budget 未冻结，
因此预期结论为 12E-08D NO-GO。
