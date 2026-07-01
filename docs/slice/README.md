# SliceSoft 正式文档入口

> 文档状态：Formal Docs Entry
> 更新日期：2026-07-01
> 适用阶段：09P-R2 已完成，当前执行 10 阶段切片输出交付契约与纹理保真验收

本目录是 SliceSoft 从 demo 切片软件转向正式项目后的正式文档入口。文档按类型分层，避免 PRD、DEV、验证方案、路线图和决策记录混在同一目录中。

## 目录结构

| 目录 | 内容 | 使用场景 |
|---|---|---|
| `PRD/` | 产品需求、阶段需求、当前 demo 功能基线 | 判断为什么做、用户价值、验收口径 |
| `DEV/` | 技术方案、模块边界、执行设计 | 判断怎么做、改哪些模块、如何验证 |
| `DOC/` | 文档治理、决策记录、审计清单 | 判断文档真源、历史归档、边界决策 |
| `DEMO/` | 演示和验证方案 | 判断阶段完成后如何演示、如何验收 |
| `ROADMAP/` | 项目路线图、短中长期计划 | 判断阶段排序、优先级、资源节奏 |
| `REPORT/` | 阶段完成报告 | 阶段结束后记录已做事项和实际验证结果 |

## 当前必读入口

| 文件 | 用途 |
|---|---|
| `DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md` | 文档真源、证据等级、归档策略 |
| `DOC/DOC_CLASSIFICATION_2026-06-30_docs治理归档清单.md` | 本轮 docs 分类与归档清单 |
| `PRD/PRD_FORMAL_SliceSoft_正式切片软件产品需求总览.md` | 正式产品级 PRD 总控 |
| `DEV/DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md` | 正式技术方案总控 |
| `PRD/PRD_DEMO_IMPLEMENTED_SliceSoft_当前Demo功能基线.md` | 当前 demo 已实现功能基线 |
| `DEV/DEV_DEMO_IMPLEMENTED_SliceSoft_当前Demo技术基线.md` | 当前 demo 已实现技术基线 |
| `ROADMAP/ROADMAP_FORMAL_SliceSoft_Demo到正式项目演进路线.md` | demo 到正式项目演进路线 |
| `ROADMAP/ROADMAP_SHORT_MID_LONG_SliceSoft_项目运行计划.md` | 短期 / 中期 / 长期项目运行计划 |
| `PRD/PRD_SHORT_MID_LONG_SliceSoft_项目运行计划需求.md` | 项目运行计划对应的产品需求 |
| `DEV/DEV_SHORT_MID_LONG_SliceSoft_项目运行计划执行方案.md` | 项目运行计划对应的技术执行方案 |
| `PRD/PRD_10_切片输出交付契约与纹理保真验收.md` | 当前 10 阶段产品需求 |
| `DEV/DEV_10_OutputContract_TextureFidelity设计.md` | 当前 10 阶段技术方案 |
| `DEV/DEV_10_OutputContract_FieldMatrix.md` | 当前 10 阶段输出契约字段矩阵 |
| `DEV/DEV_10_LayerChannelSummaryContract.md` | 当前 10 阶段 layer/channel summary 统计契约 |
| `DEV/DEV_10_TextureFidelityMetrics.md` | 当前 10 阶段纹理保真指标定义 |
| `DEMO/DEMO_10_切片输出契约与纹理保真验证方案.md` | 当前 10 阶段验证方案 |
| `DOC/DOC_DECISION_10_RIP边界与切片输出契约.md` | 当前 10 阶段 RIP 边界决策 |

## 阶段入口

| 文件 | 用途 |
|---|---|
| `DOC/DOC_AUDIT_00_08_历史阶段文档缺口与补齐清单.md` | 00-08 历史阶段文档缺口审计与补齐说明 |
| `PRD/PRD_00_08_Demo阶段功能基线汇总.md` | 00-08 功能基线汇总 |
| `DEV/DEV_00_08_Demo阶段技术基线汇总.md` | 00-08 技术基线汇总 |
| `DEMO/DEMO_00_08_Demo阶段验证与回归基线.md` | 00-08 验证与回归基线 |
| `PRD/PRD_09P_R2_OpenVDB实验生产管线Hardening.md` | 09P-R2 产品需求：experimental OpenVDB hardening |
| `DEV/DEV_09P_R2_ReportSchema_AdmissionGate_CI_UI设计.md` | 09P-R2 技术方案：report schema、admission gate、CI、UI |
| `DEV/DEV_09P_R2_ServiceDataContract.md` | 09P-R2 OpenVDB / texture / composer / admission / report 服务数据契约 |
| `DEMO/DEMO_09P_R2_OpenVDB实验生产管线Hardening验证方案.md` | 09P-R2 验证方案 |
| `DEMO/DEMO_09P_R2_CI_Matrix验证方案.md` | 09P-R2 OpenVDB OFF / ON / Benchmark 分层 CI matrix |
| `DEMO/DEMO_09P_R2_experimental_golden_rip_compatibility.md` | 09P-R2 experimental golden / downstream output contract / texture fidelity compatibility |
| `DOC/DOC_SCHEMA_09P_R2_experimental_openvdb_shell_report.md` | 09P-R2 experimental OpenVDB CLI report schema 契约 |
| `DOC/DOC_MATRIX_09P_R2_topology_admission_gate.md` | 09P-R2 topology admission gate 矩阵 |
| `DOC/DOC_DECISION_09P_R2_mesh_repair_admission_gate.md` | 09P-R2 mesh repair 前置判断与 `repair_then_strict` 决策 |
| `REPORT/REPORT_09P_R2_OpenVDB实验生产管线Hardening当前状态.md` | 09P-R2 当前实现状态与验证报告 |
| `PRD/PRD_10_切片输出交付契约与纹理保真验收.md` | 10 阶段产品需求：切片输出契约、纹理保真、下游交付 |
| `DEV/DEV_10_OutputContract_TextureFidelity设计.md` | 10 阶段技术方案：output contract、layer summary、texture fidelity |
| `DEMO/DEMO_10_切片输出契约与纹理保真验证方案.md` | 10 阶段验证方案 |
| `DOC/DOC_DECISION_10_RIP边界与切片输出契约.md` | 10 阶段边界决策：不实现 RIP，只定义切片输出契约 |
| `PRD/PRD_11_UI切片层预览交互配置与多模型能力.md` | 11 阶段产品需求：层预览、伪彩、配置交互、多模型评估 |
| `DEV/DEV_11_LayerPreview_UIConfig_MultiModel设计.md` | 11 阶段技术方案：LayerPreview contract、UI 模块、配置面板、多模型数据模型 |
| `DEMO/DEMO_11_UI切片层预览交互配置验证方案.md` | 11 阶段验证方案：layer slider、伪彩、配置面板、multi-model decision fixture |
| `DOC/DOC_DECISION_11_多模型切片处理范围决策.md` | 多模型处理范围决策：只新增 11 一个阶段，多模型先评估不默认 production |

## 目录边界

```text
docs/slice
  正式 PRD / DEV / ROADMAP / DEMO / REPORT / DOC_DECISION，按类型分目录保存

docs/codex_task
  Codex 执行任务、提示词、任务归档

docs/archive/2026-06-30_slicer_legacy
  旧 docs/slicer 阶段文档归档，只作历史证据和背景

docs/user_guides
  面向使用者或调试者的操作说明
```

## 真源规则

当文档之间冲突时，按以下顺序判断：

```text
当前代码 / CMake / 脚本 / 测试
> 最新阶段 REPORT
> docs/slice 下的正式 PRD / DEV / ROADMAP
> docs/codex_task/current 下的当前任务
> archive 下的历史阶段文档
> PDF / 网页聊天导出
```

归档文档不删除，但不再作为当前实现或当前计划的唯一依据。
