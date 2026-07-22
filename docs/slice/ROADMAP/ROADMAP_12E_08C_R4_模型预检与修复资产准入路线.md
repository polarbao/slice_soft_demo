# ROADMAP_12E-08C-R4 模型预检与修复资产准入路线

> 文档状态：IN PROGRESS / R4-01..06 IMPLEMENTATION COMPLETE / REAL FAMILY MATRIX 0/3 BLOCKED
> 日期：2026-07-22
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

R4C Required Asset Admission
  R4-06 Required Family Candidate Intake and Attribute Audit
  R4-07 Four-case Global/Release/Legacy Regression
  R4-08 08D GO/NO-GO Refresh
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
爱神、玫瑰、梯田三个 required family 各至少一个 strict PASS 原始/外部修复/独立重建候选；
身份、尺寸、姿态、UV、材质、纹理 provenance PASS；
完整自相交和 post-strict PASS；
四 case global full chain PASS；
Release budget frozen；
legacy TIFF/RIP/Quick CI 回归闭环；
R4-08 输出 GO 且用户明确授权。
```

缺少任一 required family 的 admitted 候选时，R4C 保持 BLOCKED，不用跨族正常 fixture 替代。

R4-06 的原子级 intake manifest、provenance、属性差异、完整自相交和 post-strict 准入准备见
`../DOC/DOC_PREP_12E_08C_R4_06_RepairedAssetIntake准备.md`。该文档完成不等于外部资产已到位。

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
08D：等待 R4-08 GO；
09A：R4B 后可继续 diagnostic UI；
09B：等待 08D production admission；
10A：可基于 R4B 准备 preview；
10B/10C production evidence：等待 R4C 与 08D。
```
