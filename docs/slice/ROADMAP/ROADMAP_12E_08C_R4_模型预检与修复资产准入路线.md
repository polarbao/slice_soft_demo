# ROADMAP_12E-08C-R4 模型预检与修复资产准入路线

> 文档状态：R4-07-R1 READY / RESTRICTED PRODUCTION CANDIDATE 2 FAMILIES
> 初始日期：2026-07-22
> 准入规则修改时间：2026-07-23 11:32 +08:00
> 插入位置：R3-04 NO-GO -> R4 -> 08D

## 1. 路线总览

```text
R4A Contract & Preflight
  R4-01 DTO/Schema/Error/Cache Contract
  R4-02 Fast + Transformed Full Preflight
  R4-03 Mode Admission and Pipeline Gate

R4B Product Interaction & Positive Path
  R4-04 Qt Preflight UI and One-click Gate
  R4-05 Clean OBJ/3MF Width/Material Matrix

R4C Development and Required Asset Admission
  R4-06 Required Family Candidate Intake and Attribute Audit
  R4-07 Development Gate + Final Required-family Global/Release/Legacy Regression
  R4-08 08D GO/NO-GO Refresh

R4C-R1 Restricted Production Candidate Closure
  R4-08-R1 Admission Rule Amendment
  R4-07-R1 Two-family Release/Closure/Performance Validation
  R4-07-R2 Candidate Budget Freeze
  Quick-CI-R1 Golden Baseline Decision
  R4-08-R2 08D GO/NO-GO Refresh
```

## 2. R4A 退出标准

```text
CLI/UI/pipeline 共用同一 preflight facade；
cache stale 规则可验证；
legacy/global admission 同报告可追溯；
global blocker 不启动核心、不写生产包、不 fallback；
未改变 legacy 默认 TIFF。
```

## 3. R4B 退出标准

```text
中文 UI 显示待检测/检测中/通过/警告/阻断/过期；
两个一键入口均经过 preflight；
clean OBJ 和 Texture2D 3MF 完成 minimum/intermediate/allTexture；
white/varnish/RGB/material_role 的解析可解释；
C/M/Y/K 未配置映射时不会产生伪通道。
```

R4B 完成后，可以继续 12E-09A 的宽度和 Model Fill diagnostic UI；仍不能启动 08D production adapter。

## 4. R4C 退出标准

```text
至少两个独立真实模型族各有一个 strict/intake PASS 候选；
身份、尺寸、姿态、UV、材质、纹理 provenance PASS；
完整自相交和 post-strict PASS；
四 case global full chain PASS；
Release budget frozen；
legacy TIFF/RIP/Quick CI 回归闭环；
R4-08 输出 GO 且用户明确授权。
```

受限候选 Gate 采用至少两个独立真实模型族，不允许由单一模型或同一模型族的多个文件冒充多族覆盖。
当前 xiao_ma/yecan 已满足候选身份；aishen/meigui/titian `0/3` 保留为复杂浮雕覆盖缺口。所有输入仍
逐模型执行 strict/fail-closed，任何失败资产都不能继承候选 PASS。

R4-06 的原子级 intake manifest、provenance、属性差异、完整自相交和 post-strict 准入准备见
`../DOC/DOC_PREP_12E_08C_R4_06_RepairedAssetIntake准备.md`。该文档完成不等于外部资产已到位。

R4-07/08 的四 case Release Gate 与 08D GO/NO-GO 刷新已分别完成原子级准备：

```text
../DOC/DOC_PREP_12E_08C_R4_07_FourCaseReleaseGate准备.md
../DOC/DOC_PREP_12E_08C_R4_08_GO_NO_GORefresh准备.md
```

当前 xiao_ma/yecan development intake 为 `2/2 admitted`，R4-07 development four-case 已完成。2026-07-23
准入规则已经修订，R4-07-R1 候选验证可执行；生产预算、Quick CI 和用户对 08D production path 的独立
授权仍阻断 12E-08D。

## 5. 复杂重建后备路线

若 R4C 的外部修复流程无法稳定提供 required 资产，再单独准备 R5：

```text
候选 A：CGAL Polygon Mesh Processing；
候选 B：libigl + 自研局部重建/属性投影；
候选 C：OpenVDB 体素重建 + 最近表面属性转移，仅作有损候选。
```

R5 必须另行完成 CMake/vcpkg、许可证、维护、尺寸误差、UV/材质保持和性能决策，不在 R4 偷跑。

## 6. 与 08D/09/10 的关系

```text
08D：等待 R4-07-R1/R2、Quick-CI-R1 和 R4-08-R2 后重新判断；
09A：R4B 后可继续 diagnostic UI；
09B：等待 08D production admission；
10A：可基于 R4B 准备 preview；
10B/10C production evidence：等待 R4C 与 08D。
```
