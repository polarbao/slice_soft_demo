# TASKS_14 切片能力包封装与打印软件集成任务清单

> 文档状态：✅ **ACTIVE**（用户于 2026-08-04 授权激活）
> 版本：v2.30 ｜ 日期：2026-08-03 ｜ 激活：2026-08-04 ｜ 14A 实现收口：2026-08-05
> 作者：Claude 起草；执行由主线开发（codex）接管
> 决策依据：`docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md`
> **S2 权威条款：`docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md`（实施只看该文）**
> 详细设计：`docs/claude/INTEGRATION/INT_06..15`
> 打印侧对接：`ry_print_demo/docs/claude/INTEGRATION/CLD_04,05,06,10,27` + `PLANNING/CLD_07`

---

## 0.0 开工须知（2026-08-04 激活时新增）

```text
① 首批可并行开工：14A-01、14A-02、14A-07、14A-09、14B-06、14B-00（互不依赖）
② 14A-08 已 COMPLETE —— RIP 六问两轮闭合，不要重新发问卷
③ 新增 14A-10（manifest whiteSemantics），依赖 14A-02
④ Stage 15 已 COMPLETE，其新增的 texture.unprintableWhite* 三字段与
   whiteSemantics 是【不同层级】的东西：前者是像素级写入策略，后者是作业级语义声明。
   14A-02 的 Schema 必须同时覆盖两者，不要混为一谈。
```

**⛔ 禁止实现（路径 A 配套，已随路径 D 定案作废）**

```text
✗ Writer 断言：写出前扫描 W==0 && S==0 && V==0 的哨兵检查
✗ manifest ripBoundIntermediate { whiteRegionSentinel: "WSV=000", ... }
✗ 路径 A / B / C / E 的任何形式
✗ 逐层 1-bit sidecar
✗ p0.rgbwsv.3 协议扩展
完整清单：DOC_DECISION_14_S2 §4。若在旧文档中见到上述内容，以合同定案为准。

⚠️ 不要误删：错误码 PM-SLICER-CONTRACT-0060 本身【有效】，它是 SPI 既有通用错误码
   （「自检发现产物不符合 p0.rgbwsv.2」，见 INT_07 / INT_02 错误码表）。
   作废的只是「用它承载 WSV=000 哨兵扫描」这一用途，错误码与错误码表整体保留。
```

---

## 0. 使用规则

```text
一次只执行一个明确指定的原子任务；
任务状态推进：PREPARED → READY → IN PROGRESS → COMPLETE，不得跳级；
每个任务完成必须记录：实际命令、build type、结果、剩余风险；
未运行的验证不得写成 PASS；
同一文件同一时间只能有一个任务 Owner。
```

**状态取值全集（2026-08-06 补充登记）** —— 执行中演化出的限定态已在使用，此处正式收录，
避免"规则四级、实践八级"长期打架：

| 取值 | 含义 | 可否作为下游卡的前置 |
|---|---|:--:|
| `PREPARED` | 已成文，未开工 | ❌ |
| `READY` | 前置已清，可立即开工 | ❌ |
| `IN PROGRESS` | 执行中 | ❌ |
| `COMPLETE` | 完成且验收证据齐备 | ✅ |
| **`PREPARATION_GATE PASS`** | 准备门通过（设计/Schema/负例已冻结），**实现未完成** | ❌ **不得当作 COMPLETE** |
| **`PREPARATION_GATE PASS_WITH_SPLIT`** | 同上，且该卡已受控拆为子卡；**以子卡状态为准** | ❌ |
| **`SLICER-SIDE COMPLETE / PRINT-SIDE ACK PENDING`** | 我方交付完成，等外部书面回签 | ✅ **对切片侧下游放行**；❌ 对 14F 联调不放行 |
| **`外部依赖`** | 阻塞源在仓库外，非本仓库可推进 | ❌ |
| `NEW` | ⚠️ **不是状态**，是"本轮新增卡"的标注。新增卡的状态应另写 `PREPARED` | ❌ |

> ⚠️ **`PREPARATION_GATE PASS` ≠ `COMPLETE`**。14C-05 与 14D-04A 已完成实现；
> 14D-04 总卡仍须等待 04B，14D-08 准备门仍为阻断态，不得提前放行 Worker E2E。

**统一出口门（所有涉及生产路径的任务）**：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
.\scripts\run_material_closure_tests.ps1 -Mode RepairDisabled   # 30 层 TIFF SHA-256 不变
```

---

## 1. 14A 契约冻结（门禁：未完成不得进入 14B）

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14A-01 | 建立 `contracts/` 目录；落盘 `print_module_spi.h`（与打印侧 `CLD_10` 同源，我方只同步不改）+ **错误码表登记 `PM-SLICER-VIEWDATA-STALE` / `PM-SLICER-VIEWDATA-BUDGET`** | — | C 与 C++ 编译器分别编过；合同声明恰好 11 个 `pm_*`。实际 DLL 的 `dumpbin /EXPORTS` 延后到 14C-01 / 14C-06 验证（**新增的是错误码不是导出符号，符号数不变**）| ✅ **COMPLETE（2026-08-05）** |
| 14A-02 | `p0.rgbwsv.2` + scene + Profile 扩展 schema 形式化 JSON Schema。**须覆盖三组新字段**（见注 A）；ViewData DTO 由 14A-04 独立冻结 | — | 用真实 manifest、真实 scene 与 Stage 15 Profile 样例分别校验通过；无该字段的既有样例仍可校验（向后兼容）| ✅ **COMPLETE（2026-08-05）** |
| 14A-03 | `file_contract_v1` 完整规格（请求/结果 JSON schema、进度行、退出码表、超时、僵尸回收、staging 清理时序）| 14A-01 | 打印侧确认可满足 | 🟡 **SLICER-SIDE COMPLETE / PRINT-SIDE ACK PENDING（2026-08-05）** |
| 14A-04 | 能力 DTO 字段级规格（15 项能力的请求/响应字段与类型）| 14A-01 | 打印侧据此可编码；**`scene.get_viewdata` 网格 DTO 按注 B 纳入契约** | ✅ **COMPLETE（2026-08-05）** |
| **14A-04-R1** | **受控修订已冻结的 ViewData 合同**：增加 `top` / `three_d`、`surfacePreview`、`texcoord0`、submesh/material/texture、外观 identity 与纹理 fail-closed；保持 `PM_SPI_VERSION=1`、11 个导出、15 项能力不变 | 14A-04；用户已授权修改冻结文件 | 合同测试通过；打印侧可按 DTO 1.2 编码；双视图带纹理硬标准无灰模逃生口 | 🟡 **SLICER-SIDE COMPLETE / PRINT-SIDE ACK PENDING（2026-08-05）** |
| 14A-05 | 三车道交互契约固化（`operationId` 幂等、`expectedSceneRevision`、`SceneRevisionStale` 回滚）| 14A-04 | 与打印侧 `CLD_04` §4.3 一致 | ✅ **COMPLETE（2026-08-05）** |
| 14A-06 | 取消语义写入契约（`Cancelling ≠ Cancelled`、≤2s、staging 清理）| 14A-01 | 与 13F-R0-03 实现一致 | ✅ **COMPLETE（2026-08-05）** |
| 14A-07 | 第三方依赖再分发合规审查（assimp / miniz / libtiff 许可证 + NOTICE）| — | 成文，可随包分发 | ✅ **COMPLETE（2026-08-05）** |
| 14A-08 | **对 RIP 统一确认清单发出并回签** | — | RIP 侧按模板回填并回传（见注 C）| ✅ **COMPLETE（2026-08-04，两轮均已闭合）** |
| 14A-09 | `REPORT_12X` 补 03E 行（03E-02 现为 **`GO_ON_DEMAND`**，见 `REPORT_03E_02` §5.1）| — | 主状态表完整 | ✅ **COMPLETE（2026-08-05）** |
| **14A-10** | **manifest 新增 `whiteSemantics`（`opaque` \| `transparent`）**：manifest 为权威、Profile 仅提供默认值；两处不一致时 **fail-closed**（见注 D）| 14A-02 | Schema 覆盖新字段；不一致用例 fail-closed；无该字段的既有包仍可读 | ✅ **COMPLETE（2026-08-05）** |
| **14A-11** | **`SceneBuildVolume` 新增 Z 限高 `zLimitMm`**（`std::optional<double>`）+ scene schema 同步 + Z 超限判定；默认设备幅面 **230 × 100 × 60 mm**（见注 E）| 14A-02 | 缺省时行为与现状**逐字节一致**（既有场景与 golden 零影响）；`zLimitMm` 存在时实例世界 bbox `max.z` 超限产出**告警**；`get_snapshot` / `apply_operation` 响应带该字段 | ✅ **COMPLETE（2026-08-05）** |

**14A 出口**：`contracts/` 物料齐备（`print_module_spi.h` + 错误码表 + `file_contract_v1` + 能力 DTO 1.2 含双视图纹理 ViewData + JSON Schema 含三组新字段）；打印侧书面确认；RIP 侧已回签（14A-08 COMPLETE）；`OPEN-14-03/04/05` 已关闭。

**14A-03 本地证据（2026-08-05）**：`contracts/file_contract_v1.md`、请求/结果/
协商三份 Schema、退出码表及自动合同测试已落盘。切片侧内容已完成，状态不写 COMPLETE
仅因为验收项要求打印侧书面确认；该外部回签不阻塞 14A-04/05/06 的切片侧工作。

**14A-04 本地证据（2026-08-05）**：`contracts/slicer_capability_dtos.json` 以字段路径、
JSON 类型、必填条件、承载和错误码冻结 15 项能力；`scene.get_viewdata` 完整纳入 local mesh、
`worldMatrix`、LOD、`meshIdentity`、blob 分块和 `read_blob` 子操作。合同测试同时门禁能力数量、
生产 RGBWSV 不变量、Worker 边界及“不得新增第 16 项能力/第 12 个导出符号”。

**14A-04-R1 本地证据（2026-08-05）**：用户明确授权在 14A 开发已启动后受控修改冻结文件。
`slicer_capability_dtos` 从 1.1 升至 1.2，`slicer_three_lane_contract` 从 1.0 升至 1.1；
补齐 top 带纹理投影、three_d UV/材质/纹理、`appearances[]` 多模型引用、四类缓存 identity、
`contracts/slicer_ui_view_spec.json` 双视图网格和纹理失败显式错误，
同时修正 Transient 碰撞权威性与正常 Commit 多余 `get_snapshot`。未新增 ABI 导出或能力。

**14A-05 本地证据（2026-08-05）**：`contracts/slicer_three_lane_contract.json/.md`
冻结 Transient 零跨模块调用、Commit 原子幂等、Stale 回读回滚与 Production 只接受已提交
`sceneHash`。同 ID 同 payload 只重放首次结果；同 ID 改 payload fail-closed；Worker full preflight
保持生产权威，合同测试与 15 项 DTO 交叉校验字段一致性。

**14A-06 本地证据（2026-08-05）**：`contracts/slicer_cancel_contract.json/.md`
冻结 `pm_cancel` 幂等、`Cancelling` 非终结、Worker 真实退出与 staging 清理后才能进入
`Cancelled`、2000ms 宽限和 Job Object 兜底。取消结果 Schema 强制
`stagingRemoved=true/published=false`；清理失败必须报告 failed，禁止伪报 cancelled。

**14A-07 本地证据（2026-08-05）**：根 NOTICE、assimp/miniz/LibTIFF 完整许可证、
机器可读分发清单与合规审查已落盘。审查区分“vcpkg 声明”与“CMake 实际链接”：miniz 始终静态
编入，LibTIFF 按后端可选，Assimp 当前未链接；自动门禁确保许可证和发布动作不被删漏。

**14A-10 本地证据（2026-08-05）**：Profile 根字段提供作业级默认值，`output.whiteSemantics`
作为 manifest 显式权威值；两者冲突或枚举非法时配置与 Writer 均 fail-closed。Legacy、
Global Surface Shell 与多模型场景写包链路统一传播该字段；严格 Reader 校验可选枚举，旧包缺字段
仍保持兼容。Debug/Release 配置、Writer、Schema 与 RIP Reader 定向验证均通过。

**14A-11 本地证据（2026-08-05）**：`SceneBuildVolume::zlimitmm` 已成为可选字段；显式
`MakeDefaultDeviceBuildVolume()` 固化 230 × 100 × 60 mm 设备幅面，而旧 scene 缺字段时不补写，
canonical JSON 与 scene hash 保持兼容。实例世界 bbox `max.z` 超限只产生告警，不阻断生产准入；
`scene.get_snapshot` / `scene.apply_operation` DTO 均携带构建体积，后者额外携带 warnings。
Debug/Release scene、collision、Schema 与 DTO 定向验证均通过。

---

#### 注 A · 14A-02 须覆盖的三组新字段

```text
① texture.unprintableWhitePolicy / unprintableWhiteInkThreshold / unprintableWhiteValue
                                                       （Stage 15 已落地，schema 需补齐）
② manifest.whiteSemantics                              （14A-10）
③ buildVolume.zLimitMm                                 （14A-11，scene schema）
```

> ⚠️ 14A-02 是 14A-10 与 14A-11 的共同前置，务必一次把四组字段都纳入，
> 不要分批 —— 分批会让 schema 反复变更，下游校验器跟着反复改。

**14A-02 实际交付（2026-08-05）**：

```text
contracts/p0.rgbwsv.2.schema.json
contracts/slicesoft.multimodel_scene.13b.1.schema.json
contracts/slicesoft.slice_profile.schema.json
tests/contracts/ValidateJsonSchemas.py
```

Profile 三字段归属于切片配置而非 manifest/scene，因此使用独立 Profile 扩展 Schema，
避免把不同所有权层级的字段塞进同一合同。`whiteSemantics` 与 `zLimitMm` 本卡只冻结为
可选字段；业务写入、冲突判定与限高求值仍分别由 14A-10、14A-11 实现。

#### 注 B · `scene.get_viewdata` 双视图纹理 DTO —— v1.2 已受控修订

✅ **`docs/slice/DOC/DOC_SCHEMA_14_SceneViewData网格DTO规格.md`**
14A-04 的几何合同已冻结；14A-04-R1 在保持 ABI 外壳不变的前提下补齐纹理：

```text
① 视图规格   top=+Z 正交带纹理 surfacePreview；three_d=可交互带纹理网格
② 网格缓冲   float32x3 顶点/法线、float32x2 UV、uint16|uint32 索引；mm；little_endian；右手 Z-up
③ 外观数据   submesh + material + texture blob；白/近白纹理不得被 UI 背景吞没
④ LOD 分级   lod0/lod1/lod2/outline_only/auto；【模块决定实际 LOD，宿主给 maxBytes 预算】
⑤ 实例变换   local + worldMatrix（推荐默认）—— 多实例只传一份网格；
             且【worldMatrix 变化不使网格失效】，这是 UI-M1/M7 零跨 DLL 调用的前提
⑥ 传输策略   blob 分块，经【既有】pm_submit/pm_result 通道取回，
             使用 scene.get_viewdata 的 read_blob 子操作，不新增第 16 项能力或导出符号
⑦ 缓存失效   mesh/appearance/texture/preview identity 分离；失效表见规格 §6
⑧ 失败策略   声明纹理但缺文件、解码失败、UV 或材质绑定无效时显式失败，禁止静默灰模
```

合同层可先于 Provider 完成，但 **14B-03A 必须在 14E-04c 前落地**。任何只实现
`bbox + outline` 或用灰模代替纹理的版本，都不能通过双视图 UI 验收。

#### 注 C · 14A-08 已闭合，不要重发问卷

```text
往来记录   docs/slice/DOC/DOC_CHECKLIST_14_对RIP侧技术确认清单.md（v1.4，已转档案）
权威条款   docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md ← 实施只看该文
```

⛔ **禁止实现**（路径 A 配套，已随路径 D 定案作废）：`WSV=000` 哨兵 Writer 断言、
manifest `ripBoundIntermediate` 字段。完整作废清单见 `DOC_DECISION_14_S2` §4。
⚠️ 但错误码 `PM-SLICER-CONTRACT-0060` 本身**有效**，不要误删（见 §0.0）。

#### 注 D · 14A-10 溯源

`DOC_DECISION_14_S2` §1.4。Q3.1 确认「同层不需混用两种白」，故白色语义为**作业级**声明
而非逐像素，不需要 `p0.rgbwsv.3`。这是本轮 RIP 问答产生的**唯一**切片侧新实现工作。

#### 注 E · 14A-11 是独立于 3D 视角的生产风险

`MultiModelScene.h:149` 的 `SceneBuildVolume` 注释明写 “Optional printable **XY** volume”，
只有 `widthmm`(X) / `heightmm`(**Y，不是 Z**)，**没有任何 Z 限高字段**。后果：
**模型超高无法判定 —— 切片会成功，但实物打不出来。**

**渲染与判定分开看（2026-08-04 用户澄清 + 我方补充）**：

```text
渲染用途   zLimitMm 只在【3D 视角】画构建体积盒子；【俯视视角忽略 Z】—— 用户已确认
判定用途   与视角【无关】。俯视工作的用户同样需要超高提示，
           否则会出现"全程俯视作业、软件全绿、实物打不出来"
判定强度   告警，不硬阻断 —— 切片包本身合法，设备可换；zLimitMm 缺省时完全不判
```

⚠️ 不得拿 `autoOrient.maxHeightMm`（现配置 9.0 / 6.0）当限高：那是**自动定向目标高度**，
语义是"尽量压矮"，与"设备物理最高能打多高"是两回事，混用会同时错两处。

命名取 `zLimitMm` 而非 `depthMm`/`heightMm`：本项目已把 Y 命名为 height，再用 height 表示 Z
会直接冲突，用 depth 又与业界 depth=Y 相反。**不做破坏性重命名** ——
改 `heightMm` 会波及 schema/配置/fixture/golden/打印侧对接，得不偿失。

详见 `DOC_DECISION_14_UI_宿主模拟改造专项.md` §6.6。

---

## 2. 14B 核心 facade（Qt-free）

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14B-00 | **核心库分层可行性验证**：能否把 `scene/ layout/ config/ 几何查询/ 包读取/ preview` 拆为 `slicer_base`，其余入 `slicer_engine`；重点验证 `model.cpp`(1970 行) 能否进 base | — | 出结论文档；若不可行则 `model.import` 改 Worker 承载；结论作为 14A-04/14B-01/14C-04 输入 | ✅ **COMPLETE（2026-08-05）**；`model.import=base`，独立编译探针 PASS |
| 14B-01 | 新建 `src/slicer_core/api/`；定义 facade 接口与 DTO（含强制 `ICancelToken`）| 14A-04-R1 | 接口单测；不含 Qt/ABI 类型 | ✅ **COMPLETE（2026-08-05）**；Facade/DTO 编译与头文件门禁 PASS |
| 14B-01-R1 | **内部 Facade DTO 与能力合同 v1.2 对齐**：补齐 package summary/layer/verify/report 及模型法线来源 | 14B-01 | C++ DTO 可无损承载 v1.2；模型法线有权威导入证据；不改外部 ABI/协议 | ✅ **COMPLETE（2026-08-05）**；DTO 字段门禁与法线探针 PASS |
| 14B-01A | **落地 base/engine 两库拆分 + CI 单向依赖检查** | 14B-00, 14B-01 | `slicer_base` 不含 engine 符号；构建图正确 | ✅ **COMPLETE（2026-08-05）**；318 项 source 唯一分配，base 78 / engine 240，单向构建图门禁 PASS |
| 14B-02 | `ModelFacade` + `PackageQueryFacade` 实现（复用既有能力）| 14B-01-R1, 14B-01A | 行为与既有 CLI 一致 | ✅ **COMPLETE（2026-08-05）**；模型元数据、包摘要/层/预览/校验/报告及 base-only 链接门禁 PASS |
| 14B-03 | `SceneFacade`（变换/碰撞/越界权威求值 + revision）| 14B-01 | 与 `layout/` 既有判定逐条一致 | ✅ **COMPLETE（2026-08-05）**；完整 Commit DTO、幂等/原子提交及 Debug/Release target 门禁 PASS |
| **14B-03A** | **`TexturedSceneViewDataProvider`**：从模型/材质资产生成 top `surfacePreview` 与 three_d `mesh + texcoord0 + submeshes + materials + textures`；实现 `appearances[]` 多模型引用、独立 identity 与预算降级 | 14B-02, 14B-03, 14A-04-R1 | checker 3MF、`shengdanjie_fudiao` 与双模型场景正例；白/近白纹理可保真；missing-texture / decode-fail / no-UV 显式失败；不得成功灰模 | ✅ **COMPLETE（由 14B-03A-R1 闭合，2026-08-05）**；独立、Debug/Release、真实 OBJ/3MF、双模型 appearance、纯白/近白与 fail-closed 门禁 PASS |
| **14B-03A-R1** | **双视图 ViewData 合同审计修正**：补齐局部闭合轮廓、`content[]` / `texturePolicy` 内部请求字段、预算降级 `truncated` 语义、three_d 纹理降采样、world 网格单位矩阵及纹理与 `baseColorFactor` 一致性 | 14B-03A；DTO v1.2 | top/three_d 均带 outline；所有预算降级显式报告且不丢纹理；world 网格不重复变换；OBJ/3MF 真实资产和合成负例通过 | ✅ **COMPLETE（2026-08-05）**；独立测试、Debug/Release、真实 OBJ/3MF 与合同门禁 PASS |
| 14B-04 | `SliceFacade`（提交/进度/取消）| 14B-01 | 生产 TIFF 逐字节不变 | ✅ **COMPLETE（2026-08-05）**；正式 Debug/Release target、进度/取消与生产回归门禁 PASS |
| 14B-05 | `slicer_cli` 改走 facade | 14B-02..04 | full 回归通过 | ✅ **COMPLETE（2026-08-05）**；Debug/Release 路由门禁 7/7 PASS，Debug full regression PASS（985.8 s） |
| 14B-06 | **CI 行数门禁 G1..G5 + 白名单机制** | — | 门禁生效（`INT_11` §2.1）；**白名单初始条目见下方注**；新增目录不得入白名单 | ✅ **COMPLETE（2026-08-05）**；quick CI/CTest 已接线 |

> **14B-06 白名单初始条目（与 14E-05 关联）**
>
> 门禁一旦生效，以下两个既有文件立即超标，必须**显式登记**为白名单，否则 CI 直接红：
>
> ```text
> apps/slicer_debug_ui/MainWindow.cpp                    4267 行  → 14E-05 拆分后移除
> apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp     7401 行  → 14E-05 拆分后移除
> ```
>
> 白名单条目**必须带到期条件**（此处为「14E-05 完成」），不得无限期挂着。
> **`apps/slicer_ui_host_sim/`（14E-02 新建）与 `src/slicer_module/`（14C-01 新建）
> 从第一天起受完整门禁约束，不得进白名单** —— 新代码不应重蹈主干覆辙。

---

## 3. 14C DLL 薄壳

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14C-01 | 新建 `src/slicer_module/`；`PM_API`/`PM_CALL __cdecl`/`.def`（11 符号）| 14A-01, 14B-01 | `dumpbin /EXPORTS` 恰好 11 个，无 C++ 修饰名 | ✅ **COMPLETE（2026-08-05）**；Debug/Release 构建、静态合同和 `dumpbin` 精确 11 符号 PASS |
| 14C-02 | 缓冲三态协议 `WriteOut()` 单一实现 | 14C-01 | C-SPI-05a/b/c | ✅ **COMPLETE（2026-08-06）**；Debug/Release 单测、DLL 构建和 11 导出回归 PASS |
| 14C-03 | `HandleRegistry` 句柄生命周期 + `pm_last_error`（TLS）| 14C-01 | C-SPI-04/12/13/14/15 | ✅ **COMPLETE（2026-08-06）**；底层状态与 ABI Debug/Release 门禁 PASS，完整 Worker job 验收保留给 14C-06/14D |
| 14C-04 | 同步轻能力接线（`syncCapabilities[]` 声明）| 14C-02, 14B-02..03A, **14B-00** | 首次 `pm_poll` 即返回终态；**`syncCapabilities[]` 逐条对齐 `DEV_14` §5 承载分派表，二者不一致即判不通过**；`scene.get_viewdata` 不得绕过 14B-03A | ✅ **COMPLETE（2026-08-06）**；13 项进程内承载、真实 SPI 终态轮询、ViewData Provider 复用与 Worker 能力 fail-closed 的 Debug/Release 门禁 PASS |
| | ↳ ⚠️ **前置 14B-00 不可省**：`model.import` 归属目前是「base（待 14B-00 验证）」，是 15 项能力中**唯一未定**的一项。若 14B-00 结论为「导入必须进 Worker」，`syncCapabilities[]` 必须相应移除该项，否则返工。 | | | |
| 14C-05 | `pm_module_info` + `module.json` + 版本/运行时自述 | 14C-01, 14C-04 | C-SPI-01/02/03 | ✅ **COMPLETE（2026-08-06）**；双 Schema、运行时自述、Debug/Release 部署清单、缓冲三态与 11 导出回归 PASS |
| 14C-06 | `test_spi_conformance` 自测套件（受控拆为 06A/06B） | 14C-01..05 | **C-SPI-01..18 全绿** | ⏳ **PARTIAL / WAITING_FOR_06A_06B**；完整套件不得在 Worker 生命周期证据缺失时伪报全绿 |
| 14C-06A | 模块本地 SPI 一致性与无副作用 `pm_self_test` | 14C-01..05, 14C-07 | 不依赖 Worker 的 C-SPI 项形成独立 Debug/Release 套件 | ✅ **COMPLETE（2026-08-06）**；公开 C ABI 动态装载套件 Debug/Release 11/11 PASS，Worker 项显式 BLOCKED |
| 14C-06B | Worker 生命周期 SPI 一致性 | 14C-06A, 14D-04B, 14D-05, 14D-08 | C-SPI-08/09/13/15 真实 Worker 全绿 | ⛔ **PREPARATION_GATE BLOCKED**；等待真实 Worker、安全发布与取消链路 |
| 14C-07 | `DllMain` 红线 + `std::call_once` 初始化 + 无 Qt/PrintSDK 依赖 | 14C-01, 14C-05 | C-SPI-16/17 | ✅ **COMPLETE（2026-08-06）**；Debug/Release 初始化、并发实例、精确 11 导出与 PE 依赖红线 PASS |

**14C 出口**：`slicer_module.dll` 可被独立套件全绿验证 = 打印侧 M1-07 门禁满足。

---

## 4. 14D Worker 与取消

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14D-01 | 新建 `apps/slicer_worker/`（由 `slicer_cli` 演进）| 14A-03 | 与 CLI 行为一致 | ✅ **COMPLETE（2026-08-05）**；独立目标沿用相同 engine 构建轨道，Debug/Release 参数外壳与负例 PASS；文件合同执行由后续卡实现 |
| 14D-02 | `WorkerClient`（DLL 侧）：启动/进度解析/退出码映射/僵尸回收 | 14D-01, 14C-01 | 子进程后端可用 | ✅ **COMPLETE（2026-08-06）**；Debug/Release、严格协议解析、取消/超时与进程树回收门禁 PASS |
| 14D-03 | **`file_contract_v1` 版本协商**：`slicer_worker.exe --contract-info` + major/minor 兼容规则 + 不匹配 fail-closed | 14D-02, 14A-03 | 篡改 major 被拒绝；minor 向后兼容可用 | ✅ **COMPLETE（2026-08-06）**；Debug/Release 合同发现、版本兼容、声明缺失与日志边界门禁 PASS |
| 14D-04 | **切片链路 cancel token 贯穿**（受控拆为 04A/04B）| 14B-04 | 04A 核心贯穿；04B Worker 各阶段取消 ≤2s 且无 staging 残留 | ✅ **PREPARATION_GATE PASS_WITH_SPLIT（2026-08-06）** |
| 14D-04A | 核心 token 贯穿：阶段、逐层和长循环协作检查 | 14B-04 | 直接引擎取消 ≤2s；正常生产回归不变 | ✅ **COMPLETE（2026-08-06）**；核心链路 token、staging 清理、既有包保护和正常字节不变性 Debug/Release PASS |
| 14D-04B | Worker 真实取消、退出与残留收口 | 14D-04A, 14D-05, 14D-08 | 退出码 8、稳定取消码、≤2s、无 staging | PREPARED / 等待后置前置 |
| 14D-05 | staging→自检→原子发布 + 取消/崩溃清理双保险 | 14D-01 | C-SPI-09；无残留 | ⛔ **PREPARATION_GATE BLOCKED（2026-08-06）**；缺 jobId 临时产物身份、真实 Worker 执行、模块退出后二次清理与崩溃恢复 |
| 14D-06 | 取消 `backend=inprocess` 切片路径；`options.backend` 收敛为 `worker` | 14D-02 | 无第二条切片路径 | ⛔ **PREPARATION_GATE BLOCKED（2026-08-06）**；Worker 唯一路由尚不可执行，等待 14D-08/05 解阻 |
| 14D-07 | **引擎一致性套件 E-01..08**（Worker 独立替换的准入门）| 14D-03, 14D-05 | 套件可对任意 Worker 版本运行 | ⛔ **PREPARATION_GATE BLOCKED（2026-08-06）**；E-01..08 尚未规范冻结，且缺真实 Worker 与安全发布证据 |
| 14D-08 | Worker 独立调试入口：`slicer_worker.exe --spi-request <req.json>`（受控拆为 R1..R4） | 14D-01 | 可脱离 DLL 单独运行并附加调试器 | ⛔ **父任务 BLOCKED**；R1 共享执行基础准备门已通过，R2/R3/R4 仍等待真实映射、Facade 与安全发布 |
| 14D-08-R1 | 共享 Worker 请求解析、不可变身份、结果封装与精确调度基础 | 14D-01..03 | 无真实 executor 时身份闭合地显式失败，禁止伪成功和 fallback | ✅ **COMPLETE（2026-08-06）**；Debug/Release 定向套件 8/8 PASS |
| 14D-08-R1-01 | request.json 严格解析与不可变作业身份 | 14D-08-R1 | 合同/路径/身份正负例通过，不创建生产包 | ✅ **COMPLETE（2026-08-06）**；Debug/Release 5/5 定向门禁 PASS |
| 14D-08-R1-02 | result.json 原子写入与稳定退出映射 | 14D-08-R1-01 | 身份闭合、tmp 原子替换、写入失败稳定码 | ✅ **COMPLETE（2026-08-06）**；Debug/Release 4/4 定向门禁 PASS |
| 14D-08-R1-03 | capability 精确调度与命令入口接线 | 14D-08-R1-01..02 | 无 executor 明确失败；测试 fake 不进入生产 Worker | ✅ **COMPLETE（2026-08-06）**；共享 runtime 已接线，生产 Worker fail-closed |

---

## 5. 14E 宿主模拟与交互验证

> 🔑 **权威设计：`docs/slice/DOC/DOC_DECISION_14_UI_宿主模拟改造专项.md` v1.3**
>
> **承载方式已定案：独立 app target，不开分支。** 新建 `apps/slicer_ui_host_sim/`（Qt，只链 DLL）；
> 主干 `apps/slicer_debug_ui/` **一行不改**，继续直连 `slicer_core`。
> 原"UI 模拟分支"方案作废 —— 它与 `INT_07` §3.2「不做长命分支」原则冲突。
>
> ⏱ **时序前置修正为 M-MVP-CANDIDATE → M-MVP（2026-08-05）**
>
> ```text
> M-MVP-CANDIDATE = 14C-06 全绿 + 14D-05 完成
> M-MVP = M-MVP-CANDIDATE + 14E-01 纯 C 宿主闭环 PASS
>
> M-MVP-CANDIDATE 之前：主干 UI 布局与功能【保持原样，不做任何改造】
> Candidate 达成后：仅启动 14E-01；14E-01 PASS 后才启动 14E-02 及后续 Qt UI
> ```
>
> 理由：不能用“宿主闭环已经成立”作为构建第一个宿主的前置。14E-01 是纯 C 验证器，
> 它关闭 M-MVP；Qt UI 仍后置，避免追着 ABI 改。
>
> 🎯 **定位强化**：`slicer_ui_host_sim` 同时是**交付给打印侧的参考实现**，
> 代码质量按对外交付物要求（完整走公开 ABI、每能力有可读示例、Doxygen、错误分支不吞）。
>
> 🚧 **渲染后端选型已移出 14E（2026-08-06）**
>
> ```text
> 独立专项决策：docs/slice/DOC/DOC_DECISION_RENDER_模型显示后端选型与渲染接口冻结.md
> 独立任务清单：docs/codex_task/current/TASKS_RENDER_模型显示与LOD修复补充任务清单.md
>
> 14E【不做】后端选型，只【面向已冻结的 IRenderBackend 接口编程】。
> 后端换谁（qrhi_d3d11 / cpu_raster）都不影响 14E 任何一张卡。
> ```
>
> ⚠️ **R 组另有一项与 14E 相关的既存缺陷**：ViewData 的 `lod1/lod2` 当前由
> **跳采样**（每 N 个三角取 1 个）实现，不是网格简化，会产出破碎网格
> （证据：`SceneViewMeshBuilder.cpp:143-146`）。该缺陷归 **R-B 组**处理，
> **不属于 14E**；但 14E-04c 落地前若未修，3D 视角可能显示满身是洞的模型。
> 触发阈值：单实例超过约 13.8k 三角，22 实例场景下 `lod: auto` 即会降级。
>
> 移出理由：后端选型牵扯 Qt 版本升级与第三方许可等长周期决策，
> 与 Stage 14 的交付目标（能力包 + 集成契约）**无依赖关系**，不应阻塞 14E 与 Stage 14 收口。
>
> **14E 只需最低满足 `cpu_raster` 后端即可通过验收**；GPU 后端是可选加速，不是前置。
> 接口冻结条款 R-C1..C8 见该专项 §5.3，其中 **R-C8（渲染与拾取不得发起跨 DLL 调用）**
> 是 UI-M1 / UI-M7 成立的前提，实现时必须遵守。

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14E-01 | 轨一：`apps/slicer_host_sim/`（控制台，纯 C 调用参考实现），负责关闭 M-MVP | **M-MVP-CANDIDATE（14C-06 + 14D-05）** | 仅经 11 个导出完成导入→变换→切片→取包→校验；可进 CI；演示 fail-closed；PASS 后形成 M-MVP | PREPARED |
| 14E-02 | 轨二：新建 **`apps/slicer_ui_host_sim/`**（Qt）+ `ModuleClient`（运行时装载 11 符号）| **M-MVP** | 三条依赖守卫全过（专项 §3）：CMake 不链 core、源码禁 include core、DLL 不进导入表 | PREPARED |
| 14E-03 | 轨二：`SceneInteractionController` + `TransformCommitPolicy`（三车道 + Stale 回滚）| 14E-02 | **UI-M1** 拖拽期跨 DLL 调用恒为 0；拖拽碰撞仅为本地 bbox 非权威提示；正常 Commit 不追加快照；**UI-M4** Stale 回滚可演示且状态一致 | PREPARED |
| 14E-04 | 轨二：`TopViewRenderPolicy` + `MoveOptimizationPolicy`，使用 14B-03A 的 `surfacePreview` | 14E-03, 14B-03A | top 显示真实纹理；**UI-M2** Commit P95 ≤ 150ms；**UI-M3** 帧率 ≥ 主干 90% | PREPARED |
| **14E-04b** | **能力覆盖达标**：P0 五项端到端打通并可演示；P1 五项全部打通；P2 各调用一次并记录 | 14E-04 | 按专项 §4 清单逐项核对；**UI-M5** 取消 ≤2s 无 `.staging` 残留；**UI-M6** DLL 缺失优雅报错 | NEW |
| **14E-04c** | **带纹理 3D 视角与相机操作**：`SceneRenderPolicy`（**经 `IRenderBackend`，不直接写图形 API**）+ `AppearanceCache` + `CameraController`（orbit/pan/zoom、光标中心缩放、七向预设、透视/正交）+ 构建体积/网格/坐标轴/越界高亮 | 14E-04, **14A-04-R1**, **14A-11**, **14B-03A** | three_d 必须显示 UV/材质/纹理；**UI-M7** 相机期跨 DLL 调用恒为 0（对应冻结条款 R-C8）；**UI-M8** 帧率门由所选后端满足；**最低只需 `cpu_raster` 后端即可验收** | PREPARED（2026-08-04 新增）|
| **14E-04d** | **双入口视图选择**：设置页保存默认 `top` / `three_d`；中央画布分段控件即时切换；按 `slicer_ui_view_spec.json` 实现 1 mm/10 mm 自适应网格和白/近白纹理对比辅助 | 14E-04c | **UI-M9..13**：两视图纹理正确；切换保持 scene/selection/transform/job 并复用缓存；默认值重启恢复；网格范围取 buildVolume 且不改变切片；纹理失败显式报错；辅助显示不修改纹理/TIFF | **NEW** |
| 14E-05 | **拆分主干 UI 大文件**（`MainWindow.cpp` **4267** / `services/UiSmokeTestRunner.cpp` **7401**，与 14B-06 白名单同源）| 14E-04 | 各降至 <1500 行；self-test 绿；**完成后从 14B-06 白名单移除** | PREPARED |
| 14E-06 | 产出"可移植模块清单"交打印侧（**精确到文件级**，标明可直接复制 / 需改写）| 14E-04b, 14E-04d | 打印侧确认可据此评估移植成本 | PREPARED |

> ⚠️ **14E-04c 的 14A-04-R1 与 14B-03A 前置不可省**：前者冻结双视图纹理字段，后者实际提供
> UV/材质/纹理和 top 投影。只有字段、没有 Provider，或用灰模替代，都不能算 3D 能力完成。
>
> **3D 视角不新增任何权威求值** —— 相机与拾取纯本地，模型变换仍走三车道，
> 碰撞/越界仍由 `geometry.collision` / `scene.apply_operation` 给出。13A 系列的实例变换求值全部复用。

> **顺序判断**：14E-05 必须在 14E-02..04 **之后**——先让使用场景长出"可移植 / 切片专有"的边界，再据此拆分，比按文件大小机械切分可靠。
>
> **14E-05 与本专项解耦**：新 app 不改主干，主干拆分按 `INT_11` 独立推进；两者只在 14B-06 白名单上有交集。
>
> **新 app 从第一天起受 14B-06 行数门禁约束**（≤500 行/文件），**不进白名单** —— 新代码不应重蹈主干覆辙。

---

## 6. 14F 打包与联调

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| 14F-01 | `modules/slicer/` 打包（DLL + Worker + module.json + 依赖 DLL）| 14C-06, 14D-07 | 分发包本地隔离验证 | ✅ **SLICER-SIDE COMPLETE** |
| 14F-02 | 与打印侧 M1 联调（装载 + 能力清单 + 自检）| 14F-01 | M1 handoff 与本地接收门禁 | 🟡 **INTERFACE FROZEN / EXTERNAL VALIDATION DEFERRED** |
| 14F-03 | 与打印侧 M2 联调（单模型 → S1 校验）| 14F-02 冻结合同 | S1 正例 + 7 类负例 | ✅ **SLICER-SIDE COMPLETE / EXTERNAL VALIDATION DEFERRED** |
| 14F-04 | 与 RIP 联调（S2 契约 + `rip_output_validator`）| 14A-08 | S2 C1–C7 本地合同门禁 | ✅ **SLICER-SIDE COMPLETE / EXTERNAL VALIDATION DEFERRED** |
| 14F-05 | 端到端到 Ready + 阶段收口报告 | 14F-03/04 本地门禁 | 切片侧收口；外部验收显式延期 | 🟢 **LOCAL CLOSURE READY** |

> 2026-08-07 用户授权暂不执行打印侧验证，并按打印侧可实现性成立继续推进。
> 权威边界见 `docs/slice/DOC/DOC_DECISION_14F_外部验证延期与接口冻结.md`；
> `IMPLEMENTABILITY_ASSUMED` 不得写成打印侧或目标 RIP 已 PASS。

---

## 7. 关键路径与并行

```text
【首批·立即可并行】（无任何前置）
  14A-01  contracts/ + 错误码表
  14A-02  JSON Schema（三组新字段一次到位，见注 A）
  14A-07  第三方依赖合规审查
  14A-09  REPORT_12X 补 03E 行
  14B-06  CI 行数门禁
  14B-00  核心库分层可行性验证

【第二批】14A-02 完成后
  14A-10（whiteSemantics）· 14A-11（zLimitMm）
【第二批】14A-01 完成后
  14A-03 · 14A-04/04-R1（双视图纹理 DTO）· 14A-06

【UI 数据硬前置】
  14B-03A TexturedSceneViewDataProvider
  → 14C-04 scene.get_viewdata 接线
  → 14E-04 top 纹理
  → 14E-04c three_d 纹理

【UI Gate】
  14C-06 + 14D-05 = M-MVP-CANDIDATE
  → 14E-01 纯 C 宿主 PASS = M-MVP
  → 14E-02..04d Qt 参考宿主

【已关闭，不再阻塞】
  ✅ 14A-08 RIP 回签 —— 两轮闭合，条款见 DOC_DECISION_14_S2
  ✅ OPEN-14-01 优先级裁定 —— 已裁定「乙 并行插入」
  ✅ OPEN-14-02 TIFF 后端策略 —— 手写 Writer 默认、LibTIFF 可选；默认切换未授权
  ✅ OPEN-14-03/04/05 —— 随 RIP 六问闭合

【外部验收延期，不再阻塞切片侧本地收口】
  14F-02/03/04/05  打印软件、目标 RIP、干净机与实物证据
  OPEN-14-06       三个必需 OBJ 的处置（产品）
    ↳ 解耦手段：用已有 7 个 strict-PASS 资产先跑通 14F-02/03
  OPEN-14-07       S2-R1 极性映射表（RIP↔打印软件双边）—— 不阻塞切片侧
  G7 类            实物工艺验证 —— 阻塞发布授权，不阻塞开发
```

> **UI 组（14E）不在首批**：`14C-06 + 14D-05` 先形成 **M-MVP-CANDIDATE**，只解锁
> 14E-01；14E-01 PASS 后形成 M-MVP，才解锁 Qt UI。M-MVP 达成前主干 UI 一行不改。

## 8. 与 12E-09D 的文件所有权隔离

| 文件/目录 | Owner |
|---|---|
| `src/slicer_core/config.*` | **12E-09D**（Stage 14 不动）|
| `src/slicer_core/api/`、`src/slicer_module/`、`apps/slicer_worker/` | **Stage 14**（新建，零冲突）|
| `src/slicer_core/api/viewdata/**`（或 14B 分层确认的等价 base 目录）| **14B-03A**；提供双视图纹理 ViewData，不得被 14E 绕过 |
| `apps/slicer_debug_ui/**` | Stage 14 的 M-MVP 前**不修改**；14E 使用独立 `apps/slicer_ui_host_sim/`，不建立模拟分支 |
| `src/slicer_core/slicer.cpp` | 暂无（拆分随 `CLAUDE_09` R-B，本阶段不启动）|

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版，PREPARED。6 个子阶段共 40 张原子任务卡；标注外部依赖与并行项；给出与 12E-09D 的文件所有权隔离 |
| 2026-08-05 | v1.2 | Stage 14 开工基线收口：14A-02 与 14A-04 职责分离；14B-00 移除矛盾前置并明确输出用途；TIFF 后端与 UI 独立 app 边界对齐权威决策 |
| 2026-08-05 | v1.4 | 完成 14A-09：`REPORT_12X` 登记 03E-01/02 COMPLETE、`GO_ON_DEMAND`、默认 `none` 与 14F 外部实机互操作边界 |
| 2026-08-05 | v1.3 | 完成 14A-01：同步打印侧 11 函数 C ABI 头文件、登记 18 项切片错误码并增加 C/C++ 编译与合同数量验证；明确实际 DLL 导出表验证归 14C-01 / 14C-06 |
| 2026-08-05 | v1.4 | 完成 14A-02：新增 manifest、scene、Profile 三份 Draft 2020-12 Schema；覆盖 Stage 15 三字段、可选 `whiteSemantics`、可选 `zLimitMm`，并以真实/旧样例及负向变体通过自动校验 |
| 2026-08-05 | v1.5 | 完成 14A-03 切片侧合同：冻结 `file_contract_v1` 请求/结果/协商 Schema、进度行、退出码、有限超时、Job Object 僵尸回收与 staging 双保险清理；打印侧书面确认仍待回签 |
| 2026-08-05 | v1.6 | 完成 14A-04：冻结 15 项能力请求/响应字段、承载与错误码；完整吸纳 Scene ViewData 网格、LOD、缓存身份与复用现有 ABI 的 blob 分块合同，并增加自动漂移门禁 |
| 2026-08-05 | v1.7 | 完成 14A-05：冻结 Transient/Commit/Production 三车道、operationId 幂等、revision 原子提交、SceneRevisionStale 回读回滚及生产 sceneHash 准入合同 |
| 2026-08-05 | v1.8 | 完成 14A-06：冻结 Cancelling/Cancelled 状态机、≤2s 协作取消、Job Object 兜底、双保险 staging 清理与取消结果 Schema 门禁 |
| 2026-08-05 | v1.9 | 完成 14A-07：落盘第三方 NOTICE、assimp/miniz/LibTIFF 完整许可证、机器分发清单及再分发合规审查门禁 |
| 2026-08-05 | v2.0 | 完成 14A-10：实现 manifest 权威、Profile 默认的 `whiteSemantics` 解析与写出；冲突/非法值 fail-closed，旧包缺字段保持兼容；下一任务推进为 14A-11 |
| 2026-08-05 | v2.1 | 增加 14A-04-R1 与 14B-03A；双视图纹理改为硬标准；修正 M-MVP 自循环、Transient 碰撞权威性和 Commit 多余快照；细化 QOpenGL UI 信息架构与依赖链 |
| 2026-08-05 | v2.2 | 完成 14A-11：增加可选 Z 限高、230 × 100 × 60 mm 显式设备默认、超限非阻断告警及 scene 响应字段；14A 切片侧实现任务全部完成，外部回签继续单独跟踪 |
| 2026-08-05 | v2.3 | 完成 14B-00：输出 301 项文件级 base/engine 归属、4 条窄接口抽取边与独立模型导入编译探针；唯一结论为 `model.import=base` |
| 2026-08-05 | v2.4 | 完成 14B-06：G1..G5 增量门禁接入 quick CI 与 CTest；初始 UI 白名单仅豁免 G4 且绑定 14E-05 到期条件 |
| 2026-08-05 | v2.5 | 完成 14B-01：建立 Qt-free Facade、DTO、`ApiResult` 与强制取消合同；ViewData 对齐 v1.2，下一任务为 14B-01A |
| 2026-08-05 | v2.6 | 完成 14B-01A：落地 `slicer_base` / `slicer_engine` 单向构建图、窄化模型与 DPI 配置边界，并增加 source 唯一归属门禁；14B-02/03/04 可并行 |
| 2026-08-05 | v2.7 | 受控完成 14B-01-R1：内部 Package/Model Facade DTO 对齐能力合同 v1.2，补齐 summary/layer/verify/report 字段与源法线证据；不修改 SPI、能力数量或生产协议 |
| 2026-08-05 | v2.8 | 完成 14B-03：SceneFacade 接入正式 base target，补齐双 revision、完整 Commit 响应、幂等、碰撞/越界权威求值与 14B-03A Provider 边界；Debug/Release 门禁通过 |
| 2026-08-05 | v2.9 | 完成 14B-04：SliceFacade 复用既有生产入口，接入正式 Debug/Release target，冻结提交身份、单调进度、协作取消和生产 TIFF 零改写边界；深度取消仍归 14D-04 |
| 2026-08-05 | v2.10 | 完成 14B-02：ModelFacade 与 PackageQueryFacade 复用权威模型、包、TIFF Reader 和材料预览能力；只读 TIFF/preview 下沉 base，Writer 后端保持 engine；Debug/Release 及 base-only 链接门禁通过 |
| 2026-08-05 | v2.11 | 完成 14B-03A：正式接入 TexturedSceneViewDataProvider，top/three_d 真实纹理、纯白/近白、多模型 appearance、预算与纹理失败关闭均通过独立及 Debug/Release 门禁；打印侧 ACK 仍待确认 |
| 2026-08-05 | v2.12 | 完成 14B-03A-R1 合同审计修正：补齐 outline、content/texturePolicy、显式 truncated、three_d 纹理预算、world 单位矩阵和 baseColorFactor 一致性；14B-05 保持下一任务 |
| 2026-08-05 | v2.13 | 14B-05 实现完成：`--scene-config` 改走生产 SliceFacade，新增架构路由门禁并通过 Debug/Release 目标测试；完整 Debug full regression 执行中 |
| 2026-08-05 | v2.14 | 完成 14B-05：Debug full regression 以 985.8 s 通过，CLI 场景路由迁移收口；14B 全部任务完成，下一批并行进入 14C-01 / 14D-01 |
| 2026-08-05 | v2.15 | 完成 14C-01：建立仅链接 slicer_base 的 slicer_module DLL 外壳，Debug/Release 精确导出 11 个无修饰 pm_* 符号；14C-02/03 可并行启动 |
| 2026-08-05 | v2.16 | 完成 14D-01：建立独立 slicer_worker 目标和稳定参数外壳，Debug/Release 构建及负向合同门禁通过；真实协商、请求执行、取消和安全发布仍按 14D-02..08 原子卡推进 |
| 2026-08-06 | v2.17 | 完成 14C-02：落地唯一 `WriteOut()` 缓冲三态实现，Debug/Release 覆盖探测、差 1、完整写入、空串、负容量和哨兵不变；后续 ABI 字符串出参统一复用 |
| 2026-08-06 | v2.18 | 完成 14C-03：建立不透明句柄注册表、module-job 归属、最小状态机与 TLS 错误 JSON；接入 module 生命周期和 `pm_last_error`，未接线能力继续 fail-closed |
| 2026-08-06 | v2.19 | 完成 14D-02：建立 Windows WorkerClient、严格进度/耗时解析、退出码映射、协作取消与 Job Object 进程树回收；下一批 14C-04 与 14D-03 可并行，14C-05/07 已准备但共享 DLL 接线需串行集成 |
| 2026-08-06 | v2.20 | 完成 14D-03：Worker 输出 `file_contract_v1` 自述，模块侧以 major 相等、worker minor 不低于要求、生产协议和能力完整为准 fail-closed 协商；14C-04 保持并行开发中 |
| 2026-08-06 | v2.21 | 完成 14C-04：13 项同步轻能力经既有 Facade 接入 SPI；`geometry.preflight` 仅 `mode=fast` 进程内执行，ViewData 复用 14B-03A Provider；Debug/Release 真实 ABI 调用门禁 PASS |
| 2026-08-06 | v2.22 | 完成下一批准备审计：14C-05 冻结运行时自述/部署双 Schema 与版本真源；14D-04 受控拆为可立即执行的核心贯穿 04A 和依赖 14D-05/08 的 Worker 收口 04B |
| 2026-08-06 | v2.23 | 完成 14C-05 与 14D-04A：模块自述/部署清单和核心取消令牌贯穿通过 Debug/Release 门禁；14C-07 准备门 PASS，14D-08 显式登记准备阻断缺口 |
| 2026-08-06 | v2.24 | 完成 14C-07：最小 DllMain、进程级 call_once、并发创建、精确 11 导出与 PE 依赖红线通过 Debug/Release 门禁；14C-06 受控拆为可执行 06A 和等待 Worker 的 06B；14D-05/06/07 准备审计均登记真实阻断项 |
| 2026-08-06 | v2.25 | 完成 14C-06A：独立公开 C ABI 一致性程序和无副作用 pm_self_test 通过 Debug/Release 11/11 门禁，完整 14C-06 仍等待 06B；14D-08 受控拆为 R1..R4，共享执行基础 R1 准备门 PASS，下一卡为 R1-01 |
| 2026-08-06 | v2.26 | 完成 14D-08-R1-01：严格解析 file_contract_v1 请求并冻结不可变 job/result/cancel 身份，Debug/Release 定向门禁 5/5 PASS；R1-02 准备门转 READY，父任务继续 BLOCKED |
| 2026-08-06 | v2.27 | 完成 14D-08-R1-02：身份闭合结果、稳定退出映射和同目录 tmp 原子替换通过 Debug/Release 4/4 门禁；R1-03 准备门转 READY，仍禁止伪成功 executor |
| 2026-08-06 | v2.28 | 完成 14D-08-R1-03：三能力精确注册、共享 runtime、取消前检和命令入口接线通过 Debug/Release 8/8 门禁；R1 COMPLETE，R2 映射准备仍 BLOCKED |
| 2026-08-07 | v2.29 | 完成 14F-03 切片侧本地门禁：公开 ABI 单模型 import→transform→Worker slice→S1 strict 正例通过，7 类坏包稳定 fail-closed；打印侧 M2 外部验证保持延期，下一任务为 14F-04 |
| 2026-08-07 | v2.30 | 完成 14F-04 S2 本地合同门禁：冻结 C1-C7 机器合同与稳定错误码，grayBits=1/2 正例及 7 类负例通过；目标 RIP、ChannelSplitter 和极性映射实证保持延期，下一任务为 14F-05 |
