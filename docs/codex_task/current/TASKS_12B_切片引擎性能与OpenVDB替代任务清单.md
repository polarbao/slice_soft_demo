# TASKS_12B_切片引擎性能与OpenVDB替代任务清单

> 文档版本：v0.1
> 文档状态：Task List / Stage 12B
> 生成日期：2026-07-05

---

## 任务边界

12B 只处理 core-only 性能评估、OpenVDB 替代 gate、引擎路线判断和 benchmark 工程化。
不修改生产材料语义，不默认启用 OpenVDB，不改变 RGBWSV 协议。

---

## 原子任务

### Task 12B-01 Benchmark 契约确认

状态：PENDING

内容：

```text
确认 coreComputeMs / ioWriteMs / previewWriteMs / endToEndMs 的统计边界。
```

验证：

```powershell
git diff --check
```

### Task 12B-02 same-pose benchmark 配置

状态：PENDING

内容：

```text
让 legacy/openvdb 使用同模型、同姿态、同 grid、同 layerThickness。
```

验证：

```text
benchmark JSON 中 samePose=true。
```

### Task 12B-03 Release core-only 脚本

状态：PENDING

内容：

```text
新增 run_12b_core_benchmark.ps1 或扩展现有 11B benchmark。
```

验证：

```powershell
.\scripts\run_12b_core_benchmark.ps1 -BuildType Release -Engine legacy -NoImageWrite
```

### Task 12B-04 OpenVDB 语义可比性报告

状态：PENDING

内容：

```text
输出 outputSemanticsComparable=false 的具体原因，而不是只报失败。
```

验证：

```text
OpenVDB candidate 不可比较时有 failureReason。
```

### Task 12B-05 Legacy 优化候选实验

状态：PENDING

内容：

```text
评估 z bucket / active edge / tile cache / thread pool 中至少一个低风险优化。
```

验证：

```text
Release coreComputeMs 对比。
```

### Task 12B-06 Heightfield Fast Path 预研

状态：PENDING

内容：

```text
为甲片 2.5D 模型建立 topHeight/bottomHeight fast path 可行性报告。
```

验证：

```text
同模型 mask 与 legacy 差异小于阈值。
```

### Task 12B-07 OpenVDB Hybrid 定位

状态：PENDING

内容：

```text
判断 OpenVDB 是否改为 outer varnish / SDF shell / clearance 工具模块。
```

验证：

```text
输出 DOC_DECISION 或更新 REPORT_12。
```

---

## 完成标准

```text
1. 有 Release core-only benchmark；
2. 有 OpenVDB replacement gate 结论；
3. 有至少三条高性能路线比较；
4. 用户可明确知道下一步优化投向。
```
