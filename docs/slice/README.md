# SliceSoft 正式文档入口

> 文档状态：Formal Docs Entry
> 更新日期：2026-07-20
> 适用阶段：Stage 12D COMPLETE；Stage 12E-08C R1/R2/R3-01 COMPLETE、R3 IN PROGRESS / 双切片模式目标已固化

本目录是 SliceSoft 从 demo 切片软件转向正式项目后的正式文档入口。文档按类型分层，避免 PRD、DEV、验证方案、路线图和决策记录混在同一目录中。

第一次接触项目、希望按学习顺序理解行业名词、切片原理、架构、配置、输出、构建、测试和后续路线时，请从 [SliceSoft 从零到参与开发教程](../tutorials/README.md) 开始；本目录继续作为正式需求、设计、决策和阶段状态真源。

当前状态：12C、12D 已收口。12E-01 至 12E-07 以及 12E-08A/08B/08C 已完成配置、service、CPU/OpenVDB conformance、Width Sweep、纹理传递、内存 Diagnostic Composer、成功报告、12D 模型域与完整材料域 exact closure、classification-to-raster 和默认 OFF Release 证据。三个真实 OBJ 被 strict topology 阻断，Release 预算未冻结；12E-08C-R1/R2 与 R3-01 已完成，下一原子任务为 R3-01A 完整自相交证据。产品目标已补充为 `legacy | global_surface_shell` 双模式和统一 RGBWSV TIFF 输出，但 Router、global production adapter 与 UI 选择器尚未实现；12E-08D 继续阻断，12E-09A diagnostic UI 可并行启动。

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
| `PRD/PRD_RESEARCH_PrismSlicer_UI切片与策略功能逆向整理.md` | 基于公开信息重建的 PrismSlicer UI、切片功能与切片策略 PRD，不包含 RIP/半色调实现 |
| `DEV/DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md` | 正式技术方案总控 |
| `PRD/PRD_DEMO_IMPLEMENTED_SliceSoft_当前Demo功能基线.md` | 当前 demo 已实现功能基线 |
| `DEV/DEV_DEMO_IMPLEMENTED_SliceSoft_当前Demo技术基线.md` | 当前 demo 已实现技术基线 |
| `ROADMAP/ROADMAP_FORMAL_SliceSoft_Demo到正式项目演进路线.md` | demo 到正式项目演进路线 |
| `ROADMAP/ROADMAP_SHORT_MID_LONG_SliceSoft_项目运行计划.md` | 短期 / 中期 / 长期项目运行计划 |
| `PRD/PRD_SHORT_MID_LONG_SliceSoft_项目运行计划需求.md` | 项目运行计划对应的产品需求 |
| `DEV/DEV_SHORT_MID_LONG_SliceSoft_项目运行计划执行方案.md` | 项目运行计划对应的技术执行方案 |
| `REPORT/REPORT_12B_R2_OpenVDB_SDFUtility当前状态.md` | 12B-R2 与 12B 收口历史报告 |
| `REPORT/REPORT_12C_Qt工作台启动状态.md` | 当前阶段启动状态，12C-R0 可开始 |
| `REPORT/REPORT_11A_OpenVDB_OBJ彩色纹理切片前置当前状态.md` | OpenVDB OBJ 彩色纹理前置状态报告 |
| `REPORT/REPORT_11A_R1_OpenVDB候选切片写包当前状态.md` | OpenVDB Candidate 写包当前状态报告 |
| `REPORT/REPORT_11B_文档规划与OpenVDB替代评估当前状态.md` | 11B 文档规划与 OpenVDB 替代评估当前状态 |
| `REPORT/REPORT_11B_UI配置生产预览与OpenVDB姿态收口当前状态.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口当前状态 |
| `REPORT/REPORT_12_专项规划当前状态.md` | 12 阶段专项规划当前状态 |
| `DOC/DOC_DECISION_12_11B后进入切片语义引擎性能与UI产品化专项.md` | 12 阶段入口决策：切片语义、引擎性能、UI 产品化 |
| `DOC/DOC_AUDIT_12_当前切片策略与需求偏差审查.md` | 当前切片策略与需求偏差审查 |
| `ROADMAP/ROADMAP_12_切片语义引擎性能UI专项路线.md` | 12A/12B/12C 专项路线 |
| `PRD/PRD_12A_彩色纹理材料填充支撑光油策略.md` | 12A 彩色纹理材料填充、支撑、光油策略需求 |
| `DEV/DEV_12A_彩色纹理材料填充支撑光油策略设计.md` | 12A 彩色纹理材料填充、支撑、光油策略设计 |
| `DOC/DIAGRAM_12A_内部镂空支撑与外侧光油支撑关系.svg` | 12A 内部镂空支撑、外侧光油和上表面支撑关系示意图 |
| `DOC/DIAGRAM_12A_指甲模型横截面材料示意图.png` | 12A 真实 RIP 横截面材料栈参考图 |
| `DOC/DOC_REVIEW_12A_真实RIP横截面示意图对齐审查.md` | 12A 真实 RIP 横截面示意图对齐审查 |
| `PRD/PRD_12B_切片引擎性能与OpenVDB替代评估.md` | 12B 切片引擎性能与 OpenVDB 替代评估需求 |
| `DEV/DEV_12B_切片引擎性能与OpenVDB替代评估设计.md` | 12B 切片引擎性能与 OpenVDB 替代评估设计 |
| `DEV/DEV_12B_R1_LegacyHeightfield优化原型设计.md` | 12B-R1 legacy 与 heightfield 优化原型设计 |
| `PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md` | 12B-R2 OpenVDB SDF utility 定位需求 |
| `DEV/DEV_12B_R2_OpenVDB_SDFUtility评估设计.md` | 12B-R2 OpenVDB SDF utility 评估设计 |
| `DEMO/DEMO_12B_R2_OpenVDB_SDFUtility验证方案.md` | 12B-R2 OpenVDB SDF utility 验证方案 |
| `DOC/DOC_AUDIT_12B_R2_OpenVDB_SDFUtility代码盘点.md` | 12B-R2 OpenVDB SDF utility 当前代码盘点 |
| `DOC/DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md` | 12B-R2 OpenVDB SDF utility report schema |
| `DOC/DOC_MATRIX_12B_R2_OpenVDBSdfUtilityCapability.md` | 12B-R2 OpenVDB SDF utility 能力矩阵 |
| `DOC/DOC_AUDIT_12B_任务覆盖与R2缺口审查.md` | 12B 主任务覆盖与 R2 收口审查 |
| `DOC/DOC_ANALYSIS_12B_R1_2_5DHeightfieldFastPath可行性评估.md` | 12B-R1 2.5D heightfield fast path 可行性评估 |
| `DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md` | 12B R0/R1/R2 阶段拆分决策 |
| `DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md` | 12B core benchmark report schema |
| `ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md` | 12B 切片引擎性能分阶段路线 |
| `REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md` | 12B-R0 benchmark 契约与真实 Release 对比当前状态 |
| `REPORT/REPORT_12B_R1_LegacyHeightfield优化当前状态.md` | 12B-R1 legacy 与 heightfield 优化当前状态 |
| `REPORT/REPORT_12B_R2_OpenVDB_SDFUtility启动状态.md` | 12B-R2 OpenVDB SDF utility 启动状态 |
| `REPORT/REPORT_12B_R2_OpenVDB_SDFUtility当前状态.md` | 12B-R2 最终状态、能力矩阵、OFF/ON report 和 12C 移交结论 |
| `PRD/PRD_12C_Qt_UI配置预览工作台收口.md` | 12C Qt UI 配置、预览工作台收口需求 |
| `DEV/DEV_12C_Qt_UI配置预览工作台设计.md` | 12C Qt UI 配置、预览工作台设计 |
| `DEMO/DEMO_12C_Qt_UI配置预览验证方案.md` | 12C fresh build、effective config、统一预览和布局验证方案 |
| `DOC/DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md` | 12C 当前 UI 能力、增量范围和 build blocker 审查 |
| `DOC/DOC_DECISION_12C_R0_R1_R2_Qt工作台阶段拆分.md` | 12C R0/R1/R2 阶段拆分与准入边界 |
| `DOC/DOC_DECISION_12C_R0_01_QtMSVCFreshBuildLane.md` | 12C Qt/MSVC fresh build lane 决策与验证 |
| `DOC/DOC_DECISION_12C_UI产品默认值与交互冻结.md` | 12C Profile、effective config、诊断布局和 12D 接入默认值 |
| `DOC/DOC_CHECKLIST_12C_阶段准入与上下文完整性.md` | 12C 文档、上下文和原子任务准入检查 |
| `ROADMAP/ROADMAP_12C_Qt工作台分阶段执行路线.md` | 12C 构建、设置、预览和诊断工作台路线 |
| `REPORT/REPORT_12C_Qt工作台启动状态.md` | 12C 准入状态、可复用能力和下一任务 |
| `DOC/DOC_DECISION_12D_横截面材料无缝闭环专项.md` | 12D 横截面材料无缝闭环专项决策 |
| `PRD/PRD_12D_横截面材料无缝闭环验收与修复.md` | 12D 横截面材料无缝闭环产品需求 |
| `DEV/DEV_12D_材料闭环诊断与修复设计.md` | 12D 材料闭环诊断与修复技术设计 |
| `DOC/DOC_PREP_12D_R2_SemanticMask精确诊断接入准备.md` | 12D-R2 semantic mask ownership、pipeline 插入点和 exact 验收准备 |
| `DOC/DOC_PREP_12D_R2_RepairDisabled不变性验证准备.md` | 12D-06 双配置、TIFF SHA-256 和 gap 保留守门准备 |
| `DOC/DOC_PREP_12D_R3_一像素修复背景保护UI真实模型准备.md` | 12D-07 至 12D-10 repair、背景保护、UI 和真实模型准备 |
| `REPORT/REPORT_12D_材料闭环准备状态.md` | 12D 完成状态、真实模型 hash、RIP 与 timing 证据 |
| `DOC/DOC_DECISION_12E_全局纹理表面层与模型填充互补策略.md` | 12E 全局纹理表面层与模型填充互补分区决策 |
| `PRD/PRD_12E_全局纹理表面层与模型填充连续调节.md` | 12E 纹理宽度、动态最大值和全纹理产品需求 |
| `DEV/DEV_12E_全局纹理壳层与模型填充分区设计.md` | 12E 完整三维距离、互补 mask、UI 与 report 技术设计 |
| `DEMO/DEMO_12E_全局纹理壳层与模型填充验证方案.md` | 12E 单调性、全纹理、薄壁、内腔和 UI 验证方案 |
| `ROADMAP/ROADMAP_12E_全局纹理壳层与模型填充分阶段路线.md` | 12E R0-R4 分阶段执行路线 |
| `DOC/DOC_PREP_12E_R0_ConfigDTO契约准备.md` | 12E-01 Config、DTO、错误码和安全阻断契约 |
| `DOC/DOC_PREP_12E_R1_GlobalPartitionService骨架准备.md` | 12E-02 Service、3D mask DTO、不变量和测试准备 |
| `DOC/DOC_PREP_12E_R1_LegacyCpuGlobalDistanceCandidate准备.md` | 12E-03 CPU occupancy、最近面距离、拓扑和性能准备 |
| `DOC/DOC_PREP_12E_R1_OpenVdbConformanceAdapter准备.md` | 12E-04 OpenVDB OFF/ON adapter、同 grid 对照和安全边界准备 |
| `DOC/DOC_PREP_12E_R2_WidthSweep与ReportSchema准备.md` | 12E-05 宽度扫描、单调性、成功报告和 golden 准备 |
| `DOC/DOC_PREP_12E_R3_TextureTransfer与DiagnosticComposer准备.md` | 12E-06 纹理传递、fallback、确定性 tie 和诊断合成准备 |
| `DOC/DOC_PREP_12E_R3_12DClosure联动准备.md` | 12E-07 exact mask、allTexture 和 12D closure 联动准备 |
| `DOC/DOC_PREP_12E_R4_ProductionAdmission准备.md` | 12E-08 raster mapping、完整 closure、性能和生产准入准备 |
| `DOC/DOC_DECISION_12E_Legacy与GlobalSurfaceShell双切片模式.md` | legacy/global_surface_shell 双模式、默认值、准入和禁止静默回退决策 |
| `DOC/DOC_SCHEMA_12E_DualSlicePipelineConfig.md` | `slicePipeline.mode` 配置、校验、错误码和统一 TIFF 输出契约 |
| `DOC/DOC_PREP_12E_08D_双模式生产写包准备.md` | 12E-08D Router、global adapter、共享 writer 和验证原子任务准备 |
| `DOC/DOC_DECISION_12E_08C_R1_R2_R3_真实模型拓扑修复前置专项.md` | 在 12E-08D 前插入显式 repair-then-strict 专项的正式决策 |
| `PRD/PRD_12E_08C_真实模型拓扑修复与严格准入.md` | 真实模型自动/人工修复、属性保持和严格准入需求 |
| `DEV/DEV_12E_08C_MeshRepairThenStrict设计.md` | Mesh Repair 服务、哈希、属性映射和 post-strict 技术设计 |
| `DEMO/DEMO_12E_08C_真实模型拓扑修复验证方案.md` | generated/真实模型/属性/Release 修复验证方案 |
| `ROADMAP/ROADMAP_12E_08C_真实模型拓扑修复分阶段路线.md` | 12E-08C-R1/R2/R3 分阶段路线 |
| `DOC/DOC_SCHEMA_12E_MeshRepairReport.md` | `slicesoft.mesh_repair.12e_08c.1` 报告契约 |
| `DOC/DOC_MATRIX_12E_真实模型拓扑修复与严格准入.md` | issue、fixture、真实模型、属性和 08D Gate 矩阵 |
| `DOC/DOC_PREP_12E_08C_R1_拓扑分类与修复契约准备.md` | R1-01 DTO/hash/report 实施准备 |
| `DOC/DOC_EXEC_12E_08C_R1_01_MeshRepairContract结果.md` | R1-01 DTO、稳定错误码、canonical SHA-256、report skeleton 和验证结果 |
| `DOC/DOC_EXEC_12E_08C_R1_02_EligibilityPolicy结果.md` | R1-02 topology/robustness 资格分类、优先级和验证结果 |
| `DOC/DOC_EXEC_12E_08C_R1_03_GeneratedFixtureGolden结果.md` | R1-03 生成夹具、确定性 hash 和 report projection golden 结果 |
| `DOC/DOC_EXEC_12E_08C_R1_04_真实模型PreRepairBaseline结果.md` | R1-04 三个真实 OBJ 与闭合 3MF 的可重复只读 baseline |
| `DOC/DOC_PREP_12E_08C_R1_EligibilityFixtureBaseline准备.md` | R1-02..04 资格、生成夹具和真实模型 baseline 准备 |
| `DOC/DOC_PREP_12E_08C_R2_ConservativeRepair准备.md` | R2 保守修复操作、属性、post-strict 和停止条件准备 |
| `DOC/DOC_PREP_12E_08C_R3_RealModelReleaseGate准备.md` | R3 真实模型、Release 预算和 08D GO/NO-GO 准备 |
| `DOC/DOC_PREP_12E_08C_R3_01A_完整自相交证据准备.md` | R3-01A 确定性完整自相交 broad-phase 与 strict 证据准备 |
| `REPORT/REPORT_12E_08C_真实模型拓扑修复专项启动状态.md` | 修复专项启动状态和下一任务 |
| `DOC/DOC_EXEC_12E_R4A_ClassificationRaster映射结果.md` | 12E-08A world-space classification-to-raster 实现与验证结果 |
| `DOC/DOC_EXEC_12E_R4B_完整材料语义闭环结果.md` | 12E-08B 支撑、内部空洞与光油 full-material closure 实现及验证结果 |
| `DOC/DOC_PREP_12E_R5_QtUI与EffectiveConfig准备.md` | 12E-09 Qt 诊断 UI、Effective Config、异步和 preview 准备 |
| `DOC/DOC_PREP_12E_R6_Preview真实模型与阶段收口准备.md` | 12E-10 preview、真实模型矩阵、Release 证据与 REPORT_12E 准备 |
| `DOC/DOC_SCHEMA_12E_TextureFillPartitionReport.md` | 12E partition report schema 与 unavailable skeleton |
| `DOC/DOC_MATRIX_12E_全局纹理填充分区验收矩阵.md` | 12E 配置、几何、分区、UI、协议和真实模型验收矩阵 |
| `REPORT/REPORT_12E_启动准备状态.md` | 12E-01 至 12E-08C 实现结果、修复专项准备与后续生产阻断状态 |
| `DOC/DOC_DECISION_12F_Release运行环境与切片性能优化专项.md` | 统一 Debug/Release Runtime、Qt 调试入口收口和后续性能专项边界 |
| `PRD/PRD_12F_Release运行环境与切片性能优化.md` | Release 运行环境、性能 KPI 与验收需求 |
| `DEV/DEV_12F_Release运行环境与切片性能优化设计.md` | NMake x64 Runtime、ToolPaths、支撑/合成/稠密 mask 优化设计 |
| `ROADMAP/ROADMAP_12F_Release运行环境与切片性能优化路线.md` | 12F R0-R5 分阶段路线；R0 已完成，算法优化未激活 |
| `DOC/DOC_ANALYSIS_OpenVDB切片功能当前不可用原因.md` | OpenVDB 已完成测试但尚不可正式切片的原因分析 |
| `DOC/DOC_RESEARCH_PrismSlicer功能处理策略与SliceSoft对照.md` | PrismSlicer 官网、论文、开源、视频与行业信息调研，以及与 SliceSoft 的能力/边界对照 |
| `DOC/DOC_ANALYSIS_11B_OpenVDB姿态配置与同姿态性能对比.md` | OpenVDB 姿态配置差异与同姿态性能对比分析 |
| `DOC/DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口.md` | OpenVDB Candidate 写包与 preview 收口决策 |
| `DOC/DOC_DECISION_11B_UI配置生产预览与OpenVDB姿态收口.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口决策 |
| `PRD/PRD_11A_R1_OpenVDB候选切片写包与Preview收口.md` | OpenVDB Candidate 写包产品需求 |
| `PRD/PRD_11B_UI配置生产预览与OpenVDB姿态收口.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口产品需求 |
| `DEV/DEV_11A_R1_OpenVDBCandidatePipeline_RGBWSVWriter设计.md` | OpenVDB Candidate pipeline 与 RGBWSV writer 技术设计 |
| `DEV/DEV_11B_UI配置生产预览与OpenVDB姿态收口设计.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口技术方案 |
| `DEV/DEV_11B_OpenVDB_LegacyCoreBenchmark设计.md` | OpenVDB 与 legacy 核心切片耗时 benchmark 设计 |
| `ROADMAP/ROADMAP_11B_OpenVDB替代Legacy生产引擎判定路线.md` | OpenVDB 替代 legacy 生产引擎的判定路线 |
| `PRD/PRD_11_UI切片层预览交互配置与多模型能力.md` | 当前 11 阶段产品需求 |
| `DEV/DEV_11_LayerPreview_UIConfig_MultiModel设计.md` | 当前 11 阶段技术方案 |
| `DEV/DEV_11_LayerPreview_DataContract.md` | 当前 11 阶段 LayerPreview 数据契约 |
| `DEV/DEV_11_MultiModel_CapabilityDecision.md` | 当前 11 阶段多模型能力边界决策 |
| `DEMO/DEMO_11_UI切片层预览交互配置验证方案.md` | 当前 11 阶段验证方案 |
| `DOC/DOC_DECISION_11_多模型切片处理范围决策.md` | 当前 11 阶段多模型范围决策 |

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
| `DOC/DOC_DECISION_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划.md` | 11A OpenVDB OBJ 彩色纹理前置计划 |
| `REPORT/REPORT_11A_OpenVDB_OBJ彩色纹理切片前置当前状态.md` | 11A OpenVDB OBJ 彩色纹理切片前置报告 |
| `DOC/DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口.md` | 11A-R1 OpenVDB Candidate 写包与 preview 收口决策 |
| `PRD/PRD_11A_R1_OpenVDB候选切片写包与Preview收口.md` | 11A-R1 OpenVDB Candidate 产品需求 |
| `DEV/DEV_11A_R1_OpenVDBCandidatePipeline_RGBWSVWriter设计.md` | 11A-R1 OpenVDB Candidate 技术设计 |
| `DEMO/DEMO_11A_R1_OpenVDB候选包与Preview验证方案.md` | 11A-R1 OpenVDB Candidate 验证方案 |
| `ROADMAP/ROADMAP_11A_R1_OpenVDB候选切片开发路线.md` | 11A-R1 OpenVDB Candidate 开发路线 |
| `REPORT/REPORT_11A_R1_OpenVDB候选切片写包当前状态.md` | 11A-R1 OpenVDB Candidate 当前状态报告 |
| `DOC/DOC_ANALYSIS_11B_OpenVDB姿态配置与同姿态性能对比.md` | 11B OpenVDB 姿态配置与同姿态性能对比分析 |
| `DOC/DOC_DECISION_11B_UI配置生产预览与OpenVDB姿态收口.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口决策 |
| `PRD/PRD_11B_UI配置生产预览与OpenVDB姿态收口.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口需求 |
| `DEV/DEV_11B_UI配置生产预览与OpenVDB姿态收口设计.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口设计 |
| `DEV/DEV_11B_OpenVDB_LegacyCoreBenchmark设计.md` | 11B OpenVDB 与 legacy 核心切片耗时 benchmark 设计 |
| `DEMO/DEMO_11B_UI配置生产预览与OpenVDB同姿态验证方案.md` | 11B UI 配置、生产预览与 OpenVDB 同姿态验证方案 |
| `ROADMAP/ROADMAP_11B_OpenVDB替代Legacy生产引擎判定路线.md` | 11B OpenVDB 替代 legacy 生产引擎判定路线 |
| `REPORT/REPORT_11B_文档规划与OpenVDB替代评估当前状态.md` | 11B 文档规划与 OpenVDB 替代评估当前状态 |
| `REPORT/REPORT_11B_UI配置生产预览与OpenVDB姿态收口当前状态.md` | 11B UI 配置、生产预览与 OpenVDB 姿态收口当前状态 |
| `DOC/DOC_DECISION_12_11B后进入切片语义引擎性能与UI产品化专项.md` | 12 阶段入口决策：12A 切片语义、12B 引擎性能、12C UI 产品化 |
| `DOC/DOC_AUDIT_12_当前切片策略与需求偏差审查.md` | 当前切片策略、需求偏差和专项拆分审查 |
| `ROADMAP/ROADMAP_12_切片语义引擎性能UI专项路线.md` | 12A/12B/12C 执行路线 |
| `PRD/PRD_12A_彩色纹理材料填充支撑光油策略.md` | 12A 彩色纹理模型材料填充、支撑与光油壳层需求 |
| `DEV/DEV_12A_彩色纹理材料填充支撑光油策略设计.md` | 12A 材料语义和切片组合技术设计 |
| `DEMO/DEMO_12A_彩色纹理材料支撑光油验证方案.md` | 12A 彩色纹理材料、支撑、光油验证方案 |
| `DOC/DIAGRAM_12A_内部镂空支撑与外侧光油支撑关系.svg` | 12A 内部镂空支撑与外侧光油、上表面支撑关系图 |
| `DOC/DIAGRAM_12A_指甲模型横截面材料示意图.png` | 12A 真实 RIP 横截面材料栈参考图 |
| `DOC/DOC_REVIEW_12A_真实RIP横截面示意图对齐审查.md` | 12A 真实 RIP 横截面材料栈审查结论 |
| `PRD/PRD_12B_切片引擎性能与OpenVDB替代评估.md` | 12B legacy/OpenVDB/高效引擎性能评估需求 |
| `DEV/DEV_12B_切片引擎性能与OpenVDB替代评估设计.md` | 12B benchmark 和引擎替代 gate 技术设计 |
| `DEV/DEV_12B_R1_LegacyHeightfield优化原型设计.md` | 12B-R1 legacy 与 heightfield 优化原型设计 |
| `DEMO/DEMO_12B_切片引擎性能验证方案.md` | 12B core-only 性能验证方案 |
| `PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md` | 12B-R2 OpenVDB SDF utility 定位需求 |
| `DEV/DEV_12B_R2_OpenVDB_SDFUtility评估设计.md` | 12B-R2 OpenVDB SDF utility 评估设计 |
| `DEMO/DEMO_12B_R2_OpenVDB_SDFUtility验证方案.md` | 12B-R2 OpenVDB SDF utility 验证方案 |
| `DOC/DOC_AUDIT_12B_R2_OpenVDB_SDFUtility代码盘点.md` | 12B-R2 OpenVDB SDF utility 当前代码盘点 |
| `DOC/DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md` | 12B-R2 OpenVDB SDF utility report schema |
| `DOC/DOC_MATRIX_12B_R2_OpenVDBSdfUtilityCapability.md` | 12B-R2 OpenVDB SDF utility 能力矩阵 |
| `DOC/DOC_AUDIT_12B_任务覆盖与R2缺口审查.md` | 12B 主任务覆盖与 R2 收口审查 |
| `DOC/DOC_ANALYSIS_12B_R1_2_5DHeightfieldFastPath可行性评估.md` | 12B-R1 2.5D heightfield fast path 可行性评估 |
| `DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md` | 12B R0/R1/R2 阶段拆分决策 |
| `DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md` | 12B core benchmark report schema |
| `ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md` | 12B 切片引擎性能分阶段路线 |
| `REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md` | 12B-R0 benchmark 契约与真实 Release 对比当前状态 |
| `REPORT/REPORT_12B_R1_LegacyHeightfield优化当前状态.md` | 12B-R1 legacy 与 heightfield 优化当前状态 |
| `REPORT/REPORT_12B_R2_OpenVDB_SDFUtility启动状态.md` | 12B-R2 OpenVDB SDF utility 启动状态 |
| `REPORT/REPORT_12B_R2_OpenVDB_SDFUtility当前状态.md` | 12B-R2 OpenVDB SDF utility 最终状态 |
| `PRD/PRD_12C_Qt_UI配置预览工作台收口.md` | 12C Qt UI 配置、Profile、预览工作台需求 |
| `DEV/DEV_12C_Qt_UI配置预览工作台设计.md` | 12C Qt UI 配置和预览工作台技术设计 |
| `DEMO/DEMO_12C_Qt_UI配置预览验证方案.md` | 12C Qt UI 配置和预览验证方案 |
| `DOC/DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md` | 12C 当前代码能力与收口缺口审查 |
| `DOC/DOC_DECISION_12C_R0_R1_R2_Qt工作台阶段拆分.md` | 12C R0/R1/R2 阶段拆分 |
| `DOC/DOC_DECISION_12C_UI产品默认值与交互冻结.md` | 12C 产品默认值与交互冻结 |
| `DOC/DOC_CHECKLIST_12C_阶段准入与上下文完整性.md` | 12C 阶段准入与上下文完整性 |
| `ROADMAP/ROADMAP_12C_Qt工作台分阶段执行路线.md` | 12C 分阶段执行路线 |
| `REPORT/REPORT_12C_Qt工作台启动状态.md` | 12C 启动状态 |
| `DOC/DOC_DECISION_12D_横截面材料无缝闭环专项.md` | 12D 横截面材料无缝闭环专项决策 |
| `PRD/PRD_12D_横截面材料无缝闭环验收与修复.md` | 12D 横截面材料无缝闭环产品需求 |
| `DEV/DEV_12D_材料闭环诊断与修复设计.md` | 12D 材料闭环诊断与修复技术设计 |
| `REPORT/REPORT_12_专项规划当前状态.md` | 12 阶段专项规划当前状态 |

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
