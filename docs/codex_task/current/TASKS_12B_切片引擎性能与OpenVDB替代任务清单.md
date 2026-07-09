# TASKS_12B_切片引擎性能与OpenVDB替代任务清单

> 文档版本：v0.1
> 文档状态：Task List / Stage 12B
> 生成日期：2026-07-05

---

## 任务边界

12B 只处理 core-only 性能评估、OpenVDB 替代 gate、引擎路线判断和 benchmark 工程化。
不修改生产材料语义，不默认启用 OpenVDB，不改变 RGBWSV 协议。

## 当前拆分

12B 已拆分为 R0/R1/R2 三段执行：

```text
12B-R0：Benchmark 契约、真实模型 Release core-only 对比、OpenVDB replacement gate 结论；
12B-R1：Legacy 优化和 2.5D heightfield fast path 小型原型；
12B-R2：OpenVDB hybrid/SDF utility 定位。
```

当前执行入口：

```text
R0/R1 已完成；R2 已开启，当前入口：
docs/codex_task/current/TASKS_12B_R2_OpenVDB_SDFUtility定位任务清单.md
```

正式拆分决策：

```text
docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md
docs/slice/DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md
docs/slice/ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md
docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md
docs/slice/DEV/DEV_12B_R1_LegacyHeightfield优化原型设计.md
docs/slice/REPORT/REPORT_12B_R1_LegacyHeightfield优化当前状态.md
docs/slice/PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md
docs/slice/DEV/DEV_12B_R2_OpenVDB_SDFUtility评估设计.md
docs/slice/DEMO/DEMO_12B_R2_OpenVDB_SDFUtility验证方案.md
docs/slice/REPORT/REPORT_12B_R2_OpenVDB_SDFUtility启动状态.md
```

---

## 原子任务

### Task 12B-01 Benchmark 契约确认

状态：DONE

内容：

```text
确认 coreComputeMs / ioWriteMs / previewWriteMs / endToEndMs 的统计边界。
```

验证：

```powershell
git diff --check
```

完成记录：

```text
已通过 R0 固化 slicesoft.benchmark.12b.1；
见 docs/slice/DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md。
```

### Task 12B-02 same-pose benchmark 配置

状态：DONE

内容：

```text
让 legacy/openvdb 使用同模型、同姿态、同 grid、同 layerThickness。
```

验证：

```text
benchmark JSON 中 samePose=true。
```

完成记录：

```text
R0 使用三组真实模型 legacy/openvdb 对比配置；
OpenVDB replacement gate 仍为 false。
```

### Task 12B-03 Release core-only 脚本

状态：DONE

内容：

```text
新增 run_12b_core_benchmark.ps1 或扩展现有 11B benchmark。
```

验证：

```powershell
.\scripts\run_12b_core_benchmark.ps1 -BuildType Release -Engine legacy -NoImageWrite
```

完成记录：

```text
脚本已落地：scripts/run_12b_core_benchmark.ps1；
R0/R1 均使用该脚本输出 Release core-only benchmark。
```

### Task 12B-04 OpenVDB 语义可比性报告

状态：DONE

内容：

```text
输出 outputSemanticsComparable=false 的具体原因，而不是只报失败。
```

验证：

```text
OpenVDB candidate 不可比较时有 failureReason。
```

完成记录：

```text
R0 已输出 outputSemanticsComparable=false；
结论为 OpenVDB 不能替代 legacy production path。
```

### Task 12B-05 Legacy 优化候选实验

状态：DONE

内容：

```text
评估 z bucket / active edge / tile cache / thread pool 中至少一个低风险优化。
```

验证：

```text
Release coreComputeMs 对比。
```

完成记录：

```text
R1 已选择 support generation path；
support.shape.enabled=false fast path 在三组真实模型上完成 before/after。
```

### Task 12B-06 Heightfield Fast Path 预研

状态：DONE

内容：

```text
为甲片 2.5D 模型建立 topHeight/bottomHeight fast path 可行性报告。
```

验证：

```text
同模型 mask 与 legacy 差异小于阈值。
```

完成记录：

```text
R1 已输出可行性评估；
结论为当前 relief_heightfield 已经是 column z_min/z_max 路径，maskSamplingMs 不是瓶颈；
R1 不继续实现独立 2.5D fast path。
```

### Task 12B-07 OpenVDB Hybrid 定位

状态：IN_PROGRESS

内容：

```text
判断 OpenVDB 是否改为 outer varnish / SDF shell / clearance 工具模块。
```

验证：

```text
输出 DOC_DECISION 或更新 REPORT_12。
```

完成记录：

```text
R2 文档准入已完成；
当前执行入口为 TASKS_12B_R2_OpenVDB_SDFUtility定位任务清单.md；
R2-01 当前 OpenVDB utility 代码盘点已完成；
R2-02 Utility Report Schema 已完成；
R2-03 OpenVDB OFF 默认轨道保护已完成；
R2-04 OpenVDB ON Smoke 与可用性报告已完成；
R2-05 Utility Capability Matrix 已完成；
后续需继续执行 R2-06 最小 Utility Report 原型。
```

---

## 完成标准

```text
1. 有 Release core-only benchmark；
2. 有 OpenVDB replacement gate 结论；
3. 有 legacy 支撑路径优化 before/after；
4. 有 heightfield fast path 是否继续的判断；
5. 用户可明确知道下一步优化投向。
```
