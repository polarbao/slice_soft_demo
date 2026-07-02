# DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口

> 文档版本：v0.1  
> 文档状态：DOC_DECISION / Stage 11A-R1  
> 生成日期：2026-07-02  
> 决策主题：OpenVDB 是否可以从实验诊断推进为候选切片引擎

---

## 1. Context

09 / 09P 阶段完成了 OpenVDB 依赖、SDF / surface-shell 原型、真实模型诊断、拓扑准入、实验报告 schema、UI report 展示和 OpenVDB OFF / ON 分层验证。

但当前仍未完成：

```text
OpenVDB production RGBWSV package writer；
OpenVDB candidate package 的 manifest / layers / preview / reports；
rip_reader_test 对 OpenVDB package 的验收；
真实模型 strict_closed 通过或 repair_then_strict；
UI 中明确的 OpenVDB candidate 切片入口。
```

因此，09 阶段测试结果可以支持后续引入 OpenVDB，但不能直接证明 OpenVDB 已可替换当前 legacy 生产切片路径。

---

## 2. Decision

新增 Stage 11A-R1：

```text
OpenVDB Candidate RGBWSV 写包与 Preview 收口
```

阶段目标不是立刻替换 legacy，而是建立第三条显式路径：

```text
Legacy production path
  当前默认生产路径，继续可用。

OpenVDB diagnostic path
  只输出 experimental report，不写 package。

OpenVDB candidate path
  显式启用，strict_closed 通过后写 p0.rgbwsv.2 candidate package。
```

OpenVDB 后续是否取代当前切片状态，必须以后续验收结果判断。短期内只能作为 candidate engine 并行存在。

---

## 3. Alternatives Considered

### 3.1 直接把 OpenVDB 设为默认切片引擎

拒绝。

原因：

```text
OpenVDB 默认 OFF 是项目红线；
真实标准 OBJ 仍有 topology blocker；
candidate package writer 未完成；
会破坏 legacy production 回归稳定性。
```

### 3.2 继续只保留 diagnostic，不做写包

拒绝作为下一步。

原因：

```text
当前用户目标已经从诊断转向可切片；
09P/11A 已经完成足够前置判断；
继续停留 diagnostic 无法回答 OpenVDB 能否替代 legacy 的产品问题。
```

### 3.3 新增显式 OpenVDB candidate path

采纳。

优点：

```text
不破坏 legacy；
不默认启用 OpenVDB；
可以逐步验证 RGBWSV、preview、RIP、texture fidelity；
失败时只写 report，不写半成品 package；
未来可按模型类型逐步替换 legacy。
```

---

## 4. Safety Rules

Stage 11A-R1 必须保持：

```text
不修改 p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8；
不修改 black_is_print；
不默认启用 OpenVDB；
不让 OpenVDB 成为默认构建依赖；
不把 diagnostic_only 输出当 production package；
不绕过 ProductionAdmissionPolicy；
strict_closed 不通过时不得写 candidate package；
legacy path 必须继续通过现有回归。
```

---

## 5. Consequences

### 正向影响

```text
OpenVDB 从“可诊断”推进到“可候选切片”；
UI 可以区分 legacy / diagnostic / candidate；
后续有真实数据判断是否替换 legacy；
候选输出仍复用现有 RGBWSV 协议和 RIP reader。
```

### 风险

```text
真实 OBJ 拓扑可能阻断 strict_closed；
OpenVDB ON 构建和依赖仍有环境差异；
candidate 写包可能与 legacy package summary 不完全一致；
surface shell 到 XY layer 的 raster alignment 需要 golden 验证；
性能可能不足，需要 Release benchmark。
```

---

## 6. Exit Criteria

Stage 11A-R1 完成时必须证明：

```text
OpenVDB OFF 默认轨道仍通过；
OpenVDB ON smoke 通过；
strict_closed PASS fixture 可写 candidate RGBWSV package；
candidate package 可通过 rip_reader_test；
candidate package 可被 LayerPreview / OverlayPreview 读取；
标准真实 OBJ 若仍有 topology blocker，必须保持正确阻断；
legacy 标准 OBJ package 不退化。
```

---

## 7. Follow-up

完成 11A-R1 后再决定是否进入：

```text
11A-R2：真实模型 topology repair / repair_then_strict；
12：产品化 OpenVDB/legacy 双引擎 UI；
13：按模型类型灰度替换 legacy 切片策略。
```

