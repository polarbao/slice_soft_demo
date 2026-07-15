# 12C-R2-04-00 OpenVDB Utility 摘要准入交接

## 1. 当前状态

```text
12C-R2-03 DiagnosticsDock 已完成并提交：8c31913；
R2-04 代码尚未开始；
R2-04 准入契约已冻结，可以进入实现。
```

## 2. 已确认事实

```text
schema：slicesoft.openvdb_sdf_utility.12b_r2.1；
真实 OFF 报告：output/benchmarks/12b_r2_openvdb_sdf_utility_off.json；
真实 ON 报告：output/benchmarks/12b_r2_openvdb_sdf_utility_on.json；
ON 报告的壳层/拓扑 pass 只表示 Utility 结果，不是生产切片 PASS；
decision.productionReplacementAllowed 必须为 false；
OpenVDB 仍为可选 utility/candidate，Legacy 仍为默认生产路径。
```

## 3. 冻结实现方案

```text
展示位置：DiagnosticsDock 的现有 ReportPanel；
输入：package/reports 自动发现 + 显式加载独立 JSON；
解析：新增 OpenVdbUtilityReportInterpreter service；
校验：schema、outputPolicy、utilities、decision 和枚举严格校验；
展示：中文角色、可用性、utility 状态、推进建议、blockers/issues、Legacy 保护；
负向：bad schema 与 replacement=true 必须明确无效；
Smoke：openvdb-utility-summary，临时生成 ON/OFF/bad fixtures。
```

## 4. 明确边界

```text
不运行或修改 OpenVDB probe；
不改变 Legacy 生产切片；
不修改 RGBWSV、TIFF、preview 或 package schema；
不实现 clearance/material closure；
不把 utility pass 显示为生产可打印；
不进入 12D。
```

## 5. 下一原子任务

```text
实现 12C-R2-04 OpenVDB Utility/Candidate 摘要并运行冻结的 Smoke/回归命令。
```

详细准入：`docs/slice/DOC/DOC_CHECKLIST_12C_R2_04_OpenVDBUtility摘要准入.md`。
