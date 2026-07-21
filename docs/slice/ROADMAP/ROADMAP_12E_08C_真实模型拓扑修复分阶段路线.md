# ROADMAP_12E-08C 真实模型拓扑修复分阶段路线

> 文档状态：COMPLETE / NON-PRODUCTION / R3-04 NO-GO
> 日期：2026-07-21
> 阶段位置：12E-08C Release Evidence 与 12E-08D Production Admission 之间

## 1. 总体路线

```text
12E-08C-R1 Contract & Eligibility
  -> 12E-08C-R2 Conservative Repair
  -> 12E-08C-R3 Real Model & Release Gate
  -> 12E-08D Production Admission
```

任何 R 阶段完成都不自动启动下一阶段。12E-08D 必须再次核对 Gate。

## 2. R1：契约与资格

目标：不修改网格，建立可稳定执行的修复前证据。

原子任务：

```text
R1-01：MeshRepair DTO、稳定错误码、报告 Schema 和 deterministic hash；
R1-02：Eligibility Policy 与 issue 分类；
R1-03：generated topology/attribute fixtures 和 golden；
R1-04：三个真实 OBJ pre-repair baseline 与人工建议。
```

退出标准：所有输入均有稳定 status/code/hash；`repairAttempted=false`；不写 package。

## 3. R2：保守修复

目标：只实现有唯一、可解释结果的局部操作。

原子任务：

```text
R2-01：degenerate/exact duplicate cleanup 与 source mapping；
R2-02：受约束 vertex weld、local winding 和组件守门；
R2-03：简单 boundary loop stitch/hole-fill 与 attribute policy；
R2-04：post-repair strict、attribute validator、negative/golden tests。
```

当前进度：R2-01..04 已完成。R2-03 证明简单、平面、严格凸且属性唯一的闭环可以安全填补；R2-04 用
独立 validator 复核 mapping、属性、完整 post-strict 和 hash，失败候选会被丢弃。required OBJ 因 sampled
self-intersection、non-manifold 或无 boundary 仍保持 manual，没有制造伪 PASS。

退出标准：generated repair fixtures strict PASS；冲突 fixture 稳定 blocked；修复默认关闭。

## 4. R3：真实模型与 Release Gate

目标：判断现有真实模型能否通过保守修复进入 12E core，并冻结真实证据。

原子任务：

```text
R3-01：non-manifold pattern classifier 与条件 fan split feasibility；
R3-01A：required real model 完整自相交证据与 deterministic pair hash；
R3-02：nai_you/aishen/meigui/3MF 真实模型 repair matrix；
R3-03：post-repair 12E core、Release time/memory、legacy regression；
R3-04：更新 admission matrix，给出 08D GO/NO-GO。
```

当前进度：R3-01 已完成。`aishen_fudiao` 的 59 条 non-manifold edge 由 2 条 duplicate exporter 和 57 条
attribute-conflicting fan 构成；`meigui_fudiao` 的 10940 条由 10935 条 duplicate exporter 和 5 条
attribute-conflicting fan 构成。两者均不存在全局唯一 fan split，保持 manual。R3-01A 已完成确定性完整
自相交证据：三个 required OBJ 均为 confirmed intersection，闭合 3MF 为 complete no intersection，四 case
双运行稳定且无 budget blocked。R3-02 Repair Matrix 已完成：三个 OBJ 在 mutation 前 fail-fast，闭合 3MF
保持 no-op strict PASS，任务证据 4/4 完整但 production Gate 0/4 通过。R3-03 已完成非生产 Release/legacy
回归，R3-04 已输出 NO-GO，12E-08D 继续 BLOCKED。

退出标准：每个 required case 有真实状态；预算可冻结或明确保持 BLOCKED；不得用 manual required 冒充 PASS。

## 5. 12E-08D Gate

只有 R3-04 输出 GO，且以下证据都通过，才允许开始 08D：

```text
required real model strict PASS；
attribute preservation PASS；
partition/texture/raster/full closure PASS；
Release budget PASS；
legacy/RIP/protocol gate PASS；
用户确认 production path。
```

## 6. Parallel Work

`12E-09A` diagnostic UI 可并行展示 blocked/repair status，但不得启用 production Profile。
`12E-10` 可准备 preview/real-model/report 收口文档，但其 production 部分依赖 08D。

## 7. Rollback

每个阶段保持 repair 默认关闭。删除 repair candidate 或返回 strict-only 不应改变 legacy Profile、生产 TIFF 或
默认 OpenVDB OFF 构建。
