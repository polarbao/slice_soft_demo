# TASKS_FRAME 非打印定位素材与输出画幅专项任务清单

> 版本：v1.4 ｜ 日期：2026-09-07
> 状态：COMPLETE（本地软件范围；GUI 人工交互与物理打印未验证）
> 授权：用户确认定位素材不实际打印、后续统一命名，并授权方案及实施；可按需并行。
> 技术合同：`docs/slice/DOC/DOC_DESIGN_FRAME_非打印定位素材与输出画幅分离.md`
> 命名权威：`docs/slice/DOC/DOC_SPEC_MATERIAL_NAMING_多图层素材命名与语义标识规范.md` §1.3。

## 1. 专项目标

精确材质名 `nail-Default` 表示仅用于输出 XY 画幅定位的非打印几何：随主体变换且保留画幅，不生成 RGBWSV/T、支撑或额外打印层。不自动忽略普通 `Default`，不改用户模型，不放宽真实主体命名/拓扑准入，不改变生产协议。

专项独立于 MATOPQ/MATVOL/MEMFLOW 已有阶段编号；本卡为 FRAME 唯一状态真源。完成设计不等于代码/生产包/真机验证完成。

## 2. 准备 Gate

| 准备项 | 结论 | 证据/边界 |
|---|---|---|
| 用户用途与命名授权 | PASS | 本轮明确定位块不打印并统一沿用 `nail-Default` |
| 风险与责任层 | PASS | 设计 §3..5；core 负责用途分类，Qt 不生成几何 |
| 兼容合同 | PASS | 普通材质不变；精确匹配；主体 strict、S0/MATVOL、RGBWSV 保持 |
| 测试资产 | AVAILABLE | `model/obj/multi-material/gubao05`；用户 OBJ/MTL 修改不覆盖 |
| 验证矩阵 | PREPARED → EXECUTED | 设计 §6；实施与实际验证见 §3/§5/§6，外部物理打印不计本地 Gate |

开工结论为 **FRAME PREPARED / IMPLEMENTATION GO**；完成状态见下表。本轮只实现本专项，不自动推进其他专项。

## 3. 原子任务

| 任务 | 内容 | 前置 | 状态 | 完成日期 | 实际验证 |
|---|---|---|---|---|---|
| FRAME-00 | 需求与保留名、技术边界、测试矩阵和独立清单 | 用户授权 | COMPLETE | 2026-09-07 | 文档与当前模型/变换/命名代码审计；无生产 PASS 声明 |
| FRAME-01 | 核心定位几何分类、材质绑定与主体隔离、定位-only 处理 | FRAME-00 | COMPLETE | 2026-09-07 | Release 构建及 frame_geometry PASS，含真实 gubao05 分离；源统计/打印计数口径保持 |
| FRAME-02 | 变换/定向/触底和单模型/Scene 输出 XY 范围贯通 | FRAME-01 | COMPLETE | 2026-09-07 | frame_geometry / frame_scene PASS；两实例 1570×1100、127 DPI、12 层，全层空区及 RIP strict PASS |
| FRAME-03 | 材料/支撑/空通道、准入兼容与定向回归；附属材质 XY 并集空列剔除 | FRAME-01/02 | COMPLETE | 2026-09-07 | frame_production PASS：旧全实体 RGB+按需补白/多图层工艺同画幅对照逐层零差异、T 空 Mask、材质闭区间边界；基础 10 项为 9 PASS + 1 既有失败见 §6 |
| FRAME-04 | gubao05 多图层透明全链路、部署与证据收口 | FRAME-03 | COMPLETE | 2026-09-07 | gubao05 600 DPI / 0.033 mm 真实 148 层包及独立 RIP strict PASS；切片写包报告 82.3605 s；定位独占区 1,063,460,216 pixel-layer 全空；Release 部署/四文件哈希/宿主自检 PASS。GUI 人工交互与物理打印未执行 |

## 4. 执行所有权与并行

- 主执行者负责核心实现、变换/场景/材料调用链、整体验证及结果整合。
- 文档 Agent 只写本清单、技术方案、命名规范，不改代码、OBJ/MTL，不提交。
- 无共享文件的只读审计/测试辅助可并行；核心 DTO、CMake 和生产路由由主执行者协调，避免并发覆盖。
- 用户于 2026-09-07 授权将本专项修改按任务拆分提交；仅提交本专项代码、测试和文档，不推送。用户现有模型修改与其他工作树内容保持原状。

## 5. 验证清单

- [x] 精确 `nail-Default` 被识别，普通 `Default`/大小写近似名不被识别。
- [x] 主体三角面和纹理绑定一一对应，定位-only 作业显式拒绝。
- [x] 源顶点/面/UV 统计保留，打印三角数与 frame_triangle_count 分开记录；材料表只含打印材质。
- [x] 单模型/Scene 输出画幅包含实际定位顶点的变换后 XY 范围。
- [x] 定位极高/极低 Z 不改变主体触底、打印高度和层数。
- [x] 无定位对照/材料拓扑及命名回归通过；保留 §6 场景适配器既有失败，不宣称全仓回归全绿。
- [x] 含定位几何的 CLI repair 导出显式拒绝且不创建 OBJ；生产 facade 同样加保护，未另行宣称所有 repair 路径本轮均测试。
- [x] 定位独占区域每层 RGBWSV 空值 255，T 空 Mask 单测通过，无定位支撑。
- [x] 材质 XY bbox 保守剔除只读等价性审查、边界 owner fixture 与工艺同画幅逐层输出对照通过；不更改 hits/priority/容差。
- [x] gubao05 多图层透明核心等价工艺完整切片与包严格验证通过，主体颜色与 V/必要 W 输出有数据。
- [x] 记录源哈希、Profile、姿态、DPI/层厚、包目录、画幅、层数与耗时。
- [x] Release 编译成功后执行对应二进制 CTest，未使用陈旧 build 目录。
- [x] 运行时部署与哈希/自检明确记录；未关闭用户进程。
- [x] `git diff --check`，任务状态列、日期与实际结果更新。

## 6. 验证记录

当前实施、真实资产分离、合成与真实生产包、运行时部署均已完成；下列实测由主执行者在本轮提供，文档 Agent 对源哈希、生成 Profile 和 CTest 日志作只读复核。GUI 人工点击验证与物理打印 NOT RUN，未提交或推送。

2026-09-07 开发过程记录：初次真实切片暴露定位空背景逐列扫描全部材质三角面的成本，随后 FRAME-03 增加 O(材质数) XY 并集粗剔除。旧尝试被人工终止，没有完整基线；最终仅报告优化后完成值，不计算加速倍数。

| 验证 | 实际结果 | 口径 |
|---|---|---|
| `frame_geometry_unit_tests --real` | PASS | gubao05 打印 126,134 三角，定位 36 三角 / 72 顶点；autoOrient=false 时 bbox `(-110,-17.9051,7.2983)..(110,42.0949,12.1694)`，XY 220 × 60 mm |
| 优化前 600 DPI / 0.033 mm 完整包尝试 | INTERRUPTED_BY_OPERATOR | 2026-09-07 15:19:17 启动，约 4 分 20 秒尚无包，主执行者终止本次测试 PID 36820；CPU 257.94 s，退出 1。不是已完成性能基线，不计作工艺切片失败，也不能由此计算加速比 |
| 含定位素材 repair 导出保护 | CLI NEGATIVE PASS | 生产 RepairFacade 与 CLI repair 入口增加 `E_FRAME_REPAIR_UNSUPPORTED`，禁止静默丢框；CLI 真实含定位素材拒绝且指定 OBJ 未创建；原普通模型 repair 不放宽 |
| 材质 XY 并集粗剔除 | BUILD AND GATES PASS | Release 构建与 frame_production PASS；闭区间边界及同画幅对照通过；真实 600 DPI 包完成 |
| `frame_geometry` / `frame_production` / `frame_scene` | PASS | Release；production 9.54 s，scene 3.63 s。Production 含旧全实体 RGB+按需补白与多图层透明工艺逐层同画幅对照零差异、T 空 Mask、材质边界。Scene 两实例 1570×1100 / 127 DPI / 12 层，全 TIFF 空区检查及 RIP strict 通过 |
| 最终 consolidated Release CTest | 12 PASS + 1 既有 FAIL | 13 个不同目标、14.14 s、退出 1：新 FRAME 3 PASS，基础 9 PASS + 1 既有 FAIL；未重复计算 frame_geometry。测试名与复跑命令见下 |
| `scene_layer_adapters` | 1 个既有子用例 FAIL | 精确失败 `translation preserves local layer bytes and dimensions`，与 `TASKS_PRESET_工艺可配置面收敛与自测用例收口专项任务清单.md` §13.2 既有失败逐字一致；本轮未修、未计为 FRAME PASS |
| 优化后 gubao05 600 DPI / 0.033 mm | PASS / EXIT 0 | 5197×1418、148 层，切片/写包/报告 82.3605 s，不含初始导入及后续独立严格 Reader；后者另行 PASS |
| 真实材料与空白像素 | PASS | RGB=1,073,243，W=126，S=13,063,912，V=1,066,387；定位独占区 1,063,460,216 pixel-layer 全通道 255；material_volume_report `errors=[]` / `unownedModelPixels=0` |
| 运行时部署与自检 | PASS | `slicesoft_runtime` 构建 exit 0；DeployOnly 到 `runtime/slicesoft/Release` exit 0。目录锁导致脚本原有 in-place immutable 更新，output 保留，未关闭用户进程。CLI/Worker/Module/Host 四文件构建与部署 SHA256 一致；宿主 `--self-test` exit 0：`STAGE14E02_SELF_TEST_PASS spi=1 calls=6` |
| 源码尺寸守卫 | PASS WITH EXISTING WARNINGS | 61 项既有 warnings，两个巨型源文件净行数 0；未把既有 warning 描述为本轮新增失败 |

### 6.1 真实资产与配置

- 原始 OBJ SHA256：`65FB5CEECB19545FE2A56FCBF71568CB8D82E80E5AA9D33BD4CC6E92FC67DB55`。
- 原始 MTL SHA256：`987B5D1D60175EC056E7EE38ED3A279C548618BB535216F15DEB089F3F7A7D2B`。源文件沿用用户现有命名，未由本专项改写。
- 有效配置：`output/frame/17887661253394842/profile.json`；文件 SHA256 `45CDA46E1B8E6B08EFE0E3168F79E65AED8FA7EB246C91B2589796D39F837ECF`，内嵌 Profile hash `sha256:f6cc7b1f0835df8fc1c450f0042d71204377b3aa96f72dda4b0b30eb484365eb`。
- 该配置由宿主 `HostBuildEffectiveProfile` 构造，使用“多图层透明”相同核心材料设置：MATVOL / auto_by_material_name / opacityVarnish / 各材质自身贴图 / white_underbase / 下表面支撑。测试锁定 S0、600×600 DPI、0.033 mm、autoOrient=true、maxHeight=9、baseProjection=false、preview=false、TIFF compression=none；不是人工在 GUI 中点击工艺的交互验证，也不包含额外 30 层铺底。
- 最终姿态 `identity_rotate_y_180_rotate_z_180`，旋转 `[0,180,180]`；bbox `(-110,-42.0949020385742,0)..(110,17.9050979614258,4.87107515335083)`，定位 XY 220×60 mm，Z 仅主体。
- 完整包：`output/frame/17887661253394842/package`；schema `p0.rgbwsv.2`；148 个 TIFF 共 `6,544,041,852` bytes。大空白画幅保持了用户要求的尺寸，也实际增加磁盘用量；未改变默认压缩策略来掩盖该成本。

真实复跑命令：

```powershell
build-slicesoft/main/Release/frame_geometry_unit_tests.exe --real
build-slicesoft/main/Release/frame_production_tests.exe --real 600 0.033
```

基础 9 个 PASS 目标已由 `build-slicesoft/main/Testing/Temporary/LastTest.log` 复核：`texture_white_carrier_policy_unit_tests`、`matvol_topology_unit_tests`、`matvol_interval_unit_tests`、`matvol_rgb_compose_unit_tests`、`model_transform_unit_tests`、`material_opacity_varnish_unit_tests`、`material_layer_naming_unit_tests`、`auto_orient_unit_tests`、`multi_model_production_service_unit_tests`。`LastTestsFailed.log` 对应 `scene_layer_adapters_unit_tests`，未隐藏该红灯。

针对 FRAME 与上述兼容面可使用以下复跑命令（单元与生产构建成功后执行，最后一个目标预期仍暴露已知失败）：

```powershell
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R '^(frame_.*|texture_white_carrier_policy_unit_tests|matvol_topology_unit_tests|matvol_interval_unit_tests|matvol_rgb_compose_unit_tests|model_transform_unit_tests|material_opacity_varnish_unit_tests|material_layer_naming_unit_tests|auto_orient_unit_tests|multi_model_production_service_unit_tests|scene_layer_adapters_unit_tests)$'
```

### 6.1 导入姿态变化复核（2026-09-07）

用户反馈 gubao05 默认导入不再保留原倒钩姿态。本轮使用已构建 Release CLI 的
`--inspect-model` 做三组对照，均退出 0；不执行切片、不改用户资产，不更改生产默认开关：

| 场景 | 自动定向 | 选择结果 / XYZ 角度 |
|---|---|---|
| 当前精确定位材质 | 开 | `identity_rotate_y_180_rotate_z_180` / `[0,180,180]` |
| 当前精确定位材质 | 关 | `identity` / `[0,0,0]`，保留源姿态与源 XY |
| 临时副本恢复定位块为普通 Default，模拟旧定向参考 | 开 | `identity_rotate_z_minus_90` / `[0,0,-90]` |

第三组是当前算法下恢复旧输入语义的对照，**不是运行旧版二进制**。临时配置与对照资产位于
`output/frame_orientation_audit/40ddbd0526814a8e9ed0dec0a64f4338`。
源 OBJ/MTL SHA256 与 §6 原始证据一致，未改变其顶点坐标。

根因：FRAME 在 `choose_auto_orientation` 前分离定位面，算法从 220×60 mm、
高度 14.3267 mm 的含框参考，改为约 13.3926×24.3641 mm、高度 4.87108 mm 的主体参考；
既有 `AlignNailFrontUp` 与 `AlignPlanarHeading` 由此选择不同翻转。该变化无需调用 UI 排版即可复现。
UI 自动排版是另一项仍默认开启的落位操作，不是本次核心翻面所必需的条件。

处理结论：保留现有显式开关合同，不静默屏蔽自动定向；源姿态需求通过导入前取消自动定向实现。
保留源 XY 时另取消自动排版、批次原点归零。宿主 `landOnBuildPlate=true` 仍沿 Z 触底。
规则规范 §1.3 与使用手册 §4.2.1 已同步该边界及材质导出示例。
本轮仅文档变更及导入诊断，未新运行 GUI 人工交互、切片或物理打印测试。

### 6.2 按任务拆分提交（2026-09-07）

| 拆分项 | 提交 | 范围 |
|---|---|---|
| 定位几何与回归测试 | `e7628f7` | FRAME-01/02 主体隔离、变换/画幅贯通，FRAME-03 单元/生产/Scene 测试及必要 CMake/fixture 配置 |
| 修复导出保护 | `543f236` | 含定位几何的 CLI/生产修复导出显式拒绝，防止静默丢框 |
| 空白区域性能优化 | `765aeb8` | FRAME-03 材质 XY 并集范围剔除，不变更既有求交语义 |
| 规则文档收口 | 本文所在的独立 docs(frame) 提交 | FRAME-00/04 设计、任务证据、命名规范、使用手册及 AGENTS 入口 |

提交前对最终组合工作树复跑 Release FRAME 测试，3/3 PASS，合计 12.58 s；
`ValidateSourceSizeGuard.py --base-ref HEAD` 在首个拆分提交前 PASS（61 项既有告警），
各提交执行暂存差异检查。未声称对中间提交分别重建或执行全量回归。
相关 13 项回归的 1 项既有失败仍按 §6 记录，不因本次 3 项复跑改记为全绿。

本次未提交用户原有的 `gubao05` OBJ/MTL 命名修改、`model/obj/multi-material/xx04/`
及 `analysis/`，也未纳入生成的真实包或临时诊断副本。文中早期“未提交”描述为各次验证时的历史状态；
当前 Git 交付按本节记录，未推送远端。

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-07 | v1.4 | 按用户授权拆分定位功能、修复导出保护、空列优化及文档提交；记录提交映射、3/3 定向回归复跑和不纳入范围，保留用户资产修改及既有失败证据。 |
| 2026-09-07 | v1.3 | 同步规则示例与使用手册；三组真实导入诊断确认姿态变化来自定位隔离后自动定向参考改变，记录源姿态/XY 保留方式，生产代码和默认开关不变。 |
| 2026-09-07 | v1.0 | 新建 FRAME-00..04，完成准备并授权开工；冻结精确保留名、XY 扩框但无打印/Z 贡献、兼容及验证边界。 |
| 2026-09-07 | v1.0 补充 | FRAME-03 纳入材质 XY 并集空列剔除，只读审查确认在保留既有单三角闭区间比较前提下等价；实际性能与生产证据仍待完成，不预记提升。 |
| 2026-09-07 | v1.1 | FRAME-01..03 收口：Release 构建、geometry/production/scene、真实资产分离、T 空区及 CLI repair 负例通过；保留已知 scene_layer_adapters 子用例失败。FRAME-04 正在运行真实 600 DPI 切片，未预记完成。 |
| 2026-09-07 | v1.2 | FRAME-04 收口：gubao05 600 DPI/0.033 mm、148 层真实包及定位空区/RIP strict 通过，记录 82.3605 s、实际通道/哈希/姿态/6.54 GB TIFF；Release 部署/四文件哈希/宿主自检通过。最终 CTest 12/13，保留 1 既有失败；未验证 GUI 人工交互或物理打印，未提交。 |
