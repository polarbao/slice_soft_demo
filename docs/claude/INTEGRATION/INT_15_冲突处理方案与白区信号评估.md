# INT_15 冲突处理方案 · 白区信号评估 · 层级契约补齐 · UI 模拟分支

> 目录：`docs/claude/INTEGRATION/`。日期：2026-08-03。
> 回答：① 7 项冲突的处理方案（含白区信号新方案评估）；② 三层功能定义/内部契约/Worker 能否独立替换；③ UI 与长文件拆分时点 + 模拟分支方案。
> 证据等级：A=已核实代码/配置事实，P=本方判断。

---

## 第一部分：白区信号新方案评估

## 0. 结论：**该方案不可用**，且是"撞车最严重"的一种选法

提议：把白色区域像素置为 `R/G/B/W/S/V = 0/0/0/255/255/255` 作为白区私有识别信号。

**判定：❌ 不可用。它与现有多份样例配置的【正常模型像素输出】逐字节相同。**

### 0.1 决定性证据（A）

在 `black_is_print` 下（`printValue=0` 出墨 / `emptyValue=255` 空），下列配置的 `modelMaterial` 段落会让**普通模型像素**恰好输出 `0/0/0/255/255/255`：

| 样例配置（A，实测） | `rgb` | `whiteValue` | `varnishValue` |
|---|---|---|---|
| `material_mapping/obj_mtl_material_mapping_rgbwv.json` | `[0,0,0]` | `255` | `255` |
| `material_mapping/obj_mtl_texture_material_mapping_rgbwv.json` | `[0,0,0]` | `255` | `255` |
| `obj_standard/standard_obj_texture_legacy.json` | `[0,0,0]` | `255` | `255` |
| `golden/material_process_top2_fixture.json` | `[0,0,0]` | — | — |
| `material_process/obj_mtl_texture_rgb_white_varnish.json` | `[0,0,0]` | `255` | `255` |

推导：模型区域内 `S=255`（支撑只在模型外），于是 `R/G/B/W/S/V = 0/0/0/255/255/255`。

**更糟的是**：上述多份配置同时设 `"fallbackRgb": [0, 0, 0]`。这意味着**任何缺纹理/无 UV 的回退像素也会落到这个值**——即"纹理缺失"与"白区信号"在字节层面无法区分。

### 0.2 与现行 `W/S/V=0/0/0` 方案的对比

| 方案 | 含义（black_is_print）| 碰撞概率 |
|---|---|---|
| 现行 `W/S/V=0/0/0` | 白墨+支撑+光油**同时打印** | 低——三材料同像素同时出墨在工艺上罕见 |
| 提议 `0/0/0/255/255/255` | RGB 满色黑 + 三材料**都不出** | 🔴 **极高**——这是"纯黑纹理/回退色"的标准输出 |

**所以提议方案不是改进，而是把碰撞概率从"罕见"提到了"常见"。**

### 0.3 更根本的问题：这是带内信令（in-band signaling）

两个方案都在**用像素值兼职传递元数据**。只要走这条路，就永远要回答"如果某天合法内容恰好等于哨兵怎么办"。而本项目已有明确纪律反对这种做法（教程 05 §7）：

> 生产判断应优先使用**语义 sidecar / exact evidence**，不能只从 TIFF 颜色猜测意图。

### 0.4 一个容易被忽略的事实：表达"白色"本来就有正确方式（A）

`material_policy/white_only_all_model.json` 的写法是：

```json
"rgb": [255, 255, 255],   // 不出彩色墨
"whiteValue": 0,          // 白墨打印
"varnishValue": 255
```

即 `255/255/255/0/255/255`——**`W=0` 就是"这里出白墨"的语义正解**，不需要任何哨兵。

所以 RIP 真正想要的，**不是"这里是白色"（已经能表达），而是"这个白色的语义类型"**——`opaque white`（不透明白）还是 `transparent knockout`（透明镂空）。这正是 `12G-TCWS` 冻结时列出的第一个未决问题（A，`DOC_DECISION_12X` §3）。

**这是一个额外的语义维度，用像素值编码必然溢出。**

### 0.5 推荐方案（P，三选一，按优先级）

| # | 方案 | 说明 | 代价 |
|---|---|---|---|
| **甲（推荐）** | **语义 sidecar 掩膜** | 每层附一张 1-bit 掩膜（如 `semantic/white_region_%06d.bin`），manifest 声明其存在与语义版本 | 包体积小幅增加；RIP 需读 sidecar |
| 乙 | **manifest 级 `ripContractId` + 区域声明** | 若白区可由**规则**推导（如"某 materialRole 的区域"），则只需在 manifest 声明契约 ID 与规则，不需逐像素 | 仅适用于规则可推导的场景 |
| 丙 | 保留带内哨兵但**显式化 + 写入期校验** | manifest 显式声明 `sentinel`；writer 在合成后扫描，若**合法内容**产生该值即 fail-closed 报错 | 治标；仍会在合法黑色场景下阻断出图 |

**若短期必须带内**，请务必选丙而不是"沿用现状"——因为现状是**隐式**耦合（RIP 依赖一个不在 `p0.rgbwsv.2` 契约里的约定），一旦我方合法写出该组合就会静默出错。丙至少把它变成显式且可检测。

> ⚠️ **无论选哪个，都不要选 `0/0/0/255/255/255`。**

---

## 第二部分：7 项冲突的处理建议

| # | 冲突 | 处理方案（P）| 责任 | 时机 |
|---|---|---|---|---|
| **C-1** | Stage 14 未立项 | **本轮已办**：产出 `DOC_DECISION_14` + `TASKS_14`（PROPOSED 状态），待你授权转 ACTIVE | 切片侧 | ✅ 见 §4 |
| **C-2** | 优先级冲突（09D vs 封装）| 采纳**并行插入**：09D 走既有序列；Stage 14 的 14A（契约冻结）与 14C（薄壳）同期并行，二者不碰同一批文件 | 你裁定 | 立即 |
| **C-3** | TIFF 字对齐缺陷仍在生产路径 | **建议走 B：切 LibTIFF 为默认后端**。理由：03D 已完成兼容/性能 Gate，切换成本低于修手写 writer，且**根治自产自销盲区**。需新 Gate + 你单独授权 | 你授权 | 14A 期间 |
| **C-4** | RIP 白区语义冲突 | 见第一部分：**否决 `0/0/0/255/255/255`**；推荐语义 sidecar（甲）；纳入对 RIP 的确认清单 | 双方 | 14A 门禁 |
| **C-5** | `REPORT_12X` 漏 03E | 建议补一行：`03E-01 COMPLETE / 03E-02 INTERNAL COMPLETE / EXTERNAL RIP PENDING / NO_GO_DEFAULT` | 切片侧 | 10 分钟 |
| **C-6** | `13F-R2` 悬空引用 | ✅ 已修（`INT_11` Owner 改为"待 UI 拆分专项立项"）；该专项现并入 Stage 14 的 14E | 切片侧 | ✅ 已办 |
| **C-7** | 03E-02 外部 RIP 待确认 | 合并进对 RIP 的**统一确认清单**，一次性发出 | 切片侧起草 | 14A 门禁 |

### 2.1 对 RIP 的统一确认清单（建议一次性发出）

```text
Q1【最高】W/S/V 二值 → 墨滴数量化，是否由 RIP 承担？（OPEN-01）
        若是，档位如何配置？是否已知 grayBits=2 时 White 上限为 6（非 9）？
Q2【高】白区识别：能否放弃带内哨兵，改用语义 sidecar 掩膜？
        若不能，请说明 RIP 侧读取 sidecar 的障碍
Q3【高】白色语义类型（opaque white / transparent knockout）由谁定义、如何传递？
Q4【中】是否支持 PackBits 压缩输入？（决定 03E-02 能否转 GO）
Q5【中】grayBits 的请求路径（CLD_06 §5 示例中缺 profile.device.grayBits）
Q6【中】RIP 输出是否可保证 W/S/V ∈ [0, 按通道上限]，并在 manifest 回写 dropRange？
```

---

## 第三部分：三层功能定义、内部契约与 Worker 替换

## 3. 各层功能是否已明确定义

**外部契约（SPI）已明确；内部契约（层与层之间）尚未完整定义。** 这是 `INT_13` 完善清单里的 C-03，属 P0 缺口。

### 3.1 三层功能定义（本篇正式固化）

| 层 | 功能（做什么）| 反功能（明确不做）|
|---|---|---|
| **`slicer_core`**（静态库）| 全部算法与领域逻辑：导入、几何、预检、修复、场景、排版真值、材料、支撑、光油、栅格、合成、写包、报告、包校验 | 不含 C ABI、不含进程管理、不含 JSON 翻译、不含 Qt、不知道自己被谁调用 |
| **`slicer_module.dll`**（门面）| ① 11 个 C 导出；② 句柄生命周期（`pm_module_t`/`pm_job_t`）；③ JSON ↔ DTO 翻译；④ 能力协商与版本校验；⑤ **承载派发**（进程内 or Worker）；⑥ 进度聚合与取消转发；⑦ 错误码归一 | 不含算法；不做业务决策；不持久化作业状态 |
| **`slicer_worker.exe`**（执行体）| ① 接收作业请求文件；② 调用 core 执行重作业；③ 输出结构化进度到 stdout；④ staging→自检→原子发布；⑤ 协作式取消与清理 | 不对宿主暴露任何 API；不做能力协商；不解析 SPI JSON 之外的东西 |

### 3.2 内部契约现状：三条边界，只有一条定义完整

| 边界 | 契约 | 状态 |
|---|---|---|
| 打印软件 ↔ DLL | `print_module_spi.h`（11 导出 + JSON schema + 错误码）| ✅ **完整**（唯一完整的一条）|
| DLL ↔ core | `slicer_core/api/` 的 C++ facade 接口 | 🔴 **未定义**——`api/` 目录尚不存在 |
| DLL ↔ Worker | `file_contract_v1` | 🟡 **仅草案**——只有进度行格式与退出码表，缺请求/结果 JSON schema、超时、僵尸回收、staging 清理时序 |

**结论（P）**：**功能包内部尚无可供并行开发的完整契约。** 要支持"多人/多轮并行开发"，必须先补 `api/` facade 接口与 `file_contract_v1` 完整规格——这已列为 Stage 14 的 14A 任务。

### 3.3 Worker 能否独立替换？—— **当前不能**

> 问：如果需要替换 `slicer_worker.exe`，只需替换对应版本即可？

**答：不能。当前设计要求 DLL 与 Worker 必须来自同一次构建、成对替换。**

原因（A + P）：

```text
① core 被【静态链接】进 DLL 与 Worker 各一份（INT_10 §1.1）
   → 单独换 Worker = 两份 core 版本不一致
② 进程内后端与子进程后端必须产出【逐字节相同】的结果（INT_10 §3.5）
   → core 版本不一致会直接破坏这条不变量
③ file_contract_v1 目前无版本协商机制
   → 换了 Worker 也无法在启动时发现协议不兼容
```

**若要支持独立替换 Worker，需要额外做三件事（P）**：

| # | 动作 | 难度 |
|---|---|---|
| 1 | `file_contract_v1` 版本化 + DLL 启动 Worker 时协商，不匹配 fail-closed | 低 |
| 2 | 保证 core 在版本间**行为不变**（否则双后端结果分叉）| 🔴 **高**——这本质是要给 core 也定 ABI/行为契约 |
| 3 | 双后端一致性测试覆盖**跨版本组合**（v1 DLL × v2 Worker）| 中 |

**建议（P）**：**v1.0 之前不支持独立替换**，把 `modules/slicer/` 整个目录作为一个原子发布单元。理由是第 2 条成本极高，而收益（能单独热修 Worker）在当前阶段并不迫切。

**当前必须实现的最小保护**：DLL 启动 Worker 前校验其 `--version` 与自身构建标识一致，不一致报 `PM-SLICER-INTERNAL-0099` 并拒绝执行（`INT_10` §3.3 已列，Stage 14 的 14D 落地）。

---

## 第四部分：UI 与长文件拆分时点 + 模拟分支

## 4. 你提的"模拟分支"方案——我认为比我原来的建议更好

### 4.1 一个此前被我低估的关键事实（A）

**打印软件 `PrintApp` 也是 Qt 5.15 Widgets**（`find_package(Qt5 REQUIRED COMPONENTS Core Widgets Charts Network)`）。

这意味着：**切片侧 UI 里做出来的交互模块，理论上可以直接移植到打印软件**，只要它们满足一个条件——**只依赖 DLL 的 C ABI，不依赖 `slicer_core` 内部类型**。

我此前提的 `slicer_host_sim`（控制台）能验 ABI 完整性，但验不了手感；你提的 UI 模拟分支能验手感，还能产出可移植件。**两者互补，不是二选一。**

### 4.2 建议方案：双轨

| 轨 | 产物 | 验证什么 | 成本 |
|---|---|---|---|
| **轨一** `apps/slicer_host_sim/`（控制台）| 纯 C 调用参考实现 | ABI 完整性、版本协商、取消、fail-closed；可进 CI 常驻 | 2–3 人日 |
| **轨二** UI 模拟分支 | **可移植的操作层模块** | 拖拽/移动手感、显示策略、移动优化、三车道闭环 | 8–12 人日 |

### 4.3 轨二的关键设计：什么才是"可移植的"

要让模块能直接搬进打印软件，必须从一开始就按这个边界写（P）：

```text
✅ 可移植（只依赖 C ABI + Qt）
   SceneInteractionController   拖拽/选择/变换的状态机（transient 态管理）
   TransformCommitPolicy        何时提交、如何携带 expectedSceneRevision、Stale 如何回滚
   TopViewRenderPolicy          俯视显示策略（LOD、重绘节流、脏区域）
   MoveOptimizationPolicy       移动优化（本地矩阵预览、防抖、批量提交）
   ModuleClient                 DLL 装载与调用封装（LoadLibrary/GetProcAddress/JSON）

❌ 不可移植（切片侧专有）
   现有 MainWindow / UiSmokeTestRunner（与切片调试深度绑定）
   任何直接 include slicer_core 头的代码
```

**硬性约束**：轨二的新增模块**禁止 include `slicer_core/**`**，只能通过 `ModuleClient` 走 DLL。用 CI 依赖检查强制——这样"可移植"不是靠自觉，而是靠构建系统保证。

### 4.4 三车道在 UI 里怎么落（这是移植价值最高的部分）

```text
Transient  拖拽中：SceneInteractionController 只改本地矩阵 + 本地 bbox 近似
           → 不跨 DLL；采集"Host→DLL 调用次数"证明 mouse-move 零调用
Commit     松手：TransformCommitPolicy 提交 operationId + expectedSceneRevision
           → 收 newRevision/sceneHash/collisions/outOfBounds/preflightDelta
           → Stale 则重取重试；碰撞/越界则回滚显示 + 稳定错误码文案
Production 切片：只接受已提交的 sceneHash
```

打印软件拿到这三个类，等于拿到了整套交互契约的参考实现。

### 4.5 时点安排

| 工作 | 时点 | 依据 |
|---|---|---|
| 长文件门禁（G1..G5）| **立即**（2–3 人日）| `INT_11` §2.1——先止血，不依赖任何前置 |
| `tiff_io` 处置（切 LibTIFF 或修对齐）| 14A 期间 | C-3 |
| 轨一 `slicer_host_sim` | 14C 后（DLL 可用即做）| 需要 DLL |
| **轨二 UI 模拟分支** | **14C 后开分支，与 14D/14E 并行** | 需要 DLL；分支隔离不阻塞主线 |
| UI 大文件拆分（`MainWindow` / `UiSmokeTestRunner`）| **14E**，且**以轨二的可移植模块为拆分目标** | 见下 |
| core 长文件拆分（`slicer.cpp` 等）| 随 14B 步骤化 | `INT_11` §3 |

**一个重要的顺序判断（P）**：**UI 拆分应该在模拟分支【之后】做，而不是之前。**

理由：先做模拟分支，会自然长出"哪些是可移植的操作层、哪些是切片专有调试件"这条边界；届时再拆 `MainWindow`，就有了**明确的拆分目标**，而不是凭空按"窗口骨架/菜单/面板"这类形式化维度切。**让使用场景来定义边界，比让文件大小来定义边界更可靠。**

---

## 5. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。以样例配置实证否决 `0/0/0/255/255/255` 白区哨兵（与 5 份配置的正常输出逐字节相同，且 `fallbackRgb=[0,0,0]` 使缺纹理回退像素同值）；指出 `W=0` 已是白色语义正解、RIP 真正缺的是 opaque/knockout 类型维度；给出 7 项冲突逐项处理；固化三层功能定义并判定 **Worker 当前不可独立替换**；采纳并细化 UI 模拟分支双轨方案，提出"UI 拆分应在模拟分支之后" |
