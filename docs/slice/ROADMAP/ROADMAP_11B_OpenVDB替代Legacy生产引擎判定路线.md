# ROADMAP_11B_OpenVDB替代Legacy生产引擎判定路线

> 文档版本：v0.1  
> 文档状态：ROADMAP / OpenVDB Replacement Gate  
> 生成日期：2026-07-04

---

## 1. 当前阶段判断

当前 OpenVDB 已完成：

```text
optional dependency；
experimental diagnostic；
surface-shell prototype；
candidate package writer；
preview / RIP / UI smoke for PASS fixture；
non-production fallback for真实模型。
```

当前 OpenVDB 尚未完成：

```text
真实复杂 OBJ/3MF strict_closed 稳定 PASS；
真实模型 repair_then_strict；
支撑策略与 legacy 等价；
生产 RGBWSV 输出与 legacy 同语义对齐；
Release benchmark 优于或不低于 legacy；
内存预算和连续回归。
```

因此，OpenVDB 当前只能定位为：

```text
Candidate engine
```

不能定位为：

```text
Default production engine
```

---

## 2. Replacement Gate

OpenVDB 取代 legacy 前必须全部满足：

| Gate | 要求 | 当前状态 |
|---|---|---|
| G1 协议 | p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print 不变 | 已满足 |
| G2 显式开关 | OpenVDB 可显式启用，默认 OFF | 已满足 |
| G3 真实模型 | 真实 OBJ/3MF strict_closed 或 repair_then_strict PASS | 未满足 |
| G4 支撑 | 支撑策略与 legacy 等价或差异可验收 | 未满足 |
| G5 纹理 | texture fidelity 达标，UV seam/material seam 不串色 | 部分满足 |
| G6 RIP | rip_reader_test strict PASS | fixture 已满足，真实模型待补 |
| G7 UI | LayerPreview / OverlayPreview PASS | fixture 已满足，真实模型待补 |
| G8 性能 | Release 同姿态 benchmark 不慢于 legacy 或收益明确 | 未满足 |
| G9 内存 | 峰值内存在预算内 | 待补 |
| G10 回归 | OpenVDB ON/OFF 连续回归稳定 | 部分满足 |

---

## 3. 阶段路线

### R-1：11B 小收口

目标：

```text
修复 OpenVDB candidate 姿态配置；
补生产 RGB 预览和像素探针设计；
补配置收敛文档；
补 replacement gate 和 benchmark 方案。
```

退出：

```text
OpenVDB candidate 与 legacy 可同姿态比较；
仍不替代 legacy。
```

### R-2：真实模型 strict / repair 收口

目标：

```text
建立真实 OBJ/3MF 集合；
对 boundary/non-manifold/duplicate/winding/self-intersection 分类；
实现或接入 repair_then_strict；
repair 后重新 strict_closed。
```

退出：

```text
真实模型集合中至少一组可 productionAllowed=true；
失败模型有稳定 blocker code 和建议动作。
```

### R-3：支撑与材料等价

目标：

```text
OpenVDB candidate 支持 legacy full_vertical_projection 等价策略；
W/V/RGB 材料策略与 legacy 可对比；
nonSurfaceRgbPolicy 明确。
```

退出：

```text
同模型同姿态下，主要通道统计差异在阈值内；
支撑区域无明显缺失。
```

### R-4：Release Benchmark

目标：

```text
建立 Release benchmark；
记录 load / orient / levelSet / transfer / support / compose / tiff / preview / report；
拆分 coreComputeMs 与 endToEndMs；
coreComputeMs 不包含 TIFF 保存、preview 图片生成、report/manifest 写入；
记录峰值内存；
同姿态同输出语义比较 legacy 和 OpenVDB。
```

退出：

```text
OpenVDB 在目标模型集合上不慢于 legacy，或质量收益足以接受性能成本；
若慢于 legacy，需要明确优化计划。
```

约束：

```text
不能用 Debug 端到端总耗时直接做替代结论；
不能把 non-production OpenVDB 输出与 legacy production 输出直接比较；
不能把 preview/TIFF I/O 成本混入核心切片耗时后宣称算法变慢或变快。
```

### R-5：灰度替换

目标：

```text
OpenVDB 只对满足 gate 的模型类型启用；
UI 显示引擎选择和 admission；
legacy 仍保留回退。
```

退出：

```text
OpenVDB 可作为生产候选引擎；
仍不删除 legacy。
```

### R-6：默认替代决策

目标：

```text
基于连续回归、真实模型集合、性能、内存、用户验收决定是否默认 OpenVDB。
```

退出：

```text
正式 DOC_DECISION 批准后，才允许改变默认引擎。
```

---

## 4. 当前同姿态 benchmark 判断

当前 Debug 探索：

```text
legacy: 22.653s
OpenVDB candidate: 40.794s
```

当前不能证明 OpenVDB 显著提速。

更重要的是：

```text
OpenVDB candidate 是 non-production；
支撑像素为 0；
strict_closed 被 boundary edges 阻断；
输出层数和网格与 legacy 不等价。
```

因此当前结论：

```text
OpenVDB 不能现在替代 legacy；
可以作为 candidate 继续保留；
是否继续开发取决于 R-2/R-4 的真实模型和 Release benchmark 结果。
```

---

## 5. 推荐项目节奏

短期：

```text
完成 11B 小收口；
暂停 OpenVDB 默认替代讨论；
继续其他主线开发。
```

中期：

```text
在有明确资源时进入 R-2/R-4；
以真实模型集合和 Release benchmark 作为是否继续投入 OpenVDB 的依据。
```

长期：

```text
若 OpenVDB 在质量/性能/鲁棒性上通过 gate，再进入灰度替换；
否则 legacy 继续作为默认生产路径，OpenVDB 保持实验/候选能力。
```
