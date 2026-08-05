# DOC_DECISION_14 切片能力包封装与打印软件集成专项

> 文档状态：✅ **ACTIVE**（用户于 2026-08-04 授权激活）
> 版本：v1.3 ｜ 决策日期：2026-08-03 ｜ 激活日期：2026-08-04
> 作者：Claude（分析与起草）；执行由主线开发（codex）接管
> 证据等级：A=已核实代码/文档事实，B=目标设计，P=判断
> 详细分析与推导过程见 `docs/claude/INTEGRATION/INT_06..15`
>
> 🔑 **S2（RIP 接缝）权威条款见 `DOC_DECISION_14_S2_RIP接口合同定案.md` —— 实施只看该文。**
> 本文负责封装结构、能力面与三方边界；S2 逐条条款不在本文重复。

---

## 0. 激活记录（2026-08-04）

**三项开工前置全部清空：**

| 前置 | 状态 |
|---|---|
| Stage 15（阻断实际生产使用，优先级高于 14） | ✅ COMPLETE / PRODUCTION ENABLED |
| RIP 六问回签（14A-08） | ✅ 两轮均闭合，条款收敛至 `DOC_DECISION_14_S2` |
| 用户授权 | ✅ 2026-08-04 |

**本轮 RIP 问答对 Stage 14 的净影响：**

```text
✅ 解锁  Q1=方案A  → 切片与打印软件零改动，14F 具备开工条件
✅ 解锁  Q3.1      → 12G-TCWS 保持冻结，p0.rgbwsv.3 不需要
✅ 解锁  Q4        → 03E-02 转 GO_ON_DEMAND
➕ 新增  14A-10    → manifest 新增 whiteSemantics（唯一新实现工作）
⛔ 作废  路径 A 配套（WSV=000 哨兵 Writer 断言、manifest ripBoundIntermediate 字段）
                    及其余 7 项候选方案，见 DOC_DECISION_14_S2 §4
```

> ⚠️ **不要误删**：错误码 `PM-SLICER-CONTRACT-0060` 本身**有效** —— 它是 SPI 既有通用错误码
> （「自检发现产物不符合 `p0.rgbwsv.2`」，见 `INT_07` §错误码表、`INT_02` §6）。
> 作废的只是「用它承载 `WSV=000` 哨兵扫描」这一用途；错误码表整体保留。

```text
```

**首批开工任务**：14A-01、14A-02、14A-07、14A-09、14B-06、14B-00（互不依赖，可并行）。

---

## 1. 为什么要成立独立专项

切片软件需要以**能力包**形式接入打印软件（`ry_print_demo / PrintSolution`）。该工作同时改变四条此前稳定的边界：

```text
① 对外公共 ABI、句柄生命周期、内存所有权与版本协商；
② 长时/高内存作业的进程隔离、进度、取消、崩溃恢复与原子发布；
③ 三方（切片 / RIP / 打印）之间的数据接缝契约与强制校验；
④ 构建产物形态：从"内部可执行程序"变为"可分发的模块包"。
```

这些超出任何既有阶段（12E 生产语义、12F 性能、13x 场景与 UI）的范围，**不能作为某次文件拆分或一次 CMake 改动附带完成**，故成立 Stage 14。

## 2. 与既有阶段的关系

| 阶段 | 关系 |
|---|---|
| 12E-09D / 12E-10 | **并行，不阻塞**。Stage 14 首版能力面只暴露已被 12E-09B/09C 收口的语义，不依赖 09D 的纹理厚度结论 |
| 12F | Stage 14 的步骤边界是 12F 优化的前置；但 12F 仍按实测证据逐项授权 |
| 13F-R1 | 独立并行。13F 的取消状态机（`Cancelling≠Cancelled`）被 Stage 14 直接采纳为契约条款 |
| 12G-TCWS | 保持冻结。Stage 14 **不实现**其配置/composer/RIP 合同；但会把"白区语义"列为对 RIP 的确认项 |
| 03D | 已 COMPLETE（`GO_OPTIONAL`）。Stage 14 需就"是否切 LibTIFF 为默认后端"做独立决策（见 §5 G-3）|
| 03E | `03E-02 INTERNAL COMPLETE / EXTERNAL RIP PENDING`。压缩是否启用纳入对 RIP 确认清单 |

## 3. 封装结构（v1.1 修订：Worker 定位为可独立替换的切片引擎）

> 🔴 **v1.1 修订（2026-08-03）**：v1.0 曾要求"DLL 与 Worker 必须同一次构建、成对替换"。该约束**已撤销**——其根源是一个非必要的 `backend=inprocess` 切片调试选项。取消该选项后，Worker 成为可独立迭代的引擎。完整推导见 `docs/claude/INTEGRATION/INT_16`。

```text
slicer_base.lib    静态 · 稳定层（scene/layout/config/几何查询/包读取/preview）
                   → DLL 与 Worker 都链接 · 中间产物 · 不进交付包
slicer_engine.lib  静态 · 迭代层（slicer.cpp/steps/materials/support/raster/writer/repair）
                   → 【仅】Worker 链接 · 中间产物 · 不进交付包
slicer_module.dll  ABI 门面 + 轻量交互能力 · 链 base ·【不含 engine】· 交付 ·宿主唯一入口
slicer_worker.exe  切片引擎 · 链 base + engine · 交付 ·【可独立替换】
```

五条边界规则：

```text
① Worker 不对宿主暴露 API；宿主永远只认 DLL（Worker 协议是模块内部实现）；
② 切片【只】在 Worker 执行，不提供进程内切片路径 —— 由此消除双后端行为分叉风险；
③ slicer_base 不得依赖 slicer_engine（单向，CI 检查）；
④ Worker 可独立替换，当且仅当：file_contract major 相同 + produces 仍含 p0.rgbwsv.2
   + 通过引擎一致性套件 E-01..08；替换不需重编 DLL、不需改宿主；
⑤ 跨 ABI 只允许 C 基本类型、const char*、不透明句柄；禁止 STL / Qt / 异常跨界。
```

**三条契约与其版本轴**：

| 边界 | 契约 | 版本轴 |
|---|---|---|
| 打印软件 ↔ DLL | `print_module_spi.h` + 能力 DTO | `PM_SPI_VERSION` |
| DLL ↔ Worker | `file_contract_v1` | **`major.minor` 独立版本，启动协商** |
| DLL/Worker ↔ Core | `api/Facades.h`（C++，含强制 `ICancelToken`）| 随 core 源码 |

## 4. 能力面（首版，方案 C + 包查询扩展）

```text
model.import / model.get_metadata / model.release
geometry.preflight（fast / full） / geometry.collision / geometry.repair（默认关闭）
scene.apply_operation（Commit 车道） / scene.get_snapshot / scene.get_viewdata
slice.rgbwsv
package.verify / package.get_summary / package.get_layer_descriptor
package.render_layer_preview / package.read_report
```

**不提供**：排版摆放策略（packing 引擎）、设置与 Profile 管理、渲染、RIP、通道化、作业队列、任何 Qt 类型。

交互采用**三车道模型**：Transient（宿主本地，不跨 DLL）→ Commit（携 `operationId` + `expectedSceneRevision`，模块权威求值）→ Production（只接受已提交 `sceneHash`）。

## 5. 强制 Gate

```text
G-1 不修改 p0.rgbwsv.2 / RGBWSV 顺序 / uint8 / black_is_print / legacy 默认；
G-2 契约冻结（14A）未完成，不得进入 14B 及以后；
G-3 切换默认 TIFF Writer 后端需独立 Gate 与用户单独授权（03D 判定为 GO_OPTIONAL）；
G-4 白区识别不得采用未保留、未转义且与合法内容碰撞的带内哨兵；
    `0/0/0/255/255/255` 是合法 RGB 纯黑像素，不能仅靠删除配置把它变成保留值；
G-5 12G-TCWS 未解冻前，不实现其配置、resolver、composer、UI 或 RIP 合同；
G-6 模块缺失/损坏时，打印软件纯打印入口必须仍可启动；
G-7 【v1.1 替换】切片只在 Worker 执行；Worker 替换须过引擎一致性套件 E-01..08；
    golden 输出变化必须在 release note 显式声明并更新基线，禁止静默漂移；
G-8 取消语义：pm_cancel 仅表示进入 Cancelling，真实退出后方可置 Cancelled，且 .staging 必须清理；
G-9 【v1.1 新增】slicer_base 不得依赖 slicer_engine；slicer_module 的依赖闭包中不得出现
    engine 符号（CI 强制）。
```

## 5.1 已裁定事项（2026-08-03）

```text
D-1 优先级插入方案 = 【乙 并行插入】
    12E-09D 走既有序列；Stage 14 的 14A（契约冻结）与 14B/14C 同期并行。
    依据：v0.1 能力面语义已由 12E-09B/09C 收口，不依赖 09D 结论；
         且二者文件所有权可完全隔离（TASKS_14 §8）。

D-2 TIFF 字对齐缺陷处置 = 【切 LibTIFF 为默认后端】
    依据：03D-01..07 已完成兼容/性能 Gate，切换成本低于修手写 writer，
         且根治"自产自销读写器互为盲区"的结构问题。
    约束：仍需按 G-3 走独立 Gate 与授权；切换前后须逐像素解码一致。

D-3 白区语义传递 = 【当前阶段保持固定六通道，不新增逐层 sidecar；未转义 RGB 黑哨兵 NO-GO】
    依据：完整审计 109 份配置，59 份明确允许普通模型像素产生
         0/0/0/255/255/255，32 份启用纹理的配置使用有效黑色 fallback；
         39 张现有贴图中 15 张含可见纯黑像素。删除配置无法禁止合法输入产生纯黑。
    当前兼容候选：把既有 WSV=000 限定为版本化 rip_bound_intermediate 私有合同，
         前提是 RIP 证明其在物理量化前拦截且 S/V 不泄漏。
    目标评估候选：白区显式写 W=0、S/V=255，由 RIP Profile 决定使用或抑制 W。
    详细证据见 DOC_ANALYSIS_14_Q2_RIP白区带内信号与配置冲突审查.md；
         最终方案仍待 RIP 侧回签（OPEN-14-04）。
```

## 6. 阶段划分

| 子阶段 | 目标 | 出口 |
|---|---|---|
| **14A 契约冻结** | `contracts/` 落盘（SPI 头 + 3 份 schema + `file_contract_v1`）；能力 DTO 字段级规格；对 RIP 确认清单发出并回签 | 三方书面确认；schema 可校验真实产物 |
| **14B 核心 facade** | 新建 `src/slicer_core/api/`；Qt-free C++ 门面；CLI 改走 facade | 行为不变；facade 单测 |
| **14C DLL 薄壳** | 新建 `src/slicer_module/`；11 导出 + `.def` + 句柄 + 缓冲三态 | **过 C-SPI-01..18** |
| **14D Worker 与取消** | `apps/slicer_worker/`；`file_contract_v1` 落地；**切片链路 cancel token** | 取消 ≤2s 且无 staging 残留；双后端 SHA 一致 |
| **14E 交互验证与拆分** | 轨一 `slicer_host_sim` + 轨二 UI 模拟分支；据此拆 UI 大文件 | 可移植操作层模块产出；`mouse-move` 零跨 DLL 调用 |
| **14F 打包与联调** | `modules/slicer/` 打包；与打印侧 M1–M5 联调 | 干净机装载；端到端到 Ready |

## 7. 拒绝的替代方案

| 方案 | 拒绝理由 |
|---|---|
| 只提供切片引擎 exe，几何能力由打印侧自建 | 几何真值分裂；打印侧需引入 CGAL（GPL-3+ 许可证风险）|
| 只提供 DLL，不做 Worker | 边打印边切片时，切片 OOM/崩溃会拖垮正在进行的打印 |
| core 也做成 DLL | C++ 类型跨 DLL 边界，CRT/ABI 不匹配即崩 |
| 用未保留、未转义的带内像素哨兵传白区语义 | 与合法内容碰撞；配置清理不能建立协议保留值（完整审计见 `DOC_ANALYSIS_14_Q2_…`）|
| 等 12E 全部收口后再启动封装 | 打印侧 M1 门禁空等约 8–10 周 |

## 8. 未决项

| 编号 | 事项 | 需谁答 |
|---|---|---|
| ~~OPEN-14-01~~ | ~~优先级插入方案~~ → ✅ **已裁定：乙 并行插入**（§5.1 D-1）| — |
| ~~OPEN-14-02~~ | ~~TIFF 缺陷处置~~ → ✅ **已裁定：切 LibTIFF 默认后端**（§5.1 D-2，仍需 G-3 授权）| — |
| ~~OPEN-14-03~~ | ~~W/S/V 墨滴量化归属~~ → ✅ **已闭合：由 RIP 承担（方案 A）**，档位见 `DOC_DECISION_14_S2` §1.2 | — |
| ~~OPEN-14-04~~ | ~~白区语义传递方式~~ → ✅ **已闭合：路径 D**（废弃 `WSV=000`，改用 `W=0` 真实材料语义），见 `DOC_DECISION_14_S2` §1.3 | — |
| ~~OPEN-14-05~~ | ~~PackBits 是否被目标 RIP 支持~~ → ✅ **已闭合：支持**；03E-02 转 `GO_ON_DEMAND`，见 `DOC_DECISION_14_S2` §1.5 | — |
| OPEN-14-06 | 三个必需 OBJ 的处置（外部修模 / 换资产）| 产品 |
| **OPEN-14-07** | **S2-R1 极性映射表**（输入字节 `0`/`255` 各输出几滴）—— **切片侧不持有**，转 RIP ↔ 打印软件双边确认；见 `DOC_DECISION_14_S2` §3.1 | RIP 侧 + 打印侧 |

> OPEN-14-03/04/05 于 2026-08-04 随 RIP 六问两轮闭合而关闭。**S2 逐条条款以
> `DOC_DECISION_14_S2_RIP接口合同定案.md` 为准，本表只记录开闭状态。**

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版，PROPOSED。成立 Stage 14；固化三层封装结构与三条边界规则；定义首版能力面与三车道交互；列 8 条强制 Gate、6 个子阶段、5 个被拒方案与 6 项未决 |
| 2026-08-03 | v1.1 | Worker 定位修订为可独立替换引擎；取消进程内切片路径；补 base/engine 单向依赖 Gate |
| 2026-08-04 | v1.3 | **用户授权激活，状态转 ACTIVE**（§0 激活记录）；RIP 六问两轮闭合，S2 条款收敛至 `DOC_DECISION_14_S2_RIP接口合同定案.md`；关闭 OPEN-14-03/04/05，新增 OPEN-14-07（极性映射表转双边）；登记 14A-10（manifest `whiteSemantics`）与 8 项作废方案 |
| 2026-08-03 | v1.2 | 按 Q2 深度审查修订 D-3/G-4/OPEN-14-04：当前阶段不新增逐层 sidecar；将 `0/0/0/255/255/255` 定性为不可直接占用的合法纯黑；记录 59 份直接配置、32 份黑 fallback 和 15 张真实黑贴图证据；转为确认既有 WSV=000 或 W-only Profile 路径 |
