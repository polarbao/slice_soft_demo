# DOC 文档目录

本目录存放文档治理、决策记录、审计清单。这里的文档用于判断“哪些文档可信、哪些已归档、哪些边界不能越过”。

## 当前入口

| 文件 | 用途 |
|---|---|
| `DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md` | 正式文档体系索引和证据等级 |
| `DOC_CLASSIFICATION_2026-06-30_docs治理归档清单.md` | docs 分类、归档和当前入口清单 |
| `DOC_AUDIT_00_08_历史阶段文档缺口与补齐清单.md` | 00-08 历史阶段文档缺口审计 |
| `DOC_SCHEMA_09P_R2_experimental_openvdb_shell_report.md` | 09P-R2 experimental OpenVDB CLI report schema 契约 |
| `DOC_MATRIX_09P_R2_topology_admission_gate.md` | 09P-R2 topology admission gate 矩阵 |
| `DOC_ANALYSIS_OpenVDB切片功能当前不可用原因.md` | OpenVDB 已完成诊断/原型但尚不可作为正式切片流程的原因分析 |
| `DOC_ANALYSIS_11B_OpenVDB姿态配置与同姿态性能对比.md` | OpenVDB 姿态配置差异、同姿态性能对比和替代条件分析 |
| `DOC_CHECKLIST_10_DownstreamHandoff.md` | 10 阶段下游交付清单 |
| `DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md` | 12C 当前 Qt UI 能力、增量范围和构建 blocker |
| `DOC_CHECKLIST_12C_阶段准入与上下文完整性.md` | 12C 文档、上下文、依赖和原子任务准入检查 |
| `DOC_CHECKLIST_12C_R2_预览诊断工作区准入.md` | 12C-R2 共享层契约、稀疏 preview 规则和实施门禁 |
| `DOC_DECISION_12C_R0_01_QtMSVCFreshBuildLane.md` | 12C Qt 5.15.2 / MSVC 19.50+ fresh build lane 决策与验证 |
| `DOC_SCHEMA_12E_MeshRepairReport.md` | 12E-08C 单 case repair/strict 诊断报告契约 |
| `DOC_SCHEMA_12E_MeshRepairMatrix.md` | 12E-08C-R3-02 双 lane 真实模型矩阵汇总契约 |
| `DOC_SCHEMA_12E_MeshRepairReleaseEvidence.md` | R3-03 Release repair/global/legacy 汇总契约 |
| `DOC_EXEC_12E_08C_R3_02_真实模型RepairMatrix结果.md` | R3-02 四 case 双运行实际证据和生产阻断结论 |
| `DOC_PREP_12E_08C_R3_03_ReleaseCore与LegacyRegression准备.md` | R3-03 非生产 Release core、legacy/TIFF/RIP 回归准备 |
| `DOC_EXEC_12E_08C_R3_03_ReleaseCore与LegacyRegression结果.md` | R3-03 Release 分段证据与 legacy 回归结果 |
| `DOC_PREP_12E_08C_R3_04_08D_GO_NO_GO准备.md` | R3-04 生产准入决策输入与 Gate |
| `DOC_DECISION_12E_08C_R3_04_08D_GO_NO_GO.md` | 12E-08D 当前 NO-GO 决策与解除条件 |
| `DOC_DECISION_12E_08C_R4_模型导入预检与修复资产准入插入专项.md` | R3-04 后新增模型预检、正常模型正向链与修复资产准入决策 |
| `DOC_PREP_12E_08C_R4_模型预检与修复资产准入准备.md` | R4-01 实施依赖、停止条件和验证准备 |
| `DOC_PREP_12E_08C_R4_01_ModelPreflightContract准备.md` | R4-01 合同代码落点、golden 和定向验证准备 |
| `DOC_EXEC_12E_08C_R4_01_ModelPreflightContract结果.md` | R4-01 DTO、cache key、report schema、TDD 与验证结果 |
| `DOC_PREP_12E_08C_R4_05_CleanPositiveMatrix准备.md` | R4-05 clean OBJ/3MF width/material 正向矩阵的原子级准备 |
| `DOC_EXEC_12E_08C_R4_05_CleanPositiveMatrix结果.md` | R4-05 真实 clean OBJ/3MF width/material 正向矩阵结果 |
| `DOC_PREP_12E_08C_R4_06_RepairedAssetIntake准备.md` | R4-06 required 修复资产接收、来源、属性与 post-strict 审计准备 |
| `DOC_ANALYSIS_12E_R3_04后续可达性与模型治理.md` | R3-04 后功能可继续、生产仍阻断以及正常/required 模型双轨分析 |

## 决策记录

| 文件 | 决策 |
|---|---|
| `DOC_DECISION_09P_R2_mesh_repair_admission_gate.md` | 09P-R2 mesh repair 前置判断与 `repair_then_strict` 准入边界 |
| `DOC_DECISION_10_RIP边界与切片输出契约.md` | 10 阶段不实现 RIP，只定义切片输出契约 |
| `DOC_DECISION_11_多模型切片处理范围决策.md` | 11 阶段只做多模型评估，不默认进入 production |
| `DOC_DECISION_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划.md` | 11A 阶段在 Stage 12 前先处理 OpenVDB OBJ 彩色纹理前置计划 |
| `DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口.md` | 11A-R1 阶段新增 OpenVDB Candidate 写包与 preview 收口路径 |
| `DOC_DECISION_11B_UI配置生产预览与OpenVDB姿态收口.md` | 11B 阶段收口 UI 配置、生产预览和 OpenVDB 同姿态验证 |
| `DOC_DECISION_12C_R0_R1_R2_Qt工作台阶段拆分.md` | 12C 构建、设置、预览工作区分阶段准入 |
| `DOC_DECISION_12C_UI产品默认值与交互冻结.md` | 12C Profile、generated config、诊断布局和 12D 接入默认值 |
