# REPORT_14 切片能力包封装与打印软件集成准备状态

> 文档状态：✅ **ACTIVE / IMPLEMENTATION AUTHORIZED**（2026-08-04 激活）
> 版本：v3.59 ｜ 更新日期：2026-08-07
> 本文是 Stage 14 的状态入口；Stage 12 总状态仍以 `REPORT_12X` 为准
> **S2 权威条款：`docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md`**

---

## 1. 门禁状态（v1.2 · 2026-08-04）

```text
DOCUMENTATION_GATE     = PASS          （10/10 必需文档齐备，见 §2）
IMPLEMENTATION_GATE    = AUTHORIZED    ← 用户于 2026-08-04 授权
STAGE15_PRECEDENCE     = CLEARED       （Stage 15 COMPLETE / PRODUCTION ENABLED）
EXTERNAL_EVIDENCE_GATE = DEFERRED_BY_USER
                         打印侧可实现性按假定成立；接口立即冻结；
                         打印侧、目标 RIP、干净机与实物证据不得伪记 PASS
CURRENT_NEXT_TASK      = 14F-04 S2 C1-C7 本地合同门禁
M_MVP_GATE             = PASS          （14E-01 纯 C 公开 ABI 闭环 Debug/Release PASS）
14B_PREPARATION_GATE   = PASS          （Facade/Base-Engine 实施准备已冻结）
14A_EXTERNAL_ACK       = PENDING       （14A-03 与 14A-04-R1 打印侧回签）
14F_INTERFACE_STATE    = FROZEN        （见 DOC_DECISION_14F_外部验证延期与接口冻结）
```

### 1.1 激活前置清单

| 前置 | 状态 | 证据 |
|---|---|---|
| Stage 15 优先级让位 | ✅ CLEARED | `REPORT_15` COMPLETE / PRODUCTION ENABLED |
| RIP 六问回签 | ✅ CLOSED | `DOC_CHECKLIST_14` v1.4 §4.4，两轮闭合 |
| S2 条款收敛 | ✅ DONE | `DOC_DECISION_14_S2` v1.0 |
| 作废方案清理 | ✅ DONE | `DOC_ANALYSIS_14_Q2` 降级为档案；8 项禁止实现已登记 |
| 用户授权 | ✅ 2026-08-04 | 本次激活 |

### 1.2 本轮 RIP 问答对 Stage 14 的净影响

```text
✅ Q1=方案A  → 切片与打印软件零改动，14F 具备开工条件
✅ Q3.1      → 12G-TCWS 保持冻结，p0.rgbwsv.3 不需要
✅ Q4        → 03E-02 由 NO_GO_DEFAULT 转 GO_ON_DEMAND
➕ 14A-10    → manifest 新增 whiteSemantics（本轮唯一新增实现工作）
⛔ 8 项作废  → 见 DOC_DECISION_14_S2 §4，禁止实现
🔶 S2-R1     → 极性映射表转 RIP↔打印软件双边，【不阻塞切片侧】
```

### 1.2b UI 层时序（2026-08-04 用户决策）

```text
现阶段主干 UI 布局与功能【保持原样，不做任何改造】。
M-MVP-CANDIDATE = 14C-06 全绿 + 14D-05 完成，只解锁 14E-01 纯 C 宿主。
M-MVP = M-MVP-CANDIDATE + 14E-01 PASS，才解锁 14E-02 及后续 Qt 参考宿主。
```

新增 UI 需求（均落在 `apps/slicer_ui_host_sim/`，主干不动）：
3D 视角与相机操作（14E-04c）、俯视⇄3D 切换设置项（14E-04d）。
`slicer_ui_host_sim` 同时是**交付给打印侧的参考实现**，代码质量按对外交付物要求。
`contracts/slicer_ui_view_spec.json` 已冻结设置页默认视图、top/three_d 纹理内容、
1 mm/10 mm 自适应网格、白纹理对比辅助与切换零模块调用；它不属于能力 ABI 或生产 TIFF。

> ✅ **UI-R4 切片侧 Provider 已闭合**：14A-04-R1 已冻结双视图纹理 DTO，14B-03A 与
> 14B-03A-R1 已实现 `TexturedSceneViewDataProvider`、局部轮廓、显式预算降级及 world 变换闭环，
> 并通过真实 OBJ/3MF 与 Debug/Release 门禁；14C/14E 仍需完成 SPI 适配和宿主显示。

### 1.3 仍然开放的外部项

| 项 | 归属 | 阻塞范围 |
|---|---|---|
| 极性映射表书面确认（S2-R1） | RIP ↔ 打印软件双边 | RIP 实现、S2 校验器；**不阻塞切片侧** |
| 外部目标 RIP 实机互操作 | 14F 三方联调 | 14F 出口 |
| 实物工艺验证 | 工艺侧 | 发布授权 |

## 2. 文档齐备度

| 类型 | 路径 | 状态 |
|---|---|:--:|
| 主决策 | `docs/slice/DOC/DOC_DECISION_14_…` | ✅ v1.4 |
| S2 合同 | `docs/slice/DOC/DOC_DECISION_14_S2_…` | ✅ v1.0 / SETTLED |
| UI 决策 | `docs/slice/DOC/DOC_DECISION_14_UI_…` | ✅ v1.3 |
| ViewData DTO | `docs/slice/DOC/DOC_SCHEMA_14_SceneViewData…` | ✅ v1.2 / 14A-04-R1 |
| UI 视图合同 | `contracts/slicer_ui_view_spec.json` | ✅ v1.0 / top+three_d+grid |
| 需求 | `docs/slice/PRD/PRD_14_…` | ✅ v1.2 |
| 设计 | `docs/slice/DEV/DEV_14_…` | ✅ v1.1 |
| 验收 | `docs/slice/DEMO/DEMO_14_…` | ✅ v1.1 / 78 cases |
| 状态 | `docs/slice/REPORT/REPORT_14_…`（本文）| ✅ v3.0 |
| 任务与执行指令 | `docs/codex_task/current/TASKS_14_…`、`CODEX_PROMPT_14_…` | ✅ v2.2 / v1.3 |
| 分析底稿 | `docs/claude/INTEGRATION/INT_06..17` | ✅（背景证据，不覆盖正式合同）|
| 14B 实施准备 | `docs/slice/DOC/DOC_PREP_14B_核心Facade与BaseEngine分层实施准备.md` | ✅ PREPARATION COMPLETE |

**结论：文档准入完成。** 但"文档齐备"**不等于**代码完成、不等于第三方依赖已就绪、不等于发布授权。

## 3. 实现状态（A 级核实）

```text
src/slicer_core/api/        ✅ 已建立；Facade/DTO、Scene/Model/Package/Slice 实现已落地
slicer_base / slicer_engine ✅ 已分层；base -> engine 单向依赖门禁为 0
src/slicer_module/          ✅ DLL、同步能力、WorkerClient 与模块侧第二轮产物恢复已建立
apps/slicer_worker/         ✅ Worker runtime 已建立；full preflight、repair 与 slice executor 已注册
contracts/                  ✅ 已建立；SPI、错误码、Schema 与 ViewData v1.3 已落盘
slicer_module* / .def       ✅ Debug/Release 精确 11 个冻结导出
```

**Stage 14 的 14A 切片侧实现任务已全部完成：14A-01..11（14A-08 已闭合），其中
14A-03 与 14A-04-R1 仍待打印侧书面回签；14B 已完成分层、Facade 合同、Model/Package、
SceneFacade、SliceFacade、真实纹理 Provider、DLL 与 Worker 基础链路；14E-01 纯 C 宿主已关闭
M-MVP，14E-02..04b 已完成 Qt ABI 边界、三车道交互、带纹理俯视性能门禁和
15 项公开能力覆盖。**

### 3.1 已完成原子任务

| 卡号 | 状态 | 交付物 | 验证 |
|---|---|---|---|
| 14A-01 | ✅ COMPLETE（2026-08-05） | `contracts/print_module_spi.h`、`contracts/slicer_error_codes.json`、C/C++ 合同编译探针 | 打印侧头文件逐字匹配；C/C++ Debug 编译通过；11 个声明与 19 个唯一错误码测试通过 |
| 14A-02 | ✅ COMPLETE（2026-08-05） | manifest、scene、Profile 三份 Draft 2020-12 Schema；Schema 自动验证脚本 | 真实 UI smoke manifest、既有 p0 manifest、既有 scene、Stage 15/旧 Profile 正向通过；通道顺序、whiteSemantics、zLimitMm 与 W 空值负例被拒绝；Debug/Release CTest 3/3 |
| 14A-03 | 🟡 SLICER-SIDE COMPLETE（2026-08-05） | `file_contract_v1.md`、请求/结果/协商 Schema、退出码表 | Python 合同正负例通过；Debug/Release CTest 2/2；打印侧书面确认待取得，因此不标最终 COMPLETE |
| 14A-04 | ✅ COMPLETE（2026-08-05） | 15 项能力字段级 JSON 合同、人工可读合同、ViewData 网格/LOD/blob 子操作 | Python 合同测试通过；能力数量、字段、错误码、生产协议与 Worker/ABI 边界均有漂移门禁 |
| 14A-04-R1 | 🟡 SLICER-SIDE COMPLETE（2026-08-05） | ViewData DTO 1.2、三车道合同 1.1 与 UI view spec 1.0；补齐 top/three_d 纹理、多模型 appearances、1 mm/10 mm 网格和白纹理显示约束 | DTO/三车道合同测试通过；保持 15 项能力、11 个 ABI 导出与 SPI v1；打印侧须按 1.2 回签 |
| 14A-05 | ✅ COMPLETE（2026-08-05） | 三车道机器合同与人工合同；同步补齐 Commit `currentSceneRevision` 字段 | 幂等、原子 revision、Stale 回读回滚、Production sceneHash/full preflight 门禁通过 |
| 14A-06 | ✅ COMPLETE（2026-08-05） | 取消状态机/清理机器合同与人工合同；收紧 cancelled Worker 结果 Schema | ≤2s、真实退出、双保险清理、禁止取消时发布及残留 staging 的合同测试通过 |
| 14A-07 | ✅ COMPLETE（2026-08-05） | `THIRD_PARTY_NOTICES.txt`、三项完整许可证、机器分发清单与合规审查 | miniz/LibTIFF/Assimp notice 完整性和 fail-closed 发布动作合同测试通过 |
| 14A-09 | ✅ COMPLETE（2026-08-05） | Stage 12 总览补齐 03E 按需压缩结论 | `GO_ON_DEMAND`、默认 `none` 与 14F 外部互操作边界已登记 |
| 14A-10 | ✅ COMPLETE（2026-08-05） | manifest/Profile `whiteSemantics` 解析、传播、写出与 Reader 校验 | Debug/Release 配置与 Writer 单测、Schema 合同测试和 RIP Reader 通过；冲突/非法值 fail-closed，缺字段兼容 |
| 14A-11 | ✅ COMPLETE（2026-08-05） | 可选 `zLimitMm`、显式 230 × 100 × 60 mm 设备默认、Z 超限告警及 scene DTO | Debug/Release scene/collision 单测与 Schema/DTO 合同测试 4/4；缺字段 canonical JSON 不变，超限仅告警 |
| 14B-00 | ✅ COMPLETE（2026-08-05） | 301 项文件级 base/engine 归属、4 条窄接口抽取边、独立 `model_import_layering_probe` | 合同脚本 PASS；`model.cpp + miniz` 不链接 `slicer_core` 可独立导入 OBJ；唯一结论 `model.import=base` |
| 14B-01 | ✅ COMPLETE（2026-08-05） | Qt-free Facade、DTO、`ApiResult`、`ICancelToken` 与 ViewData v1.2 内部合同 | 编译单测、头文件门禁、分层与行数门禁 PASS；未接入实现 |
| 14B-01A | ✅ COMPLETE（2026-08-05） | `slicer_base` / `slicer_engine` 单向构建图与窄接口边界 | source 唯一归属、target graph、Debug/Release 构建 PASS |
| 14B-01-R1 | ✅ COMPLETE（2026-08-05） | Package/Model Facade DTO 无损承载 capability DTO v1.2 | DTO 字段门禁、OBJ/STL 法线来源探针、Debug/Release CTest PASS |
| 14B-02 | ✅ COMPLETE（2026-08-05） | ModelFacade、PackageQueryFacade、只读 TIFF API 与 base-only 预览查询链路 | Debug/Release 专属测试 5/5、相关 TIFF/RIP/preview CTest 12/12、base-only 链接和分层门禁 PASS |
| 14B-03 | ✅ COMPLETE（2026-08-05） | SceneFacade 权威状态、完整 Commit 响应、幂等、碰撞/越界及 ViewData Provider 边界 | Debug/Release 构建及目标 CTest 6/6 PASS；正常 Commit 不追加 snapshot |
| 14B-04 | ✅ COMPLETE（2026-08-05） | SliceFacade 生产委托、单调进度与协作取消 | Debug/Release 正式目标和生产回归门禁 PASS；深度取消仍归 14D-04 |
| 14B-03A-R1 | ✅ COMPLETE（2026-08-05） | ViewData 局部轮廓、显式预算降级、three_d 纹理降采样、world 单位矩阵和材质一致性 | 独立测试、Debug/Release、真实 OBJ/3MF 及 DTO/三车道合同门禁 PASS |
| 14B-05 | ✅ COMPLETE（2026-08-05） | `--scene-config` 迁移生产 SliceFacade；保持旧错误名、进度与摘要 | Debug/Release 路由门禁 7/7 PASS；Debug full regression PASS（985.8 s） |
| 14B-06 | ✅ COMPLETE（2026-08-05） | G1..G5 source-size 门禁、带到期条件白名单、quick CI 与 CTest 接线 | 自测覆盖 G1/G2/G3；全树 G4/G5 扫描可读；受保护 Stage 14 新目录禁止白名单 |
| 14C-01 | ✅ COMPLETE（2026-08-05） | `slicer_module.dll` C ABI 外壳、`.def` 与仅链接 base 的构建目标 | Debug/Release `dumpbin` 精确 11 个无修饰导出；静态合同与 CTest PASS |
| 14C-02 | ✅ COMPLETE（2026-08-06） | 唯一 `WriteOut()` 缓冲三态实现及独立单测 | Debug/Release 探测、差 1、完整写入、空串、负容量和哨兵不变 PASS；DLL 仍精确 11 导出 |
| 14C-03 | ✅ COMPLETE（2026-08-06） | 句柄注册表、最小 job 状态与 TLS `pm_last_error` | Debug/Release 组件/ABI 测试 PASS；100 次 module 生命周期增长低于 1 MiB；真实 Worker job 关闭保留给后续 |
| 14C-04 | ✅ COMPLETE（2026-08-06） | 13 项同步轻能力、终态 job 与既有 Facade/Provider 接线 | Debug/Release 真实 DLL 调用覆盖同步清单、Worker 能力拒绝、首次 poll 终态、ViewData 纹理及包查询 fail-closed |
| 14C-05 | ✅ COMPLETE（2026-08-06） | `pm_module_info`、双 Schema 与 Debug/Release `module.json` | 运行时/部署一致性、缓冲三态、同步能力、精确 11 导出与 ABI 回归 PASS |
| 14D-01 | ✅ COMPLETE（2026-08-05） | 独立 `slicer_worker.exe` 目标和稳定参数外壳 | Debug/Release 构建、帮助/未知参数/未实现能力负例与重复确定性 PASS；D14-D-01..12 仍 NOT RUN |
| 14D-02 | ✅ COMPLETE（2026-08-06） | WorkerClient、严格 stdout 协议、退出映射与进程树回收 | Debug/Release 子进程、取消、超时、协议污染和僵尸回收门禁 PASS |
| 14D-03 | ✅ COMPLETE（2026-08-06） | `file_contract_v1` 发现和 major/minor 协商 | Debug/Release 合同发现、生产协议、能力完整性与日志边界门禁 PASS |
| 14D-04A | ✅ COMPLETE（2026-08-06） | 进程内核心取消令牌贯穿和 Writer staging 清理 | Debug/Release 取消时限、长循环检查、既有包保护与正常生产字节不变性 PASS；Worker E2E 已由 04B 闭合 |
| 14D-04B | ✅ COMPLETE（2026-08-06） | 公开 SPI 真实 Worker 取消与生命周期收口 | 活动 Worker 取消 ≤2000ms，非终态结果拒绝、重复/终态取消幂等、稳定取消码和零 owned 临时产物在 Debug/Release 下 PASS |
| 14C-06A | ✅ COMPLETE（2026-08-06） | 模块本地 SPI 一致性与无副作用自检 | Debug/Release Stage 14C 定向 CTest 11/11 PASS；历史 Worker 保留项由 06B 关闭 |
| 14C-06B | ✅ COMPLETE（2026-08-06） | Worker 生命周期 SPI 一致性 | 真实 Worker 取消、旧包保护、非终态 result 拒绝和幂等 cancel 在 Debug/Release 连续 3 次 PASS |
| 14C-06 | ✅ COMPLETE（2026-08-06） | C-SPI-01..18 公开 ABI 一致性 | 14C-06A/06B 合并全绿；与已完成 14D-05 共同满足 `M-MVP-CANDIDATE` |
| 14C-07 | ✅ COMPLETE（2026-08-06） | DLL 初始化与依赖红线 | Debug/Release 5/5 定向测试、并发 call_once、32 实例、精确 11 导出和 PE 依赖红线 PASS |
| 14D-05 | ✅ COMPLETE（2026-08-06） | 安全发布与清理双保险 | R1..R4 完成；真实 Worker 与公开 DLL 链路均验证取消/强杀后的旧包保护和 owned 临时产物清零 |
| 14D-05-R1 | ✅ COMPLETE（2026-08-06） | 共享产物身份与恢复状态机 | job/attempt 精确 owned 路径、临时路径识别、reparse fail-closed、备份恢复与相邻作业保护通过定向门禁 |
| 14D-05-R2 | ✅ COMPLETE（2026-08-06） | Writer owned 发布事务 | job/attempt 贯穿生产链路；精确 staging/backup、目标级租约、发布后复验、无残留证据和并发拒绝通过 Debug/Release 定向门禁 |
| 14D-05-R3 | ✅ COMPLETE（2026-08-06） | Worker/模块双重恢复与临时路径拒绝 | Worker 起止第一轮、进程退出后模块第二轮共享精确 owned 恢复；查询/RIP 拒绝临时路径，Writer 私有验证不泄漏；Debug/Release 定向门禁 PASS |
| 14D-05-R4-A | ✅ COMPLETE（2026-08-06） | 真实 Worker 正常、取消、超时强杀与旧包保护 | Debug/Release 生产 Worker + WorkerClient 集成 PASS；有效包严格可读，取消/强杀后 manifest 字节不变且 owned staging/backup/lease 无残留 |
| 14D-05-R4-B | ✅ COMPLETE（2026-08-06） | 公开 DLL -> Worker -> Writer 与 C-SPI-09 收口 | Debug/Release 公开 C SPI 取消返回稳定代码；既有 manifest 字节不变，owned staging/backup/lease 无残留且包仍可严格读取 |
| 14D-06 | ✅ COMPLETE（2026-08-06） | Worker 唯一重能力路由 | R1/R2 完成；公共 SPI 异步执行 full preflight/repair/slice，poll/cancel/result/release/destroy 闭合，Debug/Release 与合同门禁 PASS；module 不链接 engine |
| 14D-07 | ✅ COMPLETE（2026-08-07） | 引擎一致性套件 E-01..08 | R1 合同/fixture/runner 与 R2 当前 Worker Gate 均完成；Debug/Release E-01..08 PASS |
| 14D-08 | ✅ COMPLETE（2026-08-07） | Worker 独立 `--spi-request` | R1..R4 全部完成；三真实 executor、共享 runtime、安全发布、取消、RIP strict、无 fallback 与 IDE 直接调试闭合 |
| 14D-08-R1 | ✅ COMPLETE（2026-08-06） | 共享请求解析、身份、结果与调度基础 | R1-01..03 COMPLETE；生产 Worker 在无 executor 时身份闭合地显式失败 |
| 14D-08-R1-01 | ✅ COMPLETE（2026-08-06） | `request.json` 严格解析与不可变作业身份 | Debug/Release 5/5 定向门禁 PASS；不创建 result/package，不接入 executor |
| 14D-08-R1-02 | ✅ COMPLETE（2026-08-06） | `result.json` 身份闭合与原子替换 | Debug/Release 4/4 定向门禁 PASS；写入失败稳定映射 OUTPUT-0050/exit 6 |
| 14D-08-R1-03 | ✅ COMPLETE（2026-08-06） | 三能力精确调度与共享命令入口 | Debug/Release 8/8 定向门禁 PASS；测试 fake 未进入生产 Worker，无 executor 时不伪成功 |
| 14D-08-R2 | ✅ COMPLETE（2026-08-07） | 切片请求映射与真实执行 | R2-01/02 实现；R2-03 由安全发布、真实 Package/RIP strict 和独立入口正负例闭合 |
| 14D-08-R2-01 | ✅ COMPLETE（2026-08-06） | scene/Profile 双 hash、绝对资源、输出身份和 job 目录物化 | Debug/Release 1/1，R1+R2 回归 4/4，target graph PASS；未注册 executor、未写 package |
| 14D-08-R2-02 | ✅ COMPLETE（2026-08-06） | 权威 full preflight 后调用唯一生产 SliceFacade | Debug/Release R2/R3 定向门禁 PASS；修正 production acceptance 合同；未注册 Worker、未宣称安全发布 |
| 14D-08-R3 | ✅ COMPLETE（2026-08-06） | 权威 full preflight 与 repair Facade 适配 | R3-01A/01B 与 02B COMPLETE；三项重能力均具备真实 Worker executor |
| 14D-08-R3-01A | ✅ COMPLETE（2026-08-06） | 全场景资源、拓扑、显式目标模式、越界和碰撞权威聚合 | Debug/Release 1/1 PASS；预算未完成、stale、资源变化和取消均 fail-closed；未接 Worker |
| 14D-08-R3-01B | ✅ COMPLETE（2026-08-06） | PreflightFullFacade、生产等价 Profile 重建与 Worker executor | Debug/Release 定向测试 PASS；Worker 与直接 Facade 身份/计数/admission 一致；资源、stale、取消 fail-closed；不生成 Package |
| 14D-08-R3-02A | ✅ PREPARATION COMPLETE（2026-08-06） | repair 请求、job-owned 输出与 strict 复检合同 | 字段级合同已冻结；后续审计发现生产资产 Writer 和单模型 strict adapter 尚缺 |
| 14D-08-R3-02B | ✅ COMPLETE（2026-08-06） | RepairFacade 与 Worker executor | W2 Writer、S1 strict adapter、F1 Facade 与 E1 Worker 接线全部完成 |
| 14D-08-R3-02B-W2 | ✅ COMPLETE（2026-08-06） | 项目内确定性 OBJ/MTL Writer 与资源复制 | Debug/Release 门禁 PASS；UV、材质分配、MTL 和纹理字节保持，缺失资源 fail-closed |
| 14D-08-R3-02B-S1 | ✅ COMPLETE（2026-08-06） | staged OBJ 重导入与单模型完整 strict 复检 | Debug/Release 门禁 PASS；Profile hash、geometry/attribute identity、完整自交审计均 fail-closed |
| 14D-08-R3-02B-F1 | ✅ COMPLETE（2026-08-06） | ProductionRepairFacadeFactory | 保守 OBJ repair、Writer、重导入 strict 证据和 job staging 清理闭合；由 E1 接入生产 Worker |
| 14D-08-R3-02B-E1 | ✅ COMPLETE（2026-08-06） | 生产 Worker repair executor 与 job-owned 发布 | `geometry.repair` 精确注册；OBJ、相邻资源与 strict 证据发布闭合，取消、路径越界、Profile 漂移和发布异常 fail-closed；Debug/Release 门禁 PASS |
| 14D-07-R1 | ✅ COMPLETE（2026-08-06） | 参数化 E-01..08 合同、fixture 身份、runner 与定义门禁 | R1 外壳完成；执行结论由 R2 独立产生 |
| 14D-07-R2 | ✅ COMPLETE（2026-08-07） | 当前 Worker E-01..08 完整 Gate | Debug/Release 真实 Worker PASS；生产包协议、golden、报告、负例、取消恢复及参数化替换边界闭合 |
| 14D-08-R4 | ✅ COMPLETE（2026-08-07） | Worker 独立调试入口父任务收口 | Debug/Release Stage 14D-08 各 10/10；VS Code 直接 Worker 调试、可移植请求生成与无 fake/fallback 门禁 PASS |
| 14E-01 | ✅ COMPLETE（2026-08-07） | `apps/slicer_host_sim/` 纯 C 参考宿主；运行时装载 11 个公开导出，完成导入、变换、Worker 切片、取包、校验和 fail-closed | Debug/Release Stage 14E-01 各 2/2 PASS；15 能力/三车道/源码行数门禁 PASS；形成 M-MVP |
| 14E-02 | ✅ COMPLETE（2026-08-07） | `apps/slicer_ui_host_sim/` Qt 参考宿主骨架与 `ModuleClient`；运行时解析 11 个导出并提供统一作业 API/调用计数 | Debug/Release Stage 14E-02 各 3/3 PASS；源码/CMake/PE 导入表三重依赖守卫及 DLL 缺失 fail-closed PASS |
| 14E-03 | ✅ COMPLETE（2026-08-07） | `SceneInteractionController` + `TransformCommitPolicy`；本地 Transient、原子 Commit、显式初始刷新与 Stale 快照恢复 | Debug/Release 真实 DLL 合同测试 PASS；50 次 transient 更新跨 DLL 调用为 0；正常 Commit 不追加 snapshot；Stale 丢弃本地状态且不自动重试 |
| 14E-04 | ✅ COMPLETE（2026-08-07） | `TopViewRenderPolicy` + `MoveOptimizationPolicy`；top 真实纹理 blob、双身份缓存、宿主本地移动和 retained render | Debug/Release Stage 14E-02..04 各 6/6；60 次 Commit P95=0.3819 ms；300 本地帧零 ABI；30 秒 UI-M3=133.303% |
| 14E-04b | ✅ COMPLETE（2026-08-07） | Qt 宿主 15 项公开能力覆盖 runner、机器证据、UI-M5 真实 Worker 取消与 UI-M6 缺 DLL 门禁 | Debug/Release Stage 14E-02..04b 各 7/7；P0=5/5、P1=5/5、P2=6/6；取消 103 ms 且 owned 临时产物零残留 |
| 14E-04c | ✅ COMPLETE（2026-08-07） | `IRenderBackend`、`cpu_raster`、`SceneRenderPolicy`、`AppearanceCache` 与本地相机控制器闭合带纹理 three_d 显示 | Debug/Release Stage 14E-02..04c 各 8/8；UI-M7=0 次 DLL 调用；100352 三角面 30 秒 UI-M8 P5=51.4168 FPS |
| 14E-04d | ✅ COMPLETE（2026-08-07） | `ViewModeSwitch`、中央俯视/3D 分段入口、session config 默认视图/投影持久化，以及双视图 buildVolume 自适应网格和白纹理对比 | Debug/Release Stage 14E-02..04d 各 9/9；UI-M9..13 PASS；100 次本地切换 DLL 调用为 0；missing texture 显式失败 |
| 14E-05 | ✅ COMPLETE（2026-08-07） | 主干 UI 大文件按职责拆分并关闭临时白名单 | Debug/Release 自测与代表性 UI Smoke PASS；新增实现文件均低于 500 行 |
| 14E-06 | ✅ SLICER-SIDE COMPLETE（2026-08-07） | 打印侧文件级可移植清单与自动完整性验证 | 46 个宿主源/构建文件已登记；打印侧 ACK 仍待外部确认 |
| 14E-07 | ✅ COMPLETE（2026-08-07） | VSCode 中新增独立 `SliceSoft 14E:` Debug/Release 编译、运行、自检与调试入口 | 原 UI Debug/Release 构建、部署、启动通过；封装宿主 Debug/Release SPI 自检与启动通过 |
| 14F-01 | 🟡 SLICER-SIDE COMPLETE（2026-08-07） | `modules/slicer/` Release 打包脚本、依赖 inventory、哈希、NOTICE/许可证与本地隔离验证 | 包内 Worker 合同和纯 C 宿主 import→slice→verify PASS；独立干净机装载仍 NOT RUN，14F-02 外部依赖 |
| 14F-02-PREP | 🟡 SLICER-SIDE READY（2026-08-07） | 打印侧 M1 handoff、公开合同、独立装载/能力/自检探针、缺 DLL 负例和执行手册 | Release M1 探针、handoff 哈希和本地接收门禁 PASS；打印侧 ModuleRegistry、进程模块清单及 ACK 仍待外部完成 |
| 14F-02-FREEZE | ✅ COMPLETE（2026-08-07） | 冻结 SPI v1、11 导出、15 能力、DTO v1.4、三车道 v1.1、Worker 文件合同、S1/S2 与交付边界 | 用户授权按打印侧可实现继续推进；外部验证统一标记 DEFERRED，不伪造 PASS |
| 14F-03 | ✅ SLICER-SIDE COMPLETE（2026-08-07） | 单模型公开 ABI import→transform→Worker slice→S1 strict 正例及 7 类负例 | Release 生成 3 层 `p0.rgbwsv.2` 包；正例 1/1、负例 7/7、CTest 1/1 PASS；打印侧 M2 外部验证延期 |

实际 DLL 已由 14C-01 建立，并在 14C-07 使用 Debug/Release `dumpbin /EXPORTS` 再次确认精确
11 个冻结符号；C-SPI-01..18 已由 14C-06A/06B 合并关闭。

## 4. 已裁定事项

| 编号 | 事项 | 结论 | 日期 |
|---|---|---|---|
| D-1 | 优先级插入方案 | **乙 并行插入**（12E-09D 走既有序列，14A/14B/14C 并行）| 2026-08-03 |
| D-2 | TIFF Writer 后端 | **手写 Writer 保持默认、LibTIFF 保持可选**；默认切换未授权，PackBits 仅按需开启 | 2026-08-05 基线收口 |
| D-3 | 白区语义传递 | **S2 路径 D**：白色使用 `255/255/255/0/255/255` 的 W 真实材料语义；`whiteSemantics` 为作业级声明；WSV 哨兵、sidecar、`p0.rgbwsv.3` 均禁止 | 2026-08-04 S2 定案 |
| D-4 | Worker 定位 | **可独立迭代替换的切片引擎**；core 拆 base/engine；切片只在 Worker | 2026-08-03 |

## 5. 未决项

| 编号 | 事项 | 需谁答 | 阻塞 |
|---|---|---|---|
| OPEN-14-06 | 三个必需 OBJ 处置 | **产品** | 真实模型 E2E（可用 7 个 strict-PASS 资产解耦）|
| OPEN-14-07 | S2-R1 极性映射表 | **RIP 侧 + 打印侧** | 不阻塞切片侧；阻塞最终物理映射验收 |

`OPEN-14-03/04/05` 已由 `DOC_DECISION_14_S2` 关闭；旧问题及候选方案仅保留在档案文档中。

## 6. 可立即启动的准备任务（不受未决项阻塞）

| 卡 | 任务 | 估算 |
|---|---|---:|
| 14C-01 | SPI DLL 外壳与精确 11 符号导出 | 可启动 |
| 14D-01 | 独立 `slicer_worker.exe` | 可与 14C-01 并行 |
| **下一关键路径** | | **14C 与 14D 并行、共享集成串行** |

本卡不与已完成阶段抢文件（所有权见 `TASKS_14` §8）；14A-08/09 已完成，不得重复执行。

## 7. 与其他阶段的边界

```text
12E-09D / 12E-10  并行，不阻塞（Stage 14 首版能力面语义已由 12E-09B/09C 收口）
12F               Stage 14 的步骤/分层边界是其前置；仍按实测证据逐项授权
13F-R1            独立并行；其 Cancelling≠Cancelled 已被 Stage 14 采纳为契约条款
12G-TCWS          保持冻结；Stage 14 不实现，仅把白区语义列为对 RIP 确认项
03D               已 COMPLETE / GO_OPTIONAL；手写 Writer 保持默认，LibTIFF 可选
03E               03E-02 GO_ON_DEMAND；PackBits 可显式开启，默认仍为 none
```

## 8. 风险提示

```text
① 默认 Writer 仍为手写实现；LibTIFF 仅为可选后端，禁止在 Stage 14 内静默切换默认值；
② RIP 书面合同已闭合，但目标 RIP 实机互操作仍必须由 14F 留证；
③ base/engine 分层的三个高风险切分点（model.cpp / geometry / reports）需 14B-00 先验证；
④ 若无 CI 单向依赖门禁（14B-06 + P4），分层将在数月内退化回单库；
⑤ 14A-04-R1 双视图纹理合同与 14B-03A Provider 已闭合；14C/14E 必须复用该 Provider，禁止另造 DTO 或灰模降级；
⑥ top/three_d 网格与白纹理辅助只影响参考宿主显示，不得渗入几何真值、切片采样或 TIFF。
```

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。文档齐备度 8/8、实现量 0；记录 D-1..D-4 四项裁定与 OPEN-14-03..08 六项未决；列出可立即启动的七卡（8–12 人日）|
| 2026-08-03 | v1.1 | 同步 Q2 深度审查：撤回当前阶段 sidecar 推荐；记录完整配置/贴图碰撞范围；将 OPEN-14-04 改为确认既有 WSV=000 或 W-only Profile 六通道路径 |
| 2026-08-05 | v1.3 | Stage 14 开工基线收口：文档门更新为 10/10；D-2/D-3 对齐 03D/03E 与 S2 权威结论；移除已关闭开放项和已完成 14A-08；登记独立宿主模拟与 ViewData DTO 风险 |
| 2026-08-05 | v1.4 | 完成 14A-01：建立 contracts、同步 11 函数 SPI 头文件、登记 19 项错误码并通过 C/C++ 合同测试；下一任务推进为 14A-02 |
| 2026-08-05 | v1.5 | 完成 14A-02：形式化 manifest、scene、Profile 三份 JSON Schema，覆盖 Stage 15 三字段、`whiteSemantics`、`zLimitMm`，并通过真实/既有样例兼容与负向合同测试；下一任务推进为 14A-03 |
| 2026-08-05 | v1.6 | 完成 14A-03 切片侧 `file_contract_v1` 合同与自动验证；因打印侧书面确认尚未回签保持 SLICER-SIDE COMPLETE，切片侧下一任务推进为 14A-04 |
| 2026-08-05 | v1.7 | 完成 14A-04：冻结 15 项能力字段级 DTO，并把 Scene ViewData 的 local 网格、LOD、双身份缓存和既有 ABI blob 分块纳入合同；下一任务推进为 14A-05 |
| 2026-08-05 | v1.8 | 完成 14A-05：冻结三车道交互、operationId 幂等、SceneRevisionStale 回滚与 Production 准入；下一任务推进为 14A-06 |
| 2026-08-05 | v1.9 | 完成 14A-06：冻结取消状态机、2000ms 协作/Job Object 兜底与 staging 双保险清理；下一任务推进为 14A-07 |
| 2026-08-05 | v2.0 | 完成 14A-07：落盘第三方 NOTICE、assimp/miniz/LibTIFF 完整许可证、分发清单与合规门禁；下一任务推进为 14A-09 |
| 2026-08-05 | v2.1 | 完成 14A-09：Stage 12 总览补齐 03E `GO_ON_DEMAND`、默认不压缩与 14F 外部互操作边界；下一任务推进为 14A-10 |
| 2026-08-05 | v2.2 | 完成 14A-10：落地 manifest 权威、Profile 默认的 `whiteSemantics`；冲突与非法值 fail-closed，旧包缺字段兼容；下一任务推进为 14A-11 |
| 2026-08-05 | v2.3 | 受控完成 14A-04-R1 切片侧合同修订：top/three_d 均要求真实纹理，补齐 ViewData 外观 DTO 与三车道调用约束；打印侧须基于 DTO 1.2 重新回签 |
| 2026-08-05 | v2.4 | 完成 14A-11：落地可选 Z 限高、显式设备默认、超限非阻断告警和 scene 响应字段；14A 切片侧实现收口，后续转入 14B-00/06，打印侧回签继续独立跟踪 |
| 2026-08-05 | v2.5 | 补齐 UI view spec 1.0、1 mm/10 mm 双视图网格、白纹理对比、多模型 appearances 与 78 项验收入口；14B-03A Provider 仍为 14E 硬前置 |
| 2026-08-05 | v2.6 | 完成 14B 实施准备：冻结 Facade、base/engine 构建迁移、任务顺序、文件所有权、验证矩阵、回滚条件与机器门禁；下一开发任务为 14B-00，14B-06 可并行 |
| 2026-08-05 | v2.7 | 完成 14B-00：以独立编译探针确认 `model.import=base`，登记 301 项文件归属与 4 条窄接口抽取边；下一任务为 14B-06 与 14B-01 |
| 2026-08-05 | v2.8 | 完成 14B-06：G1..G5 门禁、到期白名单、quick CI 与 CTest 接线生效；14B 顺序推进为 14B-01 |
| 2026-08-05 | v2.9 | 完成 14B-01：建立 Qt-free Facade/DTO/取消合同并接入 CTest；下一任务推进为 14B-01A |
| 2026-08-05 | v3.0 | 完成 14B-01A：落地 `slicer_base` / `slicer_engine` 两库、窄化模型与 DPI 配置边界、生成式 source 唯一归属与单向依赖门禁；14B-02/03/04 可并行开发 |
| 2026-08-05 | v3.1 | 完成 14B-01-R1：内部 Facade DTO 完整承载能力合同 v1.2，补齐 package summary/layer/verify/report 与模型法线来源；外部 ABI、能力数量、生产 TIFF 均未变化 |
| 2026-08-05 | v3.2 | 完成 14B-03：SceneFacade 权威 Commit、双 revision、完整响应、幂等、碰撞/越界及正式 Debug/Release target 门禁通过；真实双视图纹理仍由 14B-03A 解锁 |
| 2026-08-05 | v3.3 | 完成 14B-04：SliceFacade 以既有生产服务为唯一执行入口，正式 Debug/Release target、单调进度、协作取消和生产回归门禁通过；14D-04 继续负责深度取消与 2 秒上限 |
| 2026-08-05 | v3.4 | 完成 14B-02：ModelFacade 与 PackageQueryFacade 接入权威模型/包能力，生产 TIFF Reader、缓存和材料预览形成 base-only 查询链路；Writer 与 backend 选择保持 engine，14B-03A/05 可并行 |
| 2026-08-05 | v3.5 | 完成 14B-03A：top/three_d 真实纹理 Provider 接入正式 base target，真实 OBJ/3MF、纯白/近白、多模型 appearance、预算与 fail-closed 的 Debug/Release 门禁通过；下一任务为 14B-05 |
| 2026-08-05 | v3.6 | 完成 14B-03A-R1：合同审计发现的 outline、静默降级、world 重复变换、请求字段和材质显示差异已闭合；下一任务保持 14B-05 |
| 2026-08-05 | v3.7 | 14B-05 实现与 Debug/Release 路由门禁完成，full regression 执行中；通过后 14B 收口并并行进入 14C-01 / 14D-01 |
| 2026-08-05 | v3.8 | 完成 14B-05：Debug full regression 以 985.8 s 通过，14B 核心 Facade 阶段收口；下一批 14C-01 与 14D-01 可并行，合同/CMake/集成提交继续串行审查 |
| 2026-08-05 | v3.9 | 完成 14C-01：建立只依赖 slicer_base 的 DLL ABI 外壳，Debug/Release 精确 11 导出通过；14C-02/03 与 14D-01 可按文件所有权并行 |
| 2026-08-05 | v3.10 | 完成 14D-01：独立 Worker 进程目标和参数外壳落地，不伪造合同能力；下一批 14C-02、14C-03 与 14D-02 可并行 |
| 2026-08-06 | v3.11 | 完成 14C-02：唯一缓冲三态实现和 Debug/Release 单测闭合；并行批次余下 14C-03 与 14D-02，后续仍按共享 CMake/导出接线串行集成 |
| 2026-08-06 | v3.12 | 完成 14C-03：句柄/TLS 基础设施及当前 ABI 接线通过 Debug/Release 门禁；14D-02 继续集成，14C-04 已解除代码前置但须避开共享 CMake/Exports 冲突 |
| 2026-08-06 | v3.13 | 完成 14D-02：WorkerClient 子进程、协议解析、退出映射、取消/超时与进程树回收通过 Debug/Release 门禁；下一批 14C-04 与 14D-03 准备门禁 PASS，14C-05/07 因共享 DLL 文件转入串行集成 |
| 2026-08-06 | v3.14 | 完成 14D-03：`--contract-info` 与模块侧合同协商通过 Debug/Release 门禁，版本、生产协议、能力和 stdout 日志边界均 fail-closed；14C-04 继续并行收口 |
| 2026-08-06 | v3.15 | 完成 14C-04：同步轻能力通过真实 SPI 接入既有 Facade/Provider，首次 poll 即终态；Worker 能力、full preflight、非法请求与纹理失败均 fail-closed；下一批先审计 14C-05/14D-04 准备门禁 |
| 2026-08-06 | v3.16 | 完成下一批准备门禁：14C-05 双 Schema、模块版本/CRT 与部署一致性已冻结；14D-04 拆分为核心 token 贯穿 04A 和依赖 14D-05/08 的 Worker E2E 04B；14C-05/14D-04A 获准并行 |
| 2026-08-06 | v3.17 | 完成 14C-05 与 14D-04A：模块自述/部署和核心协作取消通过 Debug/Release 门禁；下一批 14C-07 可实施，14D-05 先做准备审计，14D-08 保持显式阻断 |
| 2026-08-06 | v3.18 | 完成 14C-07：最小 DLL 入口、进程级一次初始化、并发实例、精确 11 导出和无 Qt/PrintSDK/Engine 依赖通过 Debug/Release 门禁；14C-06A 准备门转 READY，14D-05/06/07 准备审计均保持显式阻断，下一并行批次为 06A 实现与 14D-08 解阻拆分准备 |
| 2026-08-06 | v3.19 | 完成 14C-06A：公开 C ABI 动态装载一致性程序与无副作用 pm_self_test 通过 Debug/Release 11/11 门禁，Worker 项诚实保留给 06B；14D-08 受控拆为 R1..R4，R1 准备门 PASS，下一开发卡为 R1-01 |
| 2026-08-06 | v3.20 | 完成 14D-08-R1-01：严格请求解析、原始业务对象保留和 normalized 作业身份通过 Debug/Release 5/5 门禁；R1-02 准备门转 READY，父任务仍等待结果封装、调度与真实执行 |
| 2026-08-06 | v3.21 | 完成 14D-08-R1-02：身份闭合结果、稳定退出类别和 result.tmp 原子替换通过 Debug/Release 4/4 门禁；R1-03 准备门转 READY，父任务继续 BLOCKED |
| 2026-08-06 | v3.22 | 完成 14D-08-R1-03：三能力精确注册、共享 runtime、取消前检与命令入口通过 Debug/Release 8/8；R1 COMPLETE，下一步先补 14D-08-R2 映射准备合同 |
| 2026-08-06 | v3.23 | 完成 14D-08-R2-01：scene/Profile 双 hash、绝对路径、Profile/输出身份和 job 内原子物化通过 Debug/Release 门禁；同时冻结 R3 拆分与 E-01..08 合同，下一步并行准备 R3-01A 和建设 14D-07-R1 外壳 |
| 2026-08-06 | v3.24 | 完成 14D-08-R3-01A/02A 字段级准备：冻结全场景权威预检 DTO、完整性/准入聚合、repair 输入/输出资产/strict 复检和清理顺序；R3-01A 与 14D-07-R1 可并行开发 |
| 2026-08-06 | v3.25 | 完成 14D-07-R1：落地参数化 E-01..08 合同、固定 fixture SHA-256、定义门禁和 Worker runner；真实能力未完成时只输出 BLOCKED，完整准入保留给 R2 |
| 2026-08-06 | v3.26 | 完成 14D-08-R3-01A：全场景权威预检服务对 committed scene 的资源、完整 topology、显式目标模式、越界和碰撞进行稳定聚合；Debug/Release 定向门禁通过，下一步准备 R3-01B Worker/API 适配 |
| 2026-08-06 | v3.27 | 完成 R3-01B/02B 准备复核：发现跨进程 scene 缺 effective Profile/target mode 身份，以及 repair 缺生产资产 Writer/单模型 strict adapter；新增 `14A-04-R2` 受控修订提案，未授权前不修改机器合同或生产代码 |
| 2026-08-06 | v3.28 | 用户授权并完成 14A-04-R2：能力 DTO 升至 v1.3，full preflight 冻结 scene/Profile/hash/targetMode 身份，repair 首版冻结项目内 OBJ/MTL Writer 与完整证据；SPI v1、11 导出、15 能力和生产协议不变 |
| 2026-08-06 | v3.29 | 完成 14D-08-R3-01B：权威 preflight Facade 按完整 Profile 重建生产几何，Worker 严格物化 scene/Profile 并输出全场景证据；Debug/Release 门禁通过，下一任务转 14D-08-R2-02 |
| 2026-08-06 | v3.30 | 完成 14D-08-R2-02：文件合同经权威 full preflight 调用唯一生产 SliceFacade 并生成受控开发 RGBWSV Package；修正 production acceptance 值，stale admission 与取消 fail-closed；下一任务转 R3-02B 与 14D-05 |
| 2026-08-06 | v3.31 | 完成 14D-08-R3-02B-W2：新增项目内确定性 OBJ/MTL Writer，稳定保留 UV、材质分配与纹理字节；Debug/Release 与合同门禁通过，下一任务为单模型 strict recheck adapter |
| 2026-08-06 | v3.32 | 完成 14D-08-R3-02B-S1：staged OBJ 按 effective Profile 资源规则重导入，geometry/attribute hash 与完整 strict 审计闭合；下一任务为 ProductionRepairFacadeFactory |
| 2026-08-06 | v3.33 | 完成 14D-08-R3-02B-F1：生产 RepairFacade 串联 Profile/path identity、保守修复、确定性 Writer 和完整 strict 复检，成功仅处于 job staging；下一步重新审计 14D-05 安全发布门 |
| 2026-08-06 | v3.34 | 完成 14D-05 复审及 R1：准备门按 R1..R4 拆分后转 PASS_WITH_SPLIT，共享 job/attempt 产物身份、临时路径识别和精确 owned 恢复组件落地；下一任务为 R2 Writer 发布接线 |
| 2026-08-06 | v3.35 | 完成 14D-05-R2：SliceFacade 作业身份贯穿场景生产与 RGBWSV Writer，owned staging/backup、目标级租约、发布后严格复验和清理证据闭合；同目标并发在写包前稳定拒绝，下一任务为 R3 双保险清理与临时路径读取拒绝 |
| 2026-08-06 | v3.36 | 完成 14D-05-R3：生产 Worker 注册 `slice.rgbwsv`，Worker 起止与模块退出后双重恢复 exact-owned 产物；PackageQuery/RIP 临时路径 fail-closed，Debug/Release 定向测试与四项合同门禁通过；下一任务为 R4 强杀/崩溃与真实 Worker 验收 |
| 2026-08-06 | v3.37 | 完成 14D-08-R3-02B-E1：生产 Worker 注册 `geometry.repair`，job-owned 修复资产、相邻资源和 strict 证据安全发布；取消、越界和异常清理闭合，Debug/Release repair/strict/Worker 与合同门禁通过；14D-08-R3 收口，下一任务保持 14D-05-R4 |
| 2026-08-06 | v3.38 | 完成 14D-05-R4-A：真实生产 Worker 经 WorkerClient 的正常发布、协作取消与零宽限超时强杀通过 Debug/Release；旧有效包字节保持且 owned 临时产物无残留。R4-B 必须等待 14D-06 公开 DLL 路由，下一任务转 14D-06 |
| 2026-08-06 | v3.39 | 完成 14D-06-R1：能力 DTO 升至 v1.4，`options.backend` 缺省且唯一为 `worker`；新增 carrier 路由器，冻结 full preflight/repair/slice 的 Worker 映射和 fail-closed 负例，R2 异步 job 接线准备门 PASS |
| 2026-08-06 | v3.40 | 完成 14D-06-R2：公共 SPI 以异步 `WorkerJobService` 唯一路由 full preflight、repair 与 slice；生命周期、协作取消、结果身份、私有作业目录和修复发布闭合，Debug/Release 定向门禁及合同校验通过；下一任务转 14D-05-R4-B |
| 2026-08-06 | v3.41 | 完成 14D-05-R4-B：公开 DLL -> Worker -> Writer 取消链路在 Debug/Release 下保持既有有效包、清理 owned 临时产物并返回稳定取消码；C-SPI-09 真实链路闭合，14D-05 COMPLETE，下一任务转 14D-04B |
| 2026-08-06 | v3.42 | 完成 14D-04B：公开 C SPI 在真实 Worker 活动阶段验证取消 ≤2000ms、非终态结果拒绝、重复/终态取消幂等和零 owned 临时产物；原 14D-04 COMPLETE，下一任务转 14C-06B |
| 2026-08-06 | v3.43 | 完成 14C-06B：公开 C ABI 在真实 Worker 链路关闭 C-SPI-08/09/13/15，Debug/Release Stage 14C 各 11/11 并连续 3 次 PASS；14C-06 全绿与 14D-05 共同满足 M-MVP-CANDIDATE，下一任务转 14D-07-R2 准备复审 |
| 2026-08-07 | v3.44 | 完成 14D-07-R2：当前 Worker 经公开进程/文件合同通过 E-01..08 Debug/Release Gate；下一任务转 14D-08 父任务收口审计，14E 保持未启动 |
| 2026-08-07 | v3.45 | 完成 14D-08-R4：三项真实 executor、共享 runtime、安全发布/取消/唯一路由、Debug/Release、RIP strict、无 fallback 与 VS Code 独立调试入口全部闭合；Stage 14D 父任务收口，下一步只执行 14E 前最终门禁 |
| 2026-08-07 | v3.46 | 完成 14E-01：新增纯 C 控制台宿主，仅经 11 个公开导出完成导入、变换、Worker 切片、Package 校验与 fail-closed；Debug/Release 2/2 PASS，M-MVP 成立并解锁 14E-02 |
| 2026-08-07 | v3.47 | 完成 14E-02：新增独立 Qt 参考宿主与 runtime-loaded `ModuleClient`，冻结 11 导出解析、15 能力自述、统一作业入口和 ABI 调用计数；Debug/Release 3/3 PASS，下一任务为 14E-03 |
| 2026-08-07 | v3.48 | 完成 14E-03：落地 Transient/Commit/Stale 三车道控制器；UI-M1 零跨 DLL 调用、正常 Commit 无追加快照和 UI-M4 Stale 回滚经 Debug/Release 真实模块验证，下一任务为 14E-04 |
| 2026-08-07 | v3.49 | 完成 14E-04：top ViewData 真实纹理、preview/layout 双层缓存和本地移动零调用闭合；Commit P95=0.3819 ms，Release 30 秒 UI-M3 为主干 133.303%，下一任务为 14E-04b |
| 2026-08-07 | v3.50 | 完成 14E-04b：Qt 参考宿主经公开 SPI 覆盖 15 项能力，P0/P1 全部端到端通过、P2 返回完整记录；UI-M5 取消 103 ms 无残留，UI-M6 保持 fail-closed，下一任务为 14E-04c |
| 2026-08-07 | v3.51 | 完成 14E-04c：真实 three_d mesh/UV/submesh/material/texture 经后端中立接口闭合，七向相机与构建体积显示全本地运行；UI-M7=0，10 万三角面 30 秒 UI-M8 P5=51.4168 FPS，下一任务为 14E-04d |
| 2026-08-07 | v3.52 | 完成 14E-04d：新增中央俯视/3D 双入口、默认视图与投影 session config 持久化、buildVolume 1 mm/10 mm 自适应网格和白纹理对比辅助；Debug/Release 9/9 PASS，UI-M9..13 闭合，下一任务为 14E-05 |
| 2026-08-07 | v3.53 | 完成 14E-05：按职责拆分主干 `MainWindow` 与 `UiSmokeTestRunner`，分别降至 1218/642 行；新增实现文件均低于 500 行，Debug/Release 自测与代表性 UI Smoke PASS，并移除 14B-06 临时白名单；下一任务为 14E-06 |
| 2026-08-07 | v3.54 | 完成 14E-06 切片侧交付：文件级清单覆盖 46 个宿主源/构建文件，42 个可直接复制、4 个需改写；机器清单同步公开合同、运行时交付物和内部源码禁入边界，自动完整性验证 PASS；打印侧 ACK 仍待外部确认，下一阶段转 14F-01 准备审计 |
| 2026-08-07 | v3.55 | 完成 14E-07：VSCode 新增与原 UI 明确区分的封装宿主 Debug/Release 编译、运行、自检和调试入口；原 UI 构建/部署/启动与新宿主 SPI 自检均完成实测，14F-01 已获用户授权 |
| 2026-08-07 | v3.56 | 完成 14F-01 切片侧打包：生成独立 `modules/slicer/` Release 能力包，递归闭合 PE/MSVC Runtime 依赖并携带 NOTICE、许可证、运行时 inventory 与 SHA-256；包内 Worker 合同和纯 C 宿主 import→slice→verify 本地隔离闭环 PASS，真实干净机与打印侧 M1 证据仍待外部执行 |
| 2026-08-07 | v3.57 | 完成 14F-02 切片侧联调准备：新增只执行装载、15 项能力和 `pm_self_test` 的 M1 探针，生成能力包+公开合同+手册的自包含 handoff 并验证哈希与缺 DLL 负例；打印侧 ModuleRegistry 和进程模块清单 ACK 仍为外部依赖，14F-02 不标 COMPLETE |
| 2026-08-07 | v3.58 | 用户授权暂缓打印侧验证并按可实现性成立继续推进；新增 14F 外部验证延期与接口冻结决策，明确切片侧本地 S1/S2 门禁可继续、外部证据不得伪记 PASS；下一任务为 14F-03 |
| 2026-08-07 | v3.59 | 完成 14F-03 切片侧本地联调：公开 ABI 单模型链路生成真实 RGBWSV 包，S1 正例和 7 类负例全部通过；打印侧 M2 继续标记 EXTERNAL VALIDATION DEFERRED，下一任务为 14F-04 |
