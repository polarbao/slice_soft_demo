# DOC_DECISION_14 切片能力包封装与打印软件集成专项

> 文档状态：**PROPOSED / PENDING USER AUTHORIZATION**
> 版本：v1.0 ｜ 决策日期：2026-08-03
> 作者：Claude（分析与起草）；正式生效需用户授权并由主线开发接管
> 证据等级：A=已核实代码/文档事实，B=目标设计，P=判断
> 详细分析与推导过程见 `docs/claude/INTEGRATION/INT_06..15`

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
G-4 白区识别不得采用与合法内容碰撞的带内哨兵（`0/0/0/255/255/255` 已实证否决，见 INT_15 §0）；
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

D-3 白区语义传递 = 【否决带内哨兵，推荐语义 sidecar 掩膜】
    依据：0/0/0/255/255/255 与 5 份现有样例配置的普通模型像素输出逐字节相同，
         且多数配置 fallbackRgb=[0,0,0] 使"缺纹理回退"与"白区信号"无法区分。
         实测证据见 INT_15 §0.1。最终方案待 RIP 侧回签（OPEN-14-04）。
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
| 用带内像素哨兵传白区语义 | 与合法内容碰撞（实证见 `INT_15` §0）|
| 等 12E 全部收口后再启动封装 | 打印侧 M1 门禁空等约 8–10 周 |

## 8. 未决项

| 编号 | 事项 | 需谁答 |
|---|---|---|
| ~~OPEN-14-01~~ | ~~优先级插入方案~~ → ✅ **已裁定：乙 并行插入**（§5.1 D-1）| — |
| ~~OPEN-14-02~~ | ~~TIFF 缺陷处置~~ → ✅ **已裁定：切 LibTIFF 默认后端**（§5.1 D-2，仍需 G-3 授权）| — |
| OPEN-14-03 | W/S/V 墨滴量化归属（OPEN-01）| RIP 侧 |
| OPEN-14-04 | 白区语义传递方式（建议语义 sidecar）| RIP 侧 + 产品 |
| OPEN-14-05 | PackBits 压缩是否被目标 RIP 支持 | RIP 侧 |
| OPEN-14-06 | 三个必需 OBJ 的处置（外部修模 / 换资产）| 产品 |

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版，PROPOSED。成立 Stage 14；固化三层封装结构与三条边界规则；定义首版能力面与三车道交互；列 8 条强制 Gate、6 个子阶段、5 个被拒方案与 6 项未决 |
