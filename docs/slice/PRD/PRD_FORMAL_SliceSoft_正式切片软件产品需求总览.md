# PRD_FORMAL_SliceSoft_正式切片软件产品需求总览

> 文档版本：v0.1
> 文档状态：Formal PRD / Current Product Source
> 生成日期：2026-06-30
> 当前阶段：Stage 10 已完成，当前执行 11 UI 切片层预览、交互配置与多模型能力评估
> 适用项目：SliceSoft / UV 彩色多材料 3D 打印切片软件

---

## 1. 产品定位

SliceSoft 的正式产品定位是：

```text
面向 UV / 彩色 / 多材料 3D 打印设备的桌面级切片、诊断和工艺验证软件。
```

它不是单纯的模型查看器，也不是只输出普通层图的传统切片器。它的核心价值是把真实业务模型、纹理、材料、支撑、白墨和光油策略组合成可被 RIP 或设备前处理链路消费的多通道数据包，并提供可解释、可回归、可诊断的工程体系。

---

## 2. 最终目标

最终产品目标：

```text
输入 OBJ / MTL / PNG / 3MF / Texture2D / ColorGroup
→ 完成模型导入、拓扑诊断、几何切片、OpenVDB/SDF 壳层分类
→ 进行 RGB / W / S / V 材料策略组合
→ 输出 RGBWSV TIFF package、manifest、preview、reports
→ 通过 RIP Reader / golden / CI 验证
→ 支持 Qt Debug UI 调参、预览、报告查看
→ 交付稳定切片输出契约给下游 RIP / 设备工艺团队
```

正式项目不是一次性替换 demo，而是保留 legacy 稳定生产路径，同时逐步把 OpenVDB/SDF 能力通过 feature flag、诊断、golden 和 production gate 进入候选生产路径。

---

## 3. 当前产品状态

### 3.1 已完成能力

当前已经具备：

```text
1. Legacy RGBWSV package 输出闭环；
2. slicer_cli / rip_reader_test / slicer_debug_ui；
3. OBJ / MTL / PNG 输入；
4. 3MF stored / deflate；
5. 3MF BaseMaterial / ColorGroup / Texture2DGroup；
6. MaterialRoleMapping；
7. MaterialPolicy；
8. MaterialProcessProfile；
9. 支撑、SupportType、island diagnostics；
10. relief heightfield；
11. preview PNG / report；
12. Qt Debug UI、参数编辑、profile 可视化、UI smoke；
13. R0/R1/R2 的模块边界、配置、报告、测试、CI 基础；
14. OpenVDB / SDF smoke；
15. OpenVDB surface shell texture prototype；
16. 真实 OBJ / 3MF 壳层纹理实验验证；
17. 真实模型拓扑诊断、稳定 issue code、production admission policy；
18. 09P-R1 experimental OpenVDB pipeline 接入边界。
```

### 3.2 当前仍不是 production-safe 的能力

以下能力仍处于 experimental / diagnostic：

```text
1. 真实 OBJ/3MF 直接进入 OpenVDB production RGBWSV 输出；
2. mesh repair / repair_then_strict；
3. surface shell texture 直接写 production TIFF；
4. SDF compensated varnish；
5. SDF support clearance / overhang diagnostics；
6. 设备通信、RIP 半色调、喷头 bitstream、ICC 色彩管理。
```

---

## 4. 核心用户角色

### 4.1 算法开发者

需要：

```text
几何内核
OpenVDB / SDF
拓扑诊断
壳层纹理转移
材料通道组合
report / golden / benchmark
```

### 4.2 工艺工程师

需要：

```text
白墨 W
支撑 S
光油 V
RGB 纹理
profile 参数
通道覆盖率
层分布
失败原因
```

### 4.3 QA / 测试

需要：

```text
quick / full / heavy regression
OpenVDB OFF / ON matrix
negative fixtures
golden summaries
RIP Reader compatibility
稳定 error/warning code
```

### 4.4 UI / 应用层使用者

需要：

```text
选择 config / profile / package
运行 legacy 或 experimental path
查看 preview / overlay / report
查看 topology blocker
查看 productionAdmission
导出诊断结果
```

---

## 5. 产品能力地图

### 5.1 输入能力

| 能力 | 当前状态 | 目标 |
|---|---|---|
| STL | 已有基础 | 保持兼容 |
| OBJ | 已有 | 生产路径稳定化 |
| MTL | 已有 | 多材质策略收口 |
| PNG texture | 已有 | 采样策略与 fallback 固化 |
| 3MF stored / deflate | 已有 | 保持回归 |
| 3MF ColorGroup | 已有 | 与 material role 生产映射结合 |
| 3MF Texture2DGroup | 已有 | 与 surface shell 纹理接入结合 |
| 真实复杂 OBJ/3MF | 可实验诊断 | production admission / repair gate |

### 5.2 几何能力

| 能力 | 当前状态 | 目标 |
|---|---|---|
| legacy layer mask | 已有生产路径 | 保持默认 |
| relief heightfield | 已有 | 保持回归 |
| SupportShapePipeline | 已有 | 后续与 SDF clearance 协同 |
| OpenVDB smoke | 已有 | 保持可选依赖 |
| surface shell / interior | 实验链路已有 | 09P-R2/R3 hardening |
| topology diagnostics | R3 已收口基础 | production gate 扩展 |
| mesh repair | 未实现 | 单独阶段评估 |

### 5.3 材料能力

| 能力 | 当前状态 | 目标 |
|---|---|---|
| RGB | 已有 | surface shell RGB 生产候选 |
| White W | 已有 | underbase / profile 化 |
| Support S | 已有 | clearance / overhang 后续增强 |
| Varnish V | 已有 top layer | 09C SDF compensated varnish |
| MaterialPolicy | 已有 | 与 MaterialChannelComposer 收敛 |
| MaterialProcessProfile | 已有 | UI / CI / report 强化 |

### 5.4 输出能力

| 能力 | 当前状态 | 目标 |
|---|---|---|
| RGBWSV TIFF | 已有 | 协议冻结 |
| manifest | 已有 | schema 持续兼容 |
| RIP Reader | 已有 | 09P-R2 加入 experimental compatibility 判断 |
| preview | 已有 | OpenVDB report/preview 接入 UI |
| report | 多阶段已有 | 09P-R2 固化 experimental report schema |
| golden | 有摘要基础 | OpenVDB RGBWSV / report golden 扩展 |

---

## 6. 不可变生产协议

除非单独进入协议升级阶段，否则以下规则不可修改：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
OpenVDB 默认关闭
legacy slicer_cli production path 保持可用
warn_and_attempt 不得 production-safe
```

---

## 7. Demo 到正式产品的变化

正式化不是“把 demo 改名为产品”，而是发生以下变化：

| 维度 | Demo 阶段 | 正式项目阶段 |
|---|---|---|
| 入口 | 单个 CLI / 临时脚本 | CLI + UI + pipeline + CI |
| 配置 | 样例驱动 | schema / migration / profile |
| 几何 | 简化 raster / 实验 OpenVDB | legacy + OpenVDB feature flag |
| 纹理 | 验证采样能跑 | 可解释 transfer / fallback / seam |
| 材料 | RGBWSV 生成 | MaterialChannelComposer / role policy |
| 错误 | 字符串和日志 | 稳定 issue code / admission |
| 输出 | 能生成 package | golden / RIP / report / UI 可验证 |
| 测试 | 手动脚本 | unit / schema / golden / smoke / matrix |
| 文档 | 聊天和阶段堆叠 | PRD / DEV / TASKS / REPORT 真源体系 |
| 生产准入 | 没有 | strict_closed / repair_then_strict / gate |

---

## 8. 09P-R2 产品目标

09P-R2 的产品目标不是新增大功能，而是把 09P-R1 的实验接入边界硬化为可判断、可回归、可 UI 展示的候选路径。

09P-R2 必须完成：

```text
1. experimental report schema 文档化；
2. productionAdmission 字段稳定；
3. topology blocker 和 repair 前置判断清楚；
4. OpenVDB OFF / ON 测试矩阵清楚；
5. Qt Debug UI 能读取 experimental report；
6. golden / downstream output contract / texture fidelity compatibility 的边界明确；
7. 下一阶段是否进入 mesh repair 或 09P-R3 有明确判断。
```

09P-R2 不应做：

```text
1. 默认启用 OpenVDB；
2. 真实 OBJ/3MF 直接写 production RGBWSV；
3. 自动 mesh repair 大实现；
4. 修改 p0.rgbwsv.2；
5. RIP 半色调、设备通信、喷头 bitstream 与设备工艺联调；
6. compensated varnish / support clearance。
```

---

## 9. 后续产品路线

推荐路线：

```text
09P-R2：experimental path hardening
→ 09P-R3：Qt UI / profile / report / CI 工程化
→ 09P-R4：production gate / release candidate
→ mesh repair / admission gate 专项阶段，可按风险提前插入
→ 09C：SDF compensated varnish prototype
→ 09D：SDF support clearance / overhang diagnostics
→ 10：切片输出交付契约 / 纹理保真验收
→ 11：UI 切片层预览 / 交互配置 / 多模型能力评估
```

---

## 10. 验收总原则

任何进入正式项目主线的能力必须满足：

```text
1. 默认关闭高风险实验路径；
2. 有 config schema；
3. 有 report schema；
4. 有 stable issue code；
5. 有 unit 或脚本验证；
6. 有 golden 或摘要回归；
7. 有明确 failure/fallback policy；
8. 不破坏 legacy production path；
9. 不隐式修改 RGBWSV 协议；
10. 有 REPORT 文档记录验证命令与限制。
```

---

## 11. 结论

SliceSoft 当前已经越过“能不能切”的 demo 阶段，进入“如何安全、可解释、可回归地把 OpenVDB/SDF 壳层纹理能力推向生产候选”的阶段。

当前最重要的产品判断是：

```text
09P-R1 已完成 experimental boundary。
09P-R2 应先 hardening，不应急于 production 输出。
真实 OBJ/3MF 的 production-safe 资格仍依赖 admission gate 和可能的 mesh repair 策略。
```
