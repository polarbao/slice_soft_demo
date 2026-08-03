# INT_11 文件拆分与结构治理专项

> 目录：`docs/claude/INTEGRATION/`。日期：2026-08-02。视角：切片软件构建者。
> 回答：**哪些文件过长需要拆分？是否应成立专项？如何执行？**
> 证据等级：A=实测数据（2026-08-02 全量测量），P=本方判断。

---

## 0. 结论

**成立专项，但不是"按行数拆文件"的专项，而是"防增生 + 按边界拆"的结构治理专项。**

三个核心判断（P）：

1. **结构不缺，缺的是门禁。** `apps/slicer_debug_ui/` 已有 `controllers/ models/ services/ widgets/ workers/` 完整分层（152 文件），`src/slicer_core/` 有 18 个子目录（295 文件）——**但新代码在绕过这些结构**：`MainWindow.cpp` 从 1611 涨到 **3659**（+127%），`UiSmokeTestRunner.cpp` 从 2618 涨到 **6963**（+166%）。**结构存在而未被使用，说明问题不是"没地方放"，是"没人拦"。**
2. **"随功能阶段拆"不足以止血。** s14 主张"不成立长文件专项，拆分归属于改变该文件边界的功能阶段"——这条**对一半**：拆分确实应由改动该文件的任务承担（避免大爆炸），但过去三个月的数据证明，**仅靠这条，文件是净增长的**。必须补一道**增量门禁**。
3. **抽取开工未填是最坏的状态。** `config/NormalizedConfig.cpp` = **6 行**、`pipeline/PipelineContext.cpp` = **6 行**，而 `config.cpp` = 1168、`slicer.cpp` = 5157。空壳目录制造了"已经在拆"的错觉，实际零迁移。

---

## 1. 实测数据（A，2026-08-02）

### 1.1 规模总览

| 范围 | 文件数 | 总行数 |
|---|---:|---:|
| `src/slicer_core/` | 295 | ~62,020 |
| `apps/` | 152 | 42,429 |
| `tests/` | 81 | 30,538 |
| **>500 行文件** | **52**（src 32 / apps 20）| — |
| **>1000 行文件** | **13**（src 8 / apps 5）| — |

### 1.2 相对 2026-07-22 基线的增长（A）

| 文件 | 基线 | 当前 | Δ | 增幅 |
|---|---:|---:|---:|---:|
| `apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp` | 2618 | **6963** | **+4345** | **+166%** 🔴 |
| `apps/slicer_debug_ui/MainWindow.cpp` | 1611 | **3659** | **+2048** | **+127%** 🔴 |
| `src/slicer_core/slicer.cpp` | 4830 | **5157** | +327 | +6.8% |
| `src/slicer_core/model.cpp` | 1662 | **1970** | +308 | +18.5% |
| `src/slicer_core/config.cpp` | 1030 | **1168** | +138 | +13.4% |
| `apps/slicer_cli/main.cpp` | 743 | **871** | +128 | +17.2% |
| `src/slicer_core/tiff_io.cpp` | 641 | **655** | +14 | +2.2% |

> **最刺眼的一条（P）**：`pipeline/` 目录已有 **44 个文件、10195 行**，但 `slicer.cpp` **不降反增 327 行**。这证明 pipeline 的建立是**增生（additive）而非迁移（migration）**——新能力在旁边长出，旧单体一行没走。这正是 `CLAUDE_09` 判断"绞杀式重构必须真正迁移"的实证。

### 1.3 十大 god file

| 排名 | 文件 | 行数 | 判定理由 |
|---:|---|---:|---|
| 1 | `apps/.../services/UiSmokeTestRunner.cpp` | **6963** | 88 行头 + 6963 行实现；每加一个 UI 场景就往里追加 |
| 2 | `src/slicer_core/slicer.cpp` | **5157** | 106 行头 vs 5157 行实现；切片/栅格/合成/支撑/输出编排同居 |
| 3 | `apps/slicer_debug_ui/MainWindow.cpp` | **3659** | 典型 Qt god window；分层目录齐全却仍在膨胀 |
| 4 | `src/slicer_core/model.cpp` | **1970** | 127 行头；网格所有权 + 导入 + 变换 + bbox 校验混装 |
| 5 | `apps/multi_model_scene_matrix/Main.cpp` | 1257 | demo 的 main() 长成完整矩阵框架 |
| 6 | `apps/.../models/SceneDocument.cpp` + `.h` | 1211 + **543** | **543 行头是全仓库最大头文件** → 重编译瓶颈 |
| 7 | `src/slicer_core/preview/TiffLayerSource.cpp` | 1188 | TIFF 解码 + 层缓存 + 预览缩放三合一 |
| 8 | `src/slicer_core/pipeline/SceneLayerComposer.cpp` | 1163 | 头 16 行 vs 实现 1163 行 = **72:1**，全仓库最悬殊 |
| 9 | `src/slicer_core/pipeline/MultiModelProductionService.cpp` | 1126 | 事实上的 orchestration 层，却住在 `pipeline/` |
| 10 | `src/slicer_core/config.cpp` | 1168 | 解析+默认+校验+迁移+归一五合一，而抽取目标是空壳 |

其余 >1000 行：`MultiModelScene.cpp` 1099、`RgbwsvPackageWriter.cpp` 1022、`LayerPreviewPanel.cpp` 1164。

---

## 2. 专项设计：两条腿走路

### 2.1 腿一：**增量门禁**（立刻生效，成本最低，收益最大）

**这是本专项最重要的一条**——先止血，再手术。

```text
G1 新增文件硬上限：新建 .cpp/.h 单文件 ≤ 500 行，超出 CI 拒绝合并
G2 存量文件只减不增：对 >1000 行的 13 个文件，任何 PR 若使其行数【净增长】即拒绝
   （允许"改动但不增长"，鼓励"顺手迁出"）
G3 头文件上限：新建头 ≤ 200 行（防第二个 543 行 SceneDocument.h）
G4 头/实现比例告警：单 .cpp > 800 行且对应 .h < 100 行 → 告警（隐藏面过大）
G5 空壳检测：新建文件 < 20 行且 30 天无增长 → 告警（防"抽取开工未填"）
```

实现：一个 CI 脚本（约 100 行 PowerShell/Python）+ 白名单机制（白名单需注明豁免理由与到期日）。

> **为什么门禁优先于拆分（P）**：过去三个月拆分动作为零，而两个 UI 文件净增 6393 行。**若不先止血，拆完还会再长回来。** 门禁是唯一能防止复发的机制。

### 2.2 腿二：**按边界拆分**（随功能阶段推进，吸纳 s14 的治理纪律）

拆分动作**不单独立项排期**，而是挂在改动该文件的功能任务上。执行纪律（吸纳 s14 `DOC_DECISION_14` §6，这几条写得很好）：

```text
① 固定顺序：characterization tests → wrap → move → dependency guard → regression
② 单次原子任务【不同时】改变算法、公共合同和文件布局（三者只能改其一）
③ 同一文件同一时间只能有一个任务 Owner
④ 行数不是 DoD：拆后仍 >1000 行但职责单一、测试独立的，【不以行数为由继续机械拆分】
⑤ 每次拆分必须以"生产 TIFF 逐字节不变 + RIP strict"为出口门
```

---

## 3. 拆分矩阵与 Owner 分配

**Owner 冲突是拆分失败的头号原因**。下表把每个文件唯一归属到一个工作流，避免两个阶段同时改同一文件。

> ⚠️ **2026-08-03 修正**：本表原把 UI 文件 Owner 指给 `13F-R2`，但**主仓库的 `TASKS_13F` 只有 R0/R1，没有 R2**（A：状态行为 `R0 COMPLETE / R1 IN PROGRESS`）。R2 那 17 张 UI 拆分卡只存在于未合入的 s14 文档中。故 UI 文件 Owner 改为「**待 UI 拆分专项立项**」——在专项成立前，这些文件适用门禁 G2（只减不增），但不安排主动拆分。详见 `INT_14` §2 C-6。

| 文件 | 行数 | Owner 工作流 | 拆分目标 | 优先级 |
|---|---:|---|---|:--:|
| `UiSmokeTestRunner.cpp` | 6963 | **待 UI 拆分专项立项**（可参考 s14 R2-08D1..D5）| 按场景组 → `services/smoke/*.cpp` | **P0（阻塞于立项）** |
| `MainWindow.cpp` | 3659 | **待 UI 拆分专项立项** | 窗口骨架 / 菜单动作 / 面板协调 / 对话框 分离 | **P0（阻塞于立项）** |
| `SceneDocument.cpp` + `.h` | 1211+543 | **待 UI 拆分专项立项** | 收窄头；文档状态与视图数据分离 | **P0（阻塞于立项）** |
| `SceneViewGeometry.cpp` | 624 | **待 UI 拆分专项立项** | 同上 | P1 |
| `slicer.cpp` | **5157** | **Stage 14B / `CLAUDE_09` R-B** | orchestration ／ core compute ／ compose ／ package-report 四簇 → `steps/` | **P0** |
| `model.cpp` | 1970 | Stage 14B | 解析下沉 `importers/`；保留聚合与报告 | P1 |
| `config.cpp` | 1168 | Stage 14B | 填充 `config/NormalizedConfig`（现 6 行空壳）；解析/校验/迁移分离 | P1 |
| `SceneLayerComposer.cpp` | 1163 | Stage 14B | 头仅 16 行 → 匿名 ns 自由函数易拆，成本最低 | **P0（低成本高收益）** |
| `MultiModelProductionService.cpp` | 1126 | Stage 14B | 上提为 `orchestration/` | P1 |
| `MultiModelScene.cpp` | 1099 | Stage 14B | 场景操作 / 序列化 / 校验 分离 | P2 |
| `RgbwsvPackageWriter.cpp` | 1022 | **03D-LIBTIFF**（不得由 14B 动）| 见 §5 冲突说明 | P1 |
| `TiffLayerSource.cpp` | 1188 | **Stage 14B-10/11** | 解码 / 缓存 / 缩放 三分 | P1 |
| `LayerPreviewPanel.cpp` | 1164 | Stage 14B-11A..B/12 | widget / controller 分离 | P1 |
| `tiff_io.cpp` | 655 | **必修缺陷专项**（`INT_06` §5）| 🔴 **原 s14 矩阵中缺失** —— 见 §4 | **P0** |
| `multi_model_scene_matrix/Main.cpp` | 1257 | 低优先 | 测试框架化 | P3 |

---

## 4. 🔴 两个必须补进矩阵的遗漏项

### 4.1 `tiff_io.cpp` 的字对齐缺陷

s14 的拆分矩阵**完全没有 `tiff_io.cpp`**，而它有两处已确认缺陷（`INT_06` §5：溢出区未字对齐**已在生产输出中生效**、IFD 起始偏移未对齐被偶数通道数掩盖、无 `flush()`/`good()` 检查）。

**处置**：不是"拆分"问题而是"修复"问题，**优先级高于任何拆分**。因为 RIP 与 `ChannelSplitter` 用 libtiff 读我方产物，当前处于未定义行为区。

### 4.2 切片链路 cancel token

**A 级事实**：`slicer.cpp` 5157 行中 `cancel` 出现 **0 次**。

s14 的 63 张任务卡中，`14D-04` 做的是**进程级** `cancel→graceful→terminate→kill`，`13F-R2-06` 只给 **preflight** 补协作式取消点——**没有任何一张卡给切片计算循环加取消令牌**。后果是 s14 自己的验收 `D14-D-02 graceful cancel` 只能靠杀进程通过。

**处置**：作为 `steps/` 步骤化的**内建产物**——步骤边界与逐层循环天然是取消点。必须在拆分 `slicer.cpp` 的任务卡中显式列为 DoD，否则会被漏掉。

---

## 5. 与 s14 拆分矩阵的一处冲突（已核实）

s14 内部存在一处矛盾：`TASKS_14` §10 把 `RgbwsvPackageWriter.cpp` 分配给 `03D + 14B-06`，但 §5 定义 `14B-06` 为"包装 slice/package.verify facade，**不改变 Writer 或协议**"。**即：把文件交给了一个被禁止改它的任务。**

**我方处置**：`RgbwsvPackageWriter.cpp` 的 Owner 归 **`03D-LIBTIFF`**（该工作流本就要动 TIFF 写出），14B 只做 facade 包装、不碰 writer。已在 §3 矩阵中体现。

---

## 6. 出口门与验收

每个拆分任务的统一出口门：

```text
□ characterization test 先行（拆前先固化当前行为）
□ 生产 TIFF 逐字节不变（RepairDisabled SHA-256）
□ RIP strict 通过
□ 相关单测全绿；UI 改动加 self-test + overlay smoke
□ 依赖方向检查通过（禁反向依赖、core 禁 Qt）
□ 单次提交不同时改算法/合同/布局
```

专项级验收：

```text
□ 门禁 G1..G5 在 CI 中生效并有白名单机制
□ >1000 行文件数从 13 降至 ≤ 8（第一轮目标）
□ UiSmokeTestRunner.cpp / MainWindow.cpp 各降至 < 1500 行
□ slicer.cpp 降至 < 2500 行且 steps/ 建立
□ tiff_io.cpp 缺陷修复 + libtiff 互操作用例入 CI
□ 空壳文件（NormalizedConfig/PipelineContext）填充或删除
```

---

## 7. 工作量估算（P）

| 项 | 估算 | 说明 |
|---|---:|---|
| 门禁 G1..G5（CI 脚本 + 白名单）| **2–3 人日** | **最先做，性价比最高** |
| `tiff_io.cpp` 缺陷修复 + libtiff 自查 + golden 重基线 | 3–5 人日 | `INT_06` §5，独立于拆分 |
| `SceneLayerComposer.cpp` 拆分 | 2–3 人日 | 72:1 头实比，匿名 ns，最易拆 |
| `slicer.cpp` characterization + 四簇拆分 | **15–25 人日** | s14 给 `14B-07A` 只估 8h，**严重低估**；5157 行 + 无取消令牌 + 需 golden 保护 |
| `config.cpp` + `model.cpp` | 6–10 人日 | |
| `UiSmokeTestRunner.cpp` + `MainWindow.cpp` + `SceneDocument` | 12–18 人日 | 归 13F-R2 |
| `TiffLayerSource` + `LayerPreviewPanel` | 5–8 人日 | 归 14B-10/11 |
| **合计** | **45–72 人日** | 分摊到各功能阶段，不单独占排期 |

---

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-02 | v1.0 | 首版。实测 52 个 >500 行、13 个 >1000 行；发现 UI 两文件三月内净增 6393 行、`slicer.cpp` 在 `pipeline/` 建立后不降反增；提出"门禁止血 + 按边界拆分"两条腿；吸纳 s14 的拆分纪律与单 Owner 治理；补入 s14 遗漏的 `tiff_io.cpp` 与 cancel token；修正 s14 关于 `RgbwsvPackageWriter.cpp` 的 Owner 矛盾；指出 `14B-07A` 8h 估算严重低估 |
