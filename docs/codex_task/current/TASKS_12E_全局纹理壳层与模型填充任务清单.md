# TASKS_12E 全局纹理壳层与模型填充任务清单

> 文档状态：R4-07-R1 READY / 12E-09A-01 COMPLETE / PRODUCTION NOT ADMITTED
> 更新日期：2026-07-23
> 当前项目原子任务：R4-07-R1 受限生产候选验证；12E-09A-02 可作为独立 diagnostic UI 后续任务
> 规则：每次只执行用户明确指定的一个 12E 原子任务

## 1. 阶段目标

实现完整三维模型上的 Texture Surface / Model Fill 互补分区，使纹理宽度可从工程最小值连续调节到模型全纹理阈值，并同步 Qt UI、report、preview、closure 和 regression。

固定边界：

```text
TextureSurface ∩ ModelFill = Empty；
TextureSurface ∪ ModelFill = Model；
width 增大时 texture 单调增加、fill 单调减少；
全纹理阈值处 fill=0、texture=model；
分类基于完整 3D 模型，不允许逐层二维近似冒充；
OpenVDB optional/OFF，不自动写 production TIFF；
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print。
```

## 2. 任务执行规则

```text
1. 每次只执行用户明确指定的一个 12E 原子任务；
2. 开始前运行 git branch --show-current 和 git status --short；
3. 阅读 Decision/PRD/DEV/DEMO/ROADMAP 和当前代码；
4. 报告无关 dirty state，不覆盖用户修改；
5. 大范围 production 接入前停下确认；
6. 每个任务运行定向验证和 git diff --check；
7. 不自动进入下一个任务；
8. 不提交，除非任务或用户明确要求。
```

## 3. 12E-00 文档准入

状态：DONE / DOCUMENTATION AND STARTUP PREPARATION ONLY（2026-07-16）

内容：

```text
新增 12E Decision/PRD/DEV/DEMO/ROADMAP/TASKS/CODEX_PROMPT；
新增 Config/DTO 准备、report schema、fixture/验收矩阵和启动状态报告；
在 12A PRD/DEV 中登记后续补充关系；
在 docs/slice 和 docs/codex_task 入口登记 planned stage；
不修改 C++、Qt、CMake、config fixture 或 production output。
```

完成标准：

```text
文档术语一致；
Current/Target/Historical/Pending 分离；
0.10 mm 工程下限、动态最大值和全纹理条件明确；
git diff --check 通过。
```

## 4. 12E-01 Config 与 DTO 契约

状态：COMPLETE（2026-07-17）

目标：

```text
新增 global_surface_shell 配置 DTO、parser、validator、report DTO 占位；
新增 complement_of_global_texture_shell scope；
未实现 backend 时在切片/写包前以稳定错误明确阻断；
旧配置默认行为不变。
```

允许修改：

```text
src/slicer_core/config.*；
src/slicer_core/config/**；
新的 partition DTO/header；
tests/unit/experimental_config；
schema/docs；
最小 config fixture。
```

禁止：

```text
不生成 3D mask；
不接入 composer；
不改 UI；
不写 production package。
```

验证：config unit tests + `git diff --check`。

实际结果：

```text
global_surface_shell 与 surfaceShell DTO 已实现；
complement_of_global_texture_shell 成对校验已实现；
9 个 E_12E_* 稳定错误码已实现；
传统与 OpenVDB 候选入口均在模型加载/写包前明确阻断；
unavailable/blocked/not_evaluated report 骨架已实现；
旧配置、12A、OpenVDB 实验配置单测保持通过。
```

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R0_ConfigDTO契约准备.md；
docs/slice/DOC/DOC_SCHEMA_12E_TextureFillPartitionReport.md；
docs/slice/DOC/DOC_MATRIX_12E_全局纹理填充分区验收矩阵.md；
docs/slice/REPORT/REPORT_12E_启动准备状态.md。
```

## 5. 12E-02 Global Partition Service 骨架

状态：COMPLETE（2026-07-17）

目标：

```text
新增 GlobalTextureFillPartitionService 和 backend-neutral result；
建立 model/texture/fill 3D mask 不变量检查；
支持 unavailable/blocked/diagnostic result；
不实现 production writer 接入。
```

完成标准：generated mask unit tests 证明 XOR/union/count invariants。

实际结果：

```text
新增 backend-neutral mesh/grid/options request、candidate 和 validated result；
新增可注入 IGlobalTextureFillPartitionBackend 的 GlobalTextureFillPartitionService；
统一重算 model/texture/fill/outside/overlap/unassigned 统计；
稳定拒绝非法 grid、mask 尺寸、非二值值和四类分区违反；
有效结果只标记 diagnostic + partitionPass，不产生 productionAcceptance=passed；
新增 texture_fill_partition_service_unit_tests，覆盖 unavailable、blocked、pass、fail 和 deterministic。
```

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R1_GlobalPartitionService骨架准备.md
```

## 6. 12E-03 Legacy CPU 3D Distance Candidate

状态：COMPLETE（2026-07-17）

目标：

```text
基于现有 mesh/BVH/geometry DTO 建立完整 3D inside/distance/closest-surface candidate；
计算 effective minimum、maxInteriorDistance、allTextureThreshold；
记录 runtime/peak memory；
严格 topology blocker。
```

禁止：

```text
不退化为逐层二维 morphology；
不在性能数据前宣称 production-ready；
不新增第三方依赖。
```

验证：box/sphere/thin-wall/cavity/topology unit/golden + benchmark report。

实际结果：

```text
新增 PointInClosedMeshQuery AABB BVH、五组确定性射线和 brute-force 测试 oracle；
新增 LegacyCpuGlobalDistanceBackend，默认 USE_OPENVDB=OFF 可运行；
对最终 grid 执行完整 3D occupancy、最近三角形欧氏距离和严格补集分区；
输出 effective minimum、maxInteriorDistance、allTextureThreshold、allTexture；
保留 triangleIndex、barycentric、distance closest-surface reference；
记录 topology/occupancy/distance/partition/totalCore、查询量和进程 peak working set；
strict topology 阻断 open、non-manifold、self-intersection 和不完整 intersection audit；
generated box/sphere/sloped/thin-wall/cavity/threshold/determinism 测试通过；
结果保持 diagnostic/not_evaluated，不写 package。
```

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R1_LegacyCpuGlobalDistanceCandidate准备.md
```

## 7. 12E-04 OpenVDB Conformance Adapter

状态：COMPLETE（2026-07-17）

目标：

```text
复用 OpenVDB level set 与 world-space sample 生成同一 DTO 的 candidate 结果；
与 CPU candidate 比较 partition、distance、threshold、runtime 和 memory；
OFF 返回 unavailable，不阻断默认 build。
```

禁止：

```text
不把 OpenVDB 设为默认；
不暴露 OpenVDB types；
不写 production TIFF；
不绕过 strict admission。
```

验证：OFF/ON 独立 build lane 和 conformance report。

实际结果：

```text
新增 backend-neutral world-space SDF sample，不暴露 OpenVDB 类型；
新增 OpenVdbTextureFillConformanceBackend，OFF 返回稳定 unavailable，ON 输出同 request grid candidate；
OpenVDB 负责 level set 与 signed occupancy，NearestTriangleQuery 提供完整同口径距离和 closest reference；
对嵌套闭合表面使用 parity interior test，使 closed cavity 保持 outside model；
新增 CPU/OpenVDB conformance DTO，记录 model/texture/fill 差异、距离差异、阈值和性能比；
严格 topology blocker 在 level-set 构建前执行；
新增 8 个 OFF/ON conformance 用例，box/sloped/thin-wall/cavity/topology/threshold/repeat 通过；
默认 OFF 与 OpenVDB ON 全量 Debug build 均通过，全量 CTest 均为 12/12 PASS，Qt UI self-test PASS；
quick CI 在既有 OBJ/MTL 输出目录名不一致处返回 E_PACKAGE_NOT_FOUND，不记录为 PASS；
结果保持 diagnostic/not_evaluated；纹理 transfer 统计不在本任务伪造，继续由 12E-06 负责。
```

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R1_OpenVdbConformanceAdapter准备.md
```

## 8. 12E-05 Width Sweep 与 Report Schema

状态：COMPLETE（2026-07-17）

目标：

```text
固化 slicesoft.texture_fill_partition.12e.1；
输出 requested/effective/min/max/allTexture；
输出 per-layer/totals overlap/unassigned/coverage；
实现 monotonic sweep validator。
```

完成标准：min/intermediate/max golden 全部通过，全纹理终点 fill=0。

实际结果：

```text
新增 TextureFillPartitionWidthSweepOptions/Sample/Result 与 EvaluateWidthSweep；
默认代表点为 minimum/25%/50%/75%/allTexture threshold，按 0.01 mm 量化并去重；
显式 full-step scan 受 maxSamples 守门，不运行伪完整 partial sweep；
新增 model 不变、texture 非递减、fill 非递增、partition 和 endpoint validator；
新增 6 个稳定 width-sweep 错误码；
BuildTextureFillPartitionReport 输出成功 grid/width/partition/per-layer/performance/query/conformance；
BuildTextureFillPartitionWidthSweepSummary 输出后端无关诊断摘要；
新增两个 golden 与 13 个 width sweep、4 个 report cases；
默认 OFF 与 OpenVDB ON 定向 CTest 均 3/3 PASS；默认全量 CTest 14/14 PASS；
结果保持 diagnostic/not_evaluated，未写生产 TIFF/manifest。
```

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R2_WidthSweep与ReportSchema准备.md
```

## 9. 12E-06 Texture Transfer 与 Diagnostic Composer

状态：COMPLETE（2026-07-17）

目标：

```text
使用 closest surface reference 为 TextureSurfaceMask3D 传递 OBJ/3MF 纹理；
按 Z layer 向 composer 提供 exact texture/fill masks；
先生成 diagnostic output/report，不改变默认 production path。
```

验证：OBJ/3MF/missing texture/missing UV/tie fixtures；outsideColored=0。

实际结果：

```text
新增 backend-neutral texture transfer，统一复用 OBJ/3MF AdaptedTriangleMesh；
TextureSurface voxel 只消费已存 closest reference，nearestQueryCount=0；
missing UV/resource/sample 支持 warn_and_fallback 与 fail_fast；
确定性 tie、缓存和引用复用均进入统计；
新增内存 Diagnostic Composer，texture 写 RGB，fill 按 white/varnish/rgb 写 W/V/RGB；
S 通道保持 255，channelOrder 保持 R G B W S V；
报告新增 textureTransfer、diagnosticComposer 与 textureTransferMs 证据；
默认 OFF 与 OpenVDB ON 定向 CTest 均 4/4 PASS；未写 production TIFF/manifest。
```

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R3_TextureTransfer与DiagnosticComposer准备.md
```

## 10. 12E-07 12D Closure 联动

状态：COMPLETE（2026-07-17）

前置：12D semantic_masks exact contract 已可用。

目标：

```text
12D 直接读取 12E exact TextureSurfaceMask/ModelFillMask；
普通模式 ColorFillGap=0；
allTexture 模式 ColorFillGap=0 或 not_applicable(reason=all_texture_partition)；
repair disabled 不改 TIFF。
```

禁止：不提前实现或修改 12D repair 规则。

实际结果：

```text
新增 TextureFillPartitionClosureAdapter；
12E exact texture/fill masks 直接映射到 12D MaterialClosureSemanticLayerInput；
普通模式 ColorFillGap=0；allTexture 输出 not_applicable(reason=all_texture_partition)；
support/varnish 明确保持 not_evaluated，不以零 mask 伪造完整 closure PASS；
报告新增 closureLinkage、真实 layerIndex/zMm、model-domain gap 和稳定错误码；
repairAttempted=false、productionOutputWritten=false；
adapter 10/10、report 6/6 用例通过；Repair Disabled TIFF SHA-256 invariant PASS。
```

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R3_12DClosure联动准备.md
```

## 11. 12E-08 Production Admission

状态：IN PROGRESS；12E-08A/08B/08C COMPLETE，12E-08C-R1/R2/R3 COMPLETE，R3-04 NO-GO，12E-08D BLOCKED

前置：

```text
R1/R2 正确性、性能、内存、topology、texture transfer 和 closure gate 通过；
用户确认 production 接入范围；
默认 OFF backend 可用，或另有正式架构决策。
```

目标：

```text
global_surface_shell 显式 Profile 可写 production RGBWSV；
allTexture 合法 fill=0；
旧 Profile 输出兼容；
RIP strict PASS。
```

本任务属于 production-path change，执行前必须再次给方案并等待确认。

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R4_ProductionAdmission准备.md
```

当前阻断：12E-08C 已完成取证，但 3 个真实 OBJ 被 strict topology 阻断，Release 预算未冻结。已插入 12E-08C-R1/R2/R3 显式 repair-then-strict 前置专项；12E-08D 需其 required-case Gate、预算和用户确认全部通过。

12E-08A 实际结果：

```text
新增 world_space_cell_containment classification-to-raster mapper；
使用真实 raster center world coordinate 与半开 classification cell ownership；
输出 model/texture/fill 精确 raster mask、texture RGB、真实 layerIndex/zMm；
输出 coverage delta、quantization error、source reuse 和 mappingMs；
新增 11 个 generated fixture cases 与 rasterMapping report golden；
默认 OFF 和 OpenVDB ON 定向验证均 2/2 PASS；
productionOutputWritten=false，productionAcceptance=not_evaluated。
```

12E-08B 实际结果：

```text
新增 TextureFillPartitionFullClosureAdapter；
消费最终 raster texture/fill、support、internal-void、surface/outer varnish 和 RGBWSV evidence；
构建完整 12D MaterialClosureSemanticLayerInput sidecar；
检查 Model > OuterVarnishShell > Support > Empty、真实层序、二值 mask 和固定通道顺序；
输出 model/support/varnish 独立状态、五类 12D gap、通道 mismatch 和 expected-domain gap；
报告新增 fullClosureLinkage、fullClosureMs 和 golden；
generated adapter 16 cases、默认 OFF 全量 19/19、OpenVDB ON 定向 2/2 PASS；
Repair Disabled RIP strict 与 30 层 TIFF SHA-256 invariant PASS；
repairAttempted=false、productionOutputWritten=false、productionAcceptance=not_evaluated。
```

结果入口：

```text
docs/slice/DOC/DOC_EXEC_12E_R4B_完整材料语义闭环结果.md
```

12E-08C 实际结果：Release build、21/21 CTest、quick regression、RIP strict 和 Repair Disabled TIFF invariant PASS；3MF fixture 分区 PASS；`nai_you_new`、`aishen_fudiao`、`meigui_fudiao` strict topology BLOCKED。结果入口：`docs/slice/DOC/DOC_EXEC_12E_R4C_默认OFFRelease真实模型与Legacy回归结果.md`。

剩余阻断：真实 OBJ repair/post-strict、属性保持、可冻结的 Release 时间/内存预算以及 12E-08D 用户明确确认。

### 11.1 12E-08C-R1/R2/R3 修复前置专项

状态：COMPLETE / NON-PRODUCTION / R3-04 NO-GO。

执行入口：

```text
docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md
```

阶段顺序：Contract & Eligibility -> Conservative Repair -> Real Model & Release Gate。R3-03 已完成
Release/global/legacy 证据；R3-04 因三个 required OBJ 未 strict PASS、预算未冻结和已知 legacy baseline
输出 NO-GO。manual required 不计入 production PASS。

### 11.2 12E-08D 双模式生产写包原子任务

状态：PREPARED / BLOCKED BY 12E-08C-R3-04 NO-GO AND USER CONFIRMATION。

固定目标：

```text
slicePipeline.mode = legacy | global_surface_shell；
legacy 为兼容默认；
两种 production success 都生成完整 p0.rgbwsv.2 RGBWSV uint8 TIFF；
两种模式共用 writer、manifest、preview/report 和 RIP Reader；
global blocked/unavailable 不得 silent fallback。
```

原子任务：

```text
12E-08D-01 Config/DTO/validator、SlicePipelineRouter 与 admission fail-closed；
12E-08D-02 global partition/full closure 到现有 production layer DTO 的 adapter；
12E-08D-03 共享 TIFF writer、manifest layer list、preview/report、RIP 和 no-fallback 测试；
12E-08D-04 显式 Profile、真实模型 Release matrix 与 GO/NO-GO。
```

准备入口：

```text
docs/slice/DOC/DOC_DECISION_12E_Legacy与GlobalSurfaceShell双切片模式.md
docs/slice/DOC/DOC_SCHEMA_12E_DualSlicePipelineConfig.md
docs/slice/DOC/DOC_PREP_12E_08D_双模式生产写包准备.md
```

### 11.3 12E-08C-R4 模型导入预检与修复资产准入

状态：R4-08 原始决策 BLOCKED；R4-08-R1 准入规则修订 COMPLETE；R4-07-R1 READY；插入于 R3-04 NO-GO 与 08D 之间。

目标：

```text
导入模型先执行 fast/full preflight；
按 legacy/global 能力输出模式相关 admission；
正常闭合模型继续推进 width/material/UI 正向验证；
至少两个独立真实模型族各有一个 strict/intake PASS 候选；
爱神、玫瑰、梯田继续作为复杂浮雕覆盖缺口审计；
全局 blocker fail-closed，不 silent fallback，不写 production TIFF。
```

原子任务：

```text
R4-01 DTO/Schema/Error/Cache Contract；
R4-02 Two-stage Preflight Service；
R4-03 Mode Admission and Pipeline Gate；
R4-04 Qt Preflight UI；
R4-05 Clean OBJ/3MF Positive Matrix；
R4-06 Repaired Asset Intake；
R4-07 Four-case Release Gate；
R4-08 08D GO/NO-GO Refresh；
R4-08-R1 受限生产候选准入规则修订；
R4-07-R1 两独立模型族候选验证；
R4-07-R2 候选预算冻结；
R4-08-R2 08D GO/NO-GO Refresh。
```

R4-01..06 软件能力和 R4-07 Development Gate 已完成。R4-08 的 2026-07-22 原始执行输出 BLOCKED。
2026-07-23 用户接受至少两个独立 strict/admitted 真实模型族的受限生产候选 Gate，xiao_ma/yecan
满足候选身份，R4-07-R1 可执行。复杂浮雕覆盖仍为 0/3；预算、Quick CI 和独立授权继续阻断 08D。

准备入口：

```text
docs/slice/DOC/DOC_DECISION_12E_08C_R4_模型导入预检与修复资产准入插入专项.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_模型预检与修复资产准入准备.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_02_TwoStagePreflightService准备.md
docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md
docs/slice/REPORT/REPORT_12E_08C_R4_08_08D_GO_NO_GO刷新状态.md
docs/slice/DOC/DOC_DECISION_12E_08C_R4_08_R1_受限生产候选准入规则.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_07_R1_受限生产候选验证准备.md
```

## 12. 12E-09 Qt UI 设置与 Effective Config

状态：12E-09A-01 COMPLETE；12E-09A-02 READY；12E-09B BLOCKED BY 12E-08D

目标：

```text
新增全局三维纹理策略；
新增 width slider + QDoubleSpinBox，0.01 mm；
模型 preflight 动态 min/max/allTexture threshold；
coverage/partition/backend status；
session effective config；
保留 modelFill.material；
普通 UI 不暴露 backend 选择。
```

验证：self-test、UI smoke、三种窗口尺寸、最长文本无重叠。

12E-09A 原子任务：

```text
12E-09A-01 只读 diagnostic facade 与 UI DTO（COMPLETE，2026-07-22）；
12E-09A-02 Effective Config 事务与派生字段；
12E-09A-03 中文 width/modelFill/status 控件；
12E-09A-04 异步 worker、取消与生命周期；
12E-09A-05 真实 layerIndex/zMm 同层 preview；
12E-09A-06 self-test、窗口 smoke、用户文档与状态收口。
```

09A 只允许 diagnostic；09B production Profile 必须继续等待 12E-08D。

09B 必须新增“传统切片 / 全局纹理壳层”中文选择器，分别写入 `legacy / global_surface_shell`。UI 需展示
requested/effective mode、admission 和 TIFF 输出状态；不得把 OpenVDB backend 当成第三种产品模式。

准备入口：

```text
docs/slice/DOC/DOC_PREP_12E_R5_QtUI与EffectiveConfig准备.md
docs/slice/REPORT/REPORT_12E_09A_01_只读DiagnosticFacade与UIDTO当前状态.md
```

## 13. 12E-10 Preview、Real Model Matrix 与收口

状态：PREPARED / DEPENDENCIES RECORDED / PRODUCTION PART BLOCKED

目标：

```text
新增 Texture Surface/Model Fill/Partition preview；
真实 OBJ/3MF minimum/intermediate/allTexture matrix；
Release runtime/peak memory；
用户手册；
REPORT_12E。
```

完成标准：文档、代码、config、report、preview、UI、RIP 和 regression 一致。

原子任务：

```text
12E-10A Texture/Fill/Partition 同层 preview；
12E-10B 真实 OBJ/3MF minimum/intermediate/allTexture matrix；
12E-10C Release core/repair/peak-memory 汇总；
12E-10D 用户手册、REPORT_12E 和上下文封口。
```

准备入口：`docs/slice/DOC/DOC_PREP_12E_R6_Preview真实模型与阶段收口准备.md`。10A 依赖 09A-05；
10B/10C 的 required family 最终证据依赖 R4-07/08；production evidence 依赖 08D。

## 14. 阶段完成标准

```text
12E-01..10 全部完成；
production admission 有明确 PASS 或 keep diagnostic 结论；
默认 OFF lane 和旧 Profile 无回归；
全纹理不是通过禁用 modelFill 实现；
REPORT_12E 记录实际命令、结果和残余风险。
```

双模式收口还要求 legacy/global 的真实模型结果分别成行；global blocked 行不得用 legacy 结果代替。两条
production PASS 行均需 TIFF、manifest、preview、report、RIP strict 和分段耗时证据。
