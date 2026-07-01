# PRD_09P_R2_OpenVDB实验生产管线Hardening

> 文档版本：v0.1
> 文档状态：Formal PRD / Stage 09P-R2
> 生成日期：2026-07-01
> 阶段定位：OpenVDB experimental production-pipeline hardening

---

## 1. 阶段判断

09P-R2 不是继续扩展新功能的阶段，而是把 09P-R1 已接入的 experimental OpenVDB surface-shell pipeline 做成可解释、可回归、可准入判断、可 UI 展示的 hardening 层。

当前 09P-R2 已有任务清单，但缺正式 PRD / DEV / DEMO / CODEX_PROMPT 文档包。本文件补齐产品需求侧定义。

---

## 2. 产品目标

09P-R2 需要回答：

```text
1. experimental report schema 是否稳定；
2. productionAdmission 是否能清楚解释能否进入生产候选；
3. topology blocker 是否形成稳定 gate；
4. OpenVDB / texture / material composer 之间的数据契约是否明确；
5. experimental 输出是否能做 golden / 下游输出契约 / 纹理保真兼容判断；
6. Qt Debug UI 是否可以读取 experimental report；
7. OpenVDB OFF / ON 的 CI 分层是否清楚；
8. 是否可以进入 09P-R3，或必须先插入 mesh repair / admission gate 专项。
```

---

## 3. 用户与使用场景

| 角色 | 关注点 |
|---|---|
| 切片算法工程师 | OpenVDB surface-shell 路线是否可解释、可复测 |
| 材料/纹理工程师 | texture transfer、白墨、光油、支撑通道是否保留足够信息 |
| UI/调试人员 | report、admission、blocker、warning 是否可展示 |
| QA/CI 维护者 | OFF / ON 环境下是否有稳定验证入口 |
| 下游 RIP 工程师 | 只关心上游输出契约和纹理信息是否充分，不要求本项目实现 RIP |

---

## 4. 功能范围

### 4.1 Report Schema

必须稳定：

```text
schema；
input model metadata；
openvdb status；
surface shell status；
productionAdmission；
diagnostics / ValidationIssue；
legacyPathExecuted；
productionPackageWritten；
writeProductionRgbwsv；
timing / memory / stats。
```

### 4.2 Production Admission

必须明确：

```text
strict_closed；
warn_and_attempt；
diagnostic_only；
repair_then_strict placeholder。
```

`warn_and_attempt` 永远不能 productionAllowed。

### 4.3 Topology Gate

必须覆盖：

```text
MESH_BOUNDARY_EDGES；
MESH_SELF_INTERSECTION_CONFIRMED；
MESH_NON_MANIFOLD_EDGES；
MESH_DUPLICATE_FACES；
MESH_OPPOSITE_DUPLICATE_FACES；
MESH_LOCAL_WINDING_INCONSISTENCY；
OPENVDB_UNAVAILABLE；
OPENVDB_LEVEL_SET_FAILED。
```

### 4.4 Downstream Output Contract

09P-R2 不实现 RIP。这里只定义下游消费所需的切片输出信息：

```text
RGBWSV package summary；
manifest；
per-layer stats；
texture fallback / fidelity fields；
diagnostic golden；
optional in-memory RGBWSV summary golden。
```

---

## 5. 非目标

```text
不默认启用 OpenVDB；
不让 OpenVDB 成为强制依赖；
不替代 legacy slicer_cli production path；
不从 experimental path 写真实 OBJ/3MF production RGBWSV TIFF；
不修改 p0.rgbwsv.2；
不实现自动 mesh repair 大功能；
不实现 RIP 半色调、设备通信或喷头 bitstream；
不实现 11 阶段 UI layer slider。
```

---

## 6. 阶段验收

09P-R2 完成时必须输出：

```text
REPORT_09P_R2；
稳定 experimental report schema；
admission gate matrix；
mesh repair 前置判断；
service data contract；
experimental golden / downstream output contract / texture fidelity 方案；
Qt UI report integration 最小能力；
OpenVDB OFF / ON CI matrix。
```
