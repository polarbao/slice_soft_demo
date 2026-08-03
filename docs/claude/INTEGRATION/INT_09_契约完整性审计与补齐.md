# INT_09 契约完整性审计与补齐清单

> 目录：`docs/claude/INTEGRATION/`。日期：2026-08-02。视角：切片软件构建者。
> 回答：**打印软件与切片功能包之间的契约是否已全部明确？切片侧是否有对应的 API/SPI 文档？逻辑是否闭合？**
> 证据等级：A=已核实事实，B=打印侧/第三方文档，P=本方判断。

---

## 0. 审计结论

**主干完整，但有 7 处缺口，其中 3 处会直接导致集成返工。**

| 维度 | 结论 |
|---|---|
| 调用契约（SPI 机制层）| ✅ **完整**。11 个导出、`.def`、`__cdecl`、`/MD`、缓冲三态、运行时装载 —— 两侧一致且是全部相关文档中**唯一有真实函数签名**的一层 |
| 数据契约（四接缝）| ✅ **完整**。S1/S2/S3/S4 均有名有版本有校验器 |
| 错误码 | ✅ 完整。`PM-<MODULE>-<CATEGORY>-<CODE4>`，切片侧 15 条已定 |
| **交互契约（拖拽/提交/生产）** | 🔴 **缺失**。见 G-1 |
| **包查询与预览契约** | 🔴 **缺失**。见 G-2 |
| **取消语义** | 🔴 **缺失且两侧都漏**。见 G-3 |
| 同步/异步快路径 | ⚠️ 隐含未言明。见 G-4 |
| 大缓冲传输 | ⚠️ 只说"不过 ABI"，无协议。见 G-5 |
| 契约物料（可发布头/schema）| ⚠️ 只有文档，无 `contracts/` 实体。见 G-6 |
| 幂等与重试 | ⚠️ 未定义。见 G-7 |

---

## 1. 已闭合的部分（无需再补）

| 契约项 | 切片侧文档 | 打印侧文档 | 状态 |
|---|---|---|---|
| SPI 11 导出 + 生命周期 | `INT_07` §2 | `CLD_05` §3 / `CLD_10` §2 | ✅ 一致 |
| 缓冲三态 + `out_required` | `INT_07` §2.5 | `CLD_10` §4 | ✅ 一致 |
| ABI 六条硬约束 | `INT_07` §2.4 | `CLD_10` §3 | ✅ 一致 |
| 装载方式（运行时 `GetProcAddress`）| `INT_08` §0 | `CLD_10` §8.4a | ✅ 一致 |
| `module.json` schema | `INT_07` §2.3 | `CLD_10` §12 | ✅ 一致 |
| C-SPI-01..18 一致性套件 | `INT_07` §3.2 | `CLD_10` §13 | ✅ 采纳对方 |
| S1 `p0.rgbwsv.2` | `INT_02` §5 | `CLD_04` §6.3 | ✅ 已冻结 |
| S2 `rip.ch7.1` + 墨滴上限 | `INT_03` / `INT_08` §0 | `CLD_06` §8.2.1 | ✅ 一致 |
| S3 `printdata.v1` | `INT_08` §0 | `CLD_19` | ✅ 采纳对方 |
| S4 `ReadyTicket` | `INT_08` §1.2 | `CLD_05` §7 | ✅ 采纳对方 |
| 能力清单（方案 C）| `INT_06` §1 | `CLD_27` §5 | ✅ 一致 |
| 错误码规范 | `INT_02` §6 | `CLD_05` §6 | ✅ 一致 |
| 安全发布 `.staging`→原子改名 | `INT_02` §8 | `CLD_05` §12 | ✅ 两侧既有实现一致 |

> 一个值得记录的判断（P）：**在所有相关文档中（含 s14 的 8 篇），只有切片侧与打印侧的 SPI 文档给出了真实函数签名。** s14 的 8 篇文档合计约 1400 行，C ABI 内容仅 1 个 struct + 2 个 typedef，且自标"示意，不是已冻结头文件"。因此 SPI 机制层**必须以我方 `INT_07` + 打印侧 `CLD_10` 为准**，不可被规划类文档覆盖。

---

## 2. 七处缺口与补齐方案

### G-1 🔴 交互契约缺失：Transient / Commit / Production 三车道

**问题（P）**：现有 SPI 是**作业形态**（`pm_submit`/`pm_poll`/`pm_result`）。它没有回答一个必然出现的场景——**用户以 60fps 拖拽模型时怎么办？** 若每帧跨 DLL 调用 `scene.transform`，手感必然崩坏。

**补齐（吸纳 s14 `DOC_DECISION_14` §4.2，这是该文档最有价值的贡献）**：

```text
① Transient lane（瞬时车道）
   拖拽/缩放/旋转过程中，宿主只改本地临时矩阵 + 本地缓存 viewdata
   → 不逐帧跨 DLL。模块对此一无所知，也不需要知道

② Commit lane（提交车道）
   鼠标释放 / 数值确认 / Undo-Redo / 自动定向 / 排版 → 一次提交
   请求必带：operationId + currentSceneRevision + expectedSceneRevision
   返回必含：newRevision + sceneHash + canonicalTransform + issues + viewdataIdentity
   revision 不符 → SceneRevisionStale（PM-SLICER-LAYOUT-0022），fail-closed，不静默覆盖
   同一 operationId 重试必须幂等

③ Production lane（生产车道）
   切片只接受【已提交的 sceneHash】，并在 Worker 内重新跑完整 strict preflight
   transient 状态 / 过期 revision / 未确认变换 → 一律不得进入生产
```

**配套红线（P）**：模块可以用 `scene_handle` 做缓存，但**任何 handle 状态必须能由版本化的 `CanonicalSceneSnapshot` 重建**——handle 不得成为不可恢复的第二套持久化真源。

**可验证判据（吸纳 s14 `DEMO_14`）**：联调时采集 `Host→DLL` 调用次数，**证明 mouse-move 不跨 DLL**。这是一条可证伪的验收，比"手感良好"强得多。

### G-2 🔴 包查询与预览能力缺失

**问题（P）**：方案 C 的能力集止于 `package.verify`。但打印软件必须**展示**切片结果——层滑块、单通道 R/G/B/W/S/V、叠加预览、per-instance 统计。**若不提供，宿主必然自行解码 TIFF 并自行做材料合成，语义必然走偏**（这正是我方一直反对的"第二套真值"）。

**补齐（新增 4 项能力）**：

| 能力 ID | 职责 | 承载 |
|---|---|---|
| `package.get_summary` | 返回层数、网格、通道、per-instance 统计、profileEcho | 进程内 |
| `package.get_layer_descriptor` | 单层元信息（zMm、尺寸、各通道打印像素数）| 进程内 |
| `package.render_layer_preview` | **由生产 TIFF 解码/合成**出预览位图（单通道或叠加）| 进程内 |
| `package.read_report` | 读取 `reports/*.json` 的结构化投影 | 进程内 |

**红线（吸纳 s14 `DEV_14` §8.2）**：

```text
预览必须从生产 TIFF / Package 真源解码或合成，不得依赖仅供调试的 preview PNG
widget 不直接解析 TIFF、不决定材料合成
非方形 DPI 必须按物理比例显示（本项目 X=635/720、Y=600 不相等）
cache key 必须含：package identity + layer + mode + channel + 尺寸/LOD + 语义版本
```

> 我方 `preview/TiffLayerSource.cpp`（1188 行）已实现 TIFF 原生层数据源与 LRU（13C-01/02 已完成），**这 4 项能力是把既有资产开个出口，不是新建**。

### G-3 🔴 取消语义缺失 —— 两侧都漏，且是最深的洞

**A 级事实**：`slicer.cpp`（5157 行）中 **`cancel` 出现次数为 0**。切片计算链路**没有任何取消令牌**。

现有各方的取消设计都停在**进程级**：

| 来源 | 取消设计 | 覆盖层次 |
|---|---|---|
| 我方 `INT_07` | 子进程承载 + 杀进程兜底 | 进程级 |
| 打印侧 `CLD_38` DEC-S3 | 杀子进程 + 必须清理 `.staging` | 进程级 |
| s14 `14D-04` | `cancel→graceful→terminate→kill` 状态机 | 进程级 |
| s14 `13F-R2-06` | 给 **preflight** 补协作式取消点 | 仅预检 |

**结论（P）**：**没有任何一方给切片计算循环加取消令牌。** 后果是 s14 自己定义的验收 `D14-D-02 graceful cancel` **只能靠杀进程通过**，13F 那条"terminate 后 1 秒未退出则 kill"会从兜底变成常态路径。

**补齐（两条，缺一不可）**：

1. **契约层**：`pm_cancel` 的语义必须明确区分状态 ——

```text
pm_cancel 返回 PM_OK 只表示【取消请求已受理】，作业进入 Cancelling
必须等真实退出（进程退出 / 计算循环退出）后才可置为 Cancelled
取消后 .staging 必须不存在（C-SPI-09）
```

> 这条来自 13F 的真实教训（A 级）：旧实现调用 `QProcess::terminate()` 后**立即**把控制器标记为已取消，而 Windows 控制台进程可能仍在运行 → 日志继续输出、`m_processBusy` 不释放、切片按钮无法恢复。**契约不写死这条，宿主就会重犯同样的错。**

2. **实现层**：在 `steps/` 步骤化时把 **cancel token 贯穿切片链路**，每个 step 边界 + 逐层循环设协作式取消点。这正是 `CLAUDE_09` R-B 的天然收益——**步骤边界就是取消点**。

### G-4 ⚠️ 同步快路径未言明

**问题**：s14 `14C-04` 要求"实现**同步**轻能力 C ABI：import/preflight/scene operation/layout/viewdata/verify"。我方 SPI 只有 `pm_submit`/`pm_poll`/`pm_result` 异步三件套。二者可调和，但**从未写明**。

**补齐（明确决策，P）**：**不新增 `pm_call`**，改为在契约中写明：

```text
轻能力（capability 标注 "sync": true）在 pm_submit 内部同步完成
→ 首次 pm_poll 即返回终态，pm_result 立即可读
→ 宿主可写成 submit→poll→result 的直线调用，无需轮询循环
capabilities.syncCapabilities[] 显式列出哪些是同步的
```

理由：保持**单一调用模型**（11 导出不变），同时给宿主可预期的同步语义。这是"平滑 gizmo 与卡顿 gizmo 的差别"，必须显式而非隐式。

### G-5 ⚠️ 大缓冲传输无协议

**补齐（吸纳 s14）**：

```text
禁止：大图像编码进 JSON / Base64
方式一（默认）：两次调用 —— 先查所需尺寸，再由调用方分配缓冲
方式二（大数据）：模块写只读共享映射 / 临时文件，JSON 只回路径与标识
所有大缓冲必须带 identity（package + layer + mode + channel + size/LOD + 语义版本）
```

### G-6 ⚠️ 契约只有文档，无可发布物料

**问题（P）**：两侧都在文档里写契约，但仓库里**没有 `contracts/` 实体**。打印侧 `CLD_04` §5.8 已规划部署目录含 `contracts/`（4 份 schema + `print_module_spi.h`），但**尚未落盘**。

**补齐**：在切片仓库建立并维护：

```text
contracts/
├─ print_module_spi.h          ← 与打印侧逐字节相同（同一份文件，两仓库同步）
├─ p0.rgbwsv.2.schema.json     ← 我方产出，用真实 manifest 校验
├─ slicer.capabilities.json    ← 我方能力自述样例
└─ file_contract_v1.md         ← 子进程协议（进度行 + 退出码表）
```

**规则**：`print_module_spi.h` **以打印侧为准**，我方只同步不改；其余三份**以我方为准**。

**🔴 唯一跨边界头文件规则（P）**：

```text
contracts/print_module_spi.h 是【唯一】跨越模块边界的头文件
  ✓ 只含 C 基本类型、const char* / char*、不透明句柄、宏
  ✗ 不得出现 slicer_core/api/ 的任何 C++ 类型（STL 容器、类、模板、异常）
  ✗ 不得出现 Qt 类型
理由：slicer_core 被静态链接进 DLL 与 Worker（见 INT_10 §1.1），
      其 C++ 类型只在各模块【内部】流动；一旦泄漏到该头文件，
      跨 DLL 的 CRT/ABI 不匹配将直接导致崩溃。
```

打印软件的构建系统**只需要这一个头文件**——不需要 slicer 的任何 include 路径，不链接任何 `.lib`（运行时装载）。

### G-7 ⚠️ 幂等与重试未定义

**补齐**：

```text
所有带 operationId 的调用：同一 operationId 重试必须幂等
jobId 重复提交：幂等返回既有作业，或明确拒绝，identity 不得混淆
（对应 s14 D14-D-07：duplicate jobId / stale result）
```

---

## 3. 补齐后的完整能力清单（切片侧对外，v2）

在 `INT_06` 方案 C 的 5+2 基础上，补 G-1/G-2 带来的能力：

| 组 | 能力 ID | 同步 | 承载 | 来源 |
|---|---|:--:|---|---|
| 模型 | `model.import` | ✅ | 进程内 | 方案 C |
| 模型 | `model.get_metadata` | ✅ | 进程内 | ➕ s14 |
| 模型 | `model.release` | ✅ | 进程内 | ➕ s14 |
| 场景 | `scene.apply_operation`（Commit 车道）| ✅ | 进程内 | ➕ **G-1** |
| 场景 | `scene.get_snapshot` | ✅ | 进程内 | ➕ G-1 |
| 场景 | `scene.get_viewdata` | ✅ | 进程内 | 方案 C 可选项转正 |
| 几何 | `geometry.preflight`（fast/full）| fast ✅ | 进程内/Worker | 方案 C |
| 几何 | `geometry.collision`（碰撞/越界真值）| ✅ | 进程内 | `INT_06` §1 |
| 几何 | `geometry.repair`（默认关）| ❌ | Worker | 方案 C 可选 |
| 切片 | `slice.rgbwsv` | ❌ | **Worker** | 方案 C |
| 包 | `package.verify` | ✅ | 进程内 | 方案 C |
| 包 | `package.get_summary` | ✅ | 进程内 | ➕ **G-2** |
| 包 | `package.get_layer_descriptor` | ✅ | 进程内 | ➕ G-2 |
| 包 | `package.render_layer_preview` | ✅ | 进程内 | ➕ G-2 |
| 包 | `package.read_report` | ✅ | 进程内 | ➕ G-2 |

**仍然不提供**：`scene.layout` 的**摆放策略**（packing 引擎）、设置/Profile、渲染、RIP、通道化、作业队列、任何 Qt 类型。

> **关于 `scene.layout` 的最终立场（P）**：s14 把它放进模块，我方原本排除。**两边各对一半**——碰撞与越界是几何真值，绝不能让宿主重造（这点 s14 对）；但**摆放策略**（几列几行、间距、packing 算法）的全部输入（buildVolume/origin/axes）都归宿主，且它是会随产品迭代频繁变更的业务决策（这点我方对）。**结论：以 `geometry.collision` 出口提供真值，packing 引擎留在宿主。** 这样既不分裂真值，也不把 UX 决策锁进 DLL。

---

## 4. 逻辑闭合性自检

| 问题 | 是否闭合 | 依据 |
|---|:--:|---|
| 宿主如何发现并装载模块？ | ✅ | `module.json` + `GetProcAddress` + 10 步校验（`CLD_10` §10.3）|
| 版本不匹配怎么办？ | ✅ | 双版本轴 + 兼容矩阵 + fail-closed |
| 用户拖拽时怎么保证不卡？ | ✅（补 G-1 后）| 三车道 + 本地 transient |
| 变换后判定不一致怎么办？ | ✅ | 权威求值在模块 + `SceneRevisionStale` |
| 切片崩溃会不会拖垮打印？ | ✅ | Worker 进程隔离 |
| 取消能不能及时且干净？ | ✅（补 G-3 后）| Cancelling≠Cancelled + cancel token + staging 清理 |
| 宿主如何展示切片结果？ | ✅（补 G-2 后）| `package.*` 四项能力 |
| 半成品会不会被当成品？ | ✅ | staging→自检→原子发布 + 校验器拒绝 `.staging` |
| 参数漂移怎么防？ | ✅ | ProfileService 单一真源 + `profileHash` 原样回写 + S2-C7 |
| 6ch 怎么变 7ch？ | ✅ | RIP 承担分色 + 墨滴量化（`OPEN-01` 待签字）|
| 谁批准可以打印？ | ✅ | S4 `ReadyTicket` + `ReadyGate` |
| 模块缺失能否启动？ | ✅ | 运行时装载 + `D14-E-03` 验收 |

**结论（P）**：补齐 G-1..G-7 后，从"导入模型"到"允许打印"的**全链路契约逻辑闭合**，无悬空环节。

---

## 5. 待办与责任

| 编号 | 补齐项 | 责任 | 时机 |
|---|---|---|---|
| G-1 | 三车道 + `operationId` 幂等 + 快照重建规则 | 切片侧起草 → 两侧确认 | 契约冻结前 |
| G-2 | `package.*` 四项能力规格 | 切片侧 | 契约冻结前 |
| G-3a | `pm_cancel` 的 Cancelling≠Cancelled 语义写入契约 | 两侧 | 契约冻结前 |
| G-3b | **切片链路 cancel token**（实现） | 切片侧 | 随 `steps/` 步骤化 |
| G-4 | `syncCapabilities[]` 声明 | 切片侧 | 契约冻结前 |
| G-5 | 大缓冲传输协议 | 切片侧起草 | 契约冻结前 |
| G-6 | `contracts/` 落盘 | 切片侧（`spi.h` 同步打印侧）| 契约冻结前 |
| G-7 | 幂等规则 | 两侧 | 契约冻结前 |

---

## 6. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-02 | v1.0 | 首版。审计出 7 处契约缺口；吸纳 s14 的三车道模型、包查询预览能力、大缓冲与预览缓存规则；指出取消令牌是两侧共同盲区；给出补齐后的 v2 能力清单与逻辑闭合自检 |
