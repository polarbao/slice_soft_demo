# CLAUDE_05 模块级完善建议

> 证据等级：A=代码事实，P=Claude 建议。目录位置：`docs/claude/ANALYSIS/`。每节结构：现状(A) → 缺口 → 建议(P) → 验证门。
> 建议均须在项目红线内，且遵循 `wrap first / move later / rewrite last`。

## 1. Config（`config.h` / `config.cpp` / `config/`）

**现状(A)**：`SliceConfig` 聚合约 25 个子结构；默认值集中在 `config.h`（DPI 600、层厚 0.01、通道 `R G B W S V`、背景 255）；`config.cpp`（~1030 行）负责解析/归一化/校验；`config/ConfigMigration` 处理 `slicer.config.1` 包装迁移；`config/NormalizedConfig` 近乎空壳。

**缺口**：① 5 处材料意图可重叠冲突（`material` legacy / `material_policy` / `model_fill` / `material_process_profile` / `material_role_mapping`）；② 无 `slicePipeline.mode`（双模式目标未落地）；③ `NormalizedConfig` 未承载"effective 归一"职责。

**建议(P)**：
- 建立**材料意图优先级规范**并在 effective 层固化：明确 profile > policy > legacy material 的覆盖关系，输出单一 `EffectiveMaterialPlan`，UI/报告只读它；
- 充实 `NormalizedConfig`/`EffectiveConfig`：把"默认填充 + 迁移 + 冲突消解"结果物化为一个可序列化对象，供 CLI/UI/报告共用（消除"UI effective config ≠ core actual"风险，见 12E stop 条件）；
- 为未来 `slicePipeline.mode` 预留：**先出 schema（B），在 Router 落地任务（G1 授权后）再加字段**，不要提前塞入用户配置。

**验证门**：config 正/负单测 + schema 测试；`EffectiveConfig` golden 投影；迁移前后语义等价用例。

## 2. Importers / Scene / Model（`importers/` / `scene/` / `model.*`）

**现状(A)**：真实解析（`load_stl`/`load_obj`/`load_3mf`/`parse_obj_face_vertex`）集中在 `model.cpp`（~1662 行）；`importers/Obj|Mtl|ThreeMf Importer.cpp` 各约 15 行，仅转调 `load_model_report()`；`scene/SceneModel` 为轻量容器。

**缺口**：`importers/` 名义"拥有导入"，实际是桩；`model.cpp` 承担 demo 时代聚合职责；无 strict-PASS 真实 3MF 正例资产。

**建议(P)**：
- 随管线拆解把各格式解析**真正迁入** `importers/obj|stl|3mf/`，`model.cpp` 收缩为聚合/报告；每格式解析产出稳定 `SceneModel` DTO，不直接触碰输出；
- 补一个 strict-PASS 的真实 3MF 正例资产（当前正例仍是 `texture2d_checker_cube.3mf`）；
- 保持"importers 不写 TIFF、不决定材料"的红线。

**验证门**：各 importer fixture + `model report` golden + 旧格式回归；负向 3MF 脚本继续通过。

## 3. Geometry（`geometry/` 含 `repair/`、`OpenVdb*`）

**现状(A)**：查询与诊断齐备——`TriangleIntersectionQuery`、`PointInClosedMeshQuery`、`NearestTriangleQuery`、`MeshTopology/RobustnessDiagnostics`、`DistanceField2D`；`repair/` 集群大而完整（`MeshRepairService` 746、`MeshRepairBoundary/TopologyOperations`、`MeshCompleteSelfIntersectionAnalyzer`、`MeshRepairEvidenceValidator` 934、`MeshRepairEligibilityPolicy`）；OpenVDB 为可选 conformance/utility。

**缺口**：mesh repair 仍是"保守 + 默认 OFF + 未准入"；真实模型 strict 准入 0/3；`repair-then-strict`、属性保持、真机 Release gate 未成生产链。

**建议(P)**：
- 把 repair 推进为**可审计的分级修复链**：`preflight → eligibility → conservative repair → post-repair strict → evidence`，每级产出稳定证据；坚持 `manual_repair_required ≠ pass`、confirmed self-intersection fail fast；
- 明确"哪些拓扑问题可保守自动修、哪些必须外部重建"，与 04 §4.4 资产治理工作流对接；
- OpenVDB 严格维持 optional/OFF；不因 global 便利而默认开启（架构风险，见 02 §7）。

**验证门**：`mesh_repair_*` 单测 + `run_12e_08c_*` 证据脚本；修复后重新 strict；three 必需 OBJ 出审计版并 strict-PASS。

## 4. Materials / Texture Application（`materials/`、`material/`）

**现状(A)**：`materials/` 含 policy/profile/role/varnish 与大 `texture_application/` 集群（`GlobalTextureFillPartitionService` 718、`LegacyCpuGlobalDistanceBackend`、`OpenVdbTextureFillConformanceBackend`、`TextureFillPartitionTextureTransfer`、raster mapper、full closure adapter）；另有并列 `material/`（`MaterialClosureRepair` 507、`MaterialChannelComposer`）。

**缺口**：① `material/` 与 `materials/` 命名分裂（D-08）；② 全局壳层分区仅诊断，不进生产 writer；③ 逐层合成主体仍在 `slicer.cpp` 单体。

**建议(P)**：
- 合并命名：`material/` → `materials/composition/`（composer）与 `materials/closure/`（repair），纯移动 + include 修正，先做（低风险）；
- 随管线 S3 把 `ComposeMaterialChannels` 从单体迁出，复用 `MaterialChannelComposer`，删单体内联版（D-03）；
- 保持 12E 分区的 exact partition 不变量（`texture ∪ fill = model`、交集空、width sweep 单调、all-texture 合法 fill=0），为 08D 准入铺路。

**验证门**：`texture_fill_partition_*` 全套单测 + golden；合成迁移前后 channel-hash 不变。

## 5. Support（`support/`）

**现状(A)**：`SupportShapeOptimizer`、`SupportComponentAnalysis`、`SupportShapePipeline/Policy`、`SupportPolicy`；配置覆盖 placement/island/internalVoid/shape，含 `preserveModelPriority` 与 `maxAddedSupportRatio` 约束。

**缺口**：支撑生成是**性能头号热点**（≈2801.9ms，见 03/04）；生成主体仍在单体。

**建议(P)**：
- 随 S3 把 `GenerateSupport` 迁为独立 step，暴露独立计时点，作为 12F-R2 的优化对象；
- 优化时先 profile、wrapper 化，保留 legacy 回退与 `supportPixels/channel-hash 不变`；
- 保持"support 不写报告文件"的红线。

**验证门**：`support_shape` 单测 + golden；优化前后 support 像素与 hash 不变；Release benchmark 复现。

## 6. Diagnostics / Preflight / Raster（`diagnostics/`、`preflight/`、`raster/`）

**现状(A)**：`diagnostics/` 含语义/候选材料闭环检测、`ProductionAdmissionPolicy`、`MeshRepairReport`、`TextureFillPartition*ClosureAdapter`；`preflight/ModelPreflightService`(687) + `ModelPreflightAdmissionPolicy` + 缓存标识；`raster/TextureFillPartitionRasterMapper`(445)，`RasterBoundary` 近乎 stub。

**缺口**：preflight 已是双模式准入的关键接缝，但尚未升级为 02 §5.3 的统一 `SliceEntryFacade`；`RasterBoundary` 职责待明确。

**建议(P)**：
- 把 preflight gate 正式抽象为**核心内共享 facade**（import→fastcheck→transform→preflight→admission→slice），UI/CLI 共用，消除 UI 直读单体临时结构；
- exact detector 用于生产验收，candidate（TIFF 反推）仅诊断——保持这条证据边界；
- 明确或移除 `RasterBoundary` stub，避免"只转发一次、无不变量无测试"的空层（`14_代码导读` §5）。

**验证门**：`model_preflight_*` 单测（含 pipeline gate）；facade 下 CLI 与 UI 行为一致性用例。

## 7. Pipeline（`pipeline/`）— 最高优先

**现状(A)**：`DefaultSlicePipelineSteps()` 返回 14 步名；`RunSlicePipelineLegacy()` 经预检门后整体 `run_slicer()`（`SlicePipeline.cpp:45`）；`PipelineContext` 字段未被 legacy 逐步填充；另有 `OpenVdbCandidatePipeline`(776)、`TextureFillPartitionDiagnosticComposer`、`ModelPreflightGate`。

**缺口**：见 02 §3——概念管线未落地是全项目根债。

**建议(P)**：按 02 §5.2 六步（S0 观测 wrapper → S1 步骤 DTO → S2 非热点迁移 → S3 热点迁移 → S4 Router → S5 共享 writer）推进；`PipelineContext` 升级为 `SliceStepContext`，承载 config/scene/grid/masks/stats，逐步替代单体内联结构。

**验证门**：每步 30 层 TIFF SHA-256 不变 + RIP strict + full 回归；step 级单测。

## 8. Output（`output/rgbwsv/`、`tiff_io.*`、`rip_reader.*`、`reports/`）

**现状(A)**：`tiff_io.cpp`(641) 六通道 TIFF 读写；`RgbwsvPackage` 写包；`rip_reader.cpp`(448) 严格校验（含 storage 一致性）；`reports/` 有 `ReportBase/SchemaValidator/Writer` + 大 `TextureFillPartitionReport`(921)。

**缺口**：协议常量分散（`tiff_io.h` 定义通道数，`rip_reader.h` 重复硬编码通道顺序，进度令牌双处硬编码）；多条 JSON 序列化路径并存；无输出版本化兼容策略。

**建议(P)**：
- 新增 `output/rgbwsv/RgbwsvProtocol.h` 为**协议单一真源**（通道顺序/数量/位深/极性/schema/进度令牌），writer/reader/UI/测试统一引用（还 D-05/06）；
- 收敛报告序列化为单一栈（`reports/*` + 一个 JSON 后端），逐步弱化自研 `json_value` 与多路径并存（D-07）；
- 成文**多版本包兼容/迁移策略**（D-15），为未来协议演进预留（任何真正改协议需 G4 授权）。

**验证门**：`rip_reader_test` + bad-package + golden 包；抽常量后 TIFF 逐字节不变；schema 测试。

## 9. Apps（`slicer_cli`、`rip_reader_test`、`slicer_debug_ui`）

**现状(A)**：`slicer_cli/main.cpp`(743) 提供 legacy/experimental/candidate/benchmark 模式与 `SLICE_PROGRESS` 进度；`rip_reader_test` 包校验；`slicer_debug_ui` Qt5 工作台（`MainWindow.cpp` 1611、`UiSmokeTestRunner.cpp` 2618、约 20 services + 22 widgets）。

**缺口**：UI 侧 god file（D-14）；生产 mode selector 待 08D/09B；CLI 与 UI 编排逻辑将随作业化增长。

**建议(P)**：
- 拆分 `UiSmokeTestRunner`（runner 框架 vs 各 case）与 `MainWindow`（窗口 vs 面板协调）职责；
- CLI 增加 `--mode legacy|global`（G1 后），复用统一 facade；UI 的"端到端模式选择器"待 08D/09B；
- 保持"UI 显示稳定错误码 + 友好中文、不吞 exit code、report interpreter 与 widget 分离"的红线。

**验证门**：`slicer_debug_ui --self-test` + overlay smoke；CLI 模式解析单测。

## 10. Tests / Build / Deps / Scripts

**现状(A)**：约 37 个断言式单测（无框架）；golden/schema fixture；committed golden 包；约 49 个 PowerShell 脚本；CMake `add_test` 约 30 项 + CLI 集成测试；vcpkg（json/tiff/assimp，openvdb feature）。

**缺口**：无统一测试框架（D-10）；脚本与 CTest 双轨（D-11）；仓库卫生差（构建产物入库，D-12）；Quick CI 已知红基线（D-13）。

**建议(P)**：
- 仓库瘦身：`.gitignore` 清理 `build*/`、`vcpkg_installed/`、`runtime/` 副本，迁出构建产物（低风险高回报，先做）；
- 以 **CTest label** 收敛脚本编排（quick/full/heavy/openvdb/ui），脚本改为薄封装，减少"存在即以为跑过"的误差；
- 定位并转绿 `material_process_top2 widthPx 48 vs 226`，否则显式记录豁免范围；
- 单测框架为可选中期项：可引入轻量断言头统一断言风格，暂不强推第三方框架。

**验证门**：`ctest -N` 清单对齐脚本；瘦身后完整 CI 仍绿；D-13 转绿或豁免文档化。

## 11. 模块建议优先级速览（P）

| 优先级 | 模块动作 | 关联债 |
|---|---|---|
| P0（先做，低风险）| 协议常量单一真源；仓库瘦身；material/ 合并 | D-05/06/12/08 |
| P1（根债，高杠杆）| pipeline 六步拆解 + support/compose 迁出 | D-01/02/03 |
| P1（并行）| 模型资产治理 + repair 分级链 | 03 §2 |
| P2（收口）| Router + 共享 writer → 12E-08D（G2 授权）| 02 §4 |
| P2（性能）| 12F 冻结预算并优化 support/compose | 04 §4.1 |
| P3（产品雏形）| effective 归一；facade→JobService；ProfileRegistry；UI 拆分 | D-09/14 |
