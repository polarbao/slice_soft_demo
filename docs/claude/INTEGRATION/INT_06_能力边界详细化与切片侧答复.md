# INT_06 切片侧能力边界详细化与对打印侧的正式答复

> 目录：`docs/claude/INTEGRATION/`。日期：2026-07-28。视角：**切片软件（`slice_soft_demo`）构建者**。
> 对接对象：打印侧 `ry_print_demo/docs/claude/` 的 `CLD_04 / CLD_05 / CLD_06 / CLD_10 / CLD_27 / CLD_07`。
> 证据等级：A=已核实代码事实，B=对方正式文档，P=本方判断/承诺。
> **本篇是切片侧的正式答复函**：回答 `OPEN-27-04`、逐条答复 `SL-01..SL-10`，并把能力边界从"粗略清单"落成"可实现规格"。

---

## 0. 三句话结论

1. **采纳打印侧 CLD_27 的方案 C（几何真值包）**：首版对外提供 **5 项必需 + 2 项可选**能力，**不提供 `scene.layout`**、不提供任何设置/Profile 能力。
2. **`OPEN-27-04` 答复：这 5 项切片侧都能以 SPI 形式提供**，其中 3 项是现成能力（导入/预检/包校验），2 项需薄壳适配（变换求值/切片）。
3. **切片侧承认 5 个必须由我方修复的缺陷**，其中 **TIFF 字对齐缺陷已在生产输出中处于未定义行为区**（打印侧 CLD_10 §7.3 读我方代码发现），必须优先修，且会导致一次**受控的 golden 重基线**。

---

## 1. 对 `OPEN-27-04` 的正式答复

> 打印侧问：**切片侧能否以 SPI 形式提供方案 C 的这 5 项能力？**

**答复：能。** 逐项确认如下（A=现有代码已具备，P=需新增薄壳）：

| 能力 ID | 切片侧现状 | 需要做什么 | 结论 |
|---|---|---|---|
| `model.import` | A：`model.cpp` 已实现 OBJ/STL/3MF/MTL/贴图解析，含材质与颜色提取 | 包 C ABI + JSON 序列化 | ✅ 可提供 |
| `geometry.preflight` | A：`preflight/ModelPreflightService`(687行) + `ModelPreflightAdmissionPolicy` + `geometry/MeshTopology/RobustnessDiagnostics` + 自交/非流形分析器 | 包薄壳；区分"快检"与"全量" | ✅ 可提供 |
| `scene.transform` | A：`scene/ModelTransform` + `SceneViewGeometry` + 变换后预检（13A-04 已完成） | 包薄壳；补 `collisions/outOfBounds` 出参 | ✅ 可提供 |
| `slice.rgbwsv` | A：主链路完整，产出 `p0.rgbwsv.2` | 包薄壳 + **子进程后端契约化** + 取消 | ✅ 可提供 |
| `package.verify` | A：`apps/rip_reader_test` / `validate_slice_package()`，23 个错误码 | 包薄壳（逻辑零改动） | ✅ 可提供，**成本最低** |

**可选 2 项**：

| 能力 ID | 结论 |
|---|---|
| `geometry.repair` | ✅ 可提供，**默认关闭**。`geometry/repair/` 已有完整分级链（preflight→eligibility→conservative→post-strict→evidence）。是否启用取决于 `SL-10` 的产品决策 |
| `scene.viewdata` | ✅ 可提供，但**同意打印侧意见：首版可不要**。`SceneViewGeometry` 已存在，但首版用 `scene.transform` 返回的 bbox 画俯视框足够；待 UI 明确精度需求（`SL-05`）再开 |

**关于 `scene.layout` 的立场（P）**：接受打印侧"排版在 PRD_21–29 零命中，不为无需求功能设计跨 DLL 接口"的判断。但需要在此**记录一个事实差异**——切片侧的 `layout/GridLayoutPolicy`（含确定性行主序、`sceneRevision` 乐观并发、7 个稳定错误码）与 `layout/SceneCollisionService` **已经实现并有测试**（Stage 13B-03/04 已完成）。

因此：

- 首版对外**不暴露** `scene.layout` SPI 能力 —— 尊重方案 C；
- 但切片侧**内部保留**该模块（Stage 13 需要它做联合切片的实例摆放求值）；
- **碰撞与越界判定通过 `scene.transform` 出参对外提供**（这是打印侧要的"真值"），摆放决策留给宿主；
- 将来若产品确认排版需求，**打开一个已实现模块的 SPI 出口即可**，成本远低于打印侧自建。

> 净效果：方案 C 的能力面 = 打印侧要的最小真值集；切片侧不因收缩而丢弃既有资产。

---

## 2. 能力边界详细化（逐能力规格）

以下把每项能力从"一行说明"落成"可实现规格"。统一约定：

```text
调用形态   pm_submit(request_json) → pm_poll → pm_result（统一 SPI，见 CLD_05 §3）
承载       inprocess = DLL 内直接算；subprocess = 派发 slicer_worker.exe
错误码     PM-SLICER-<CATEGORY>-<CODE4>（码段分配见 CLD_05 §6.3）
Profile    模块不自带业务默认值；缺关键项 → PROFILE-0030；越界 → PROFILE-0031（fail-closed，不钳制）
```

### 2.1 `model.import`

| 项 | 规格 |
|---|---|
| **职责** | 把模型文件解析为场景模型 DTO，提取几何、UV、材质、颜色、bbox |
| **反职责** | 不做拓扑裁决（那是 preflight）、不做变换、不落盘产物 |
| **输入** | `{ capability, modelPath, options:{ computeBBox:true, extractMaterials:true } }` |
| **输出** | `{ modelId, triangleCount, vertexCount, hasUV, hasNormals, materials[{name,diffuseRgb,texturePath}], bboxMm{min[3],max[3]}, units, sourceDigest }` |
| **不变量** | 同一文件 + 同一版本 → `sourceDigest` 稳定（作为宿主缓存键组成，对齐 DEV_51 缓存键设计） |
| **失败** | `INPUT-0001` 不存在/不可读；`INPUT-0002` 格式不支持/解析失败 |
| **承载** | 进程内。**大模型例外**：`triangleCount > 阈值`（见 `SL-06`）时建议 subprocess |
| **性能画像** | 参考：`meigui_fudiao/04.obj` 76,926 面 / 228,991 顶点，属"中等"量级 |
| **Profile keys** | 无（纯解析，不需业务参数） |
| **测试** | 正例每格式各 1；负例：损坏文件、缺 MTL、缺贴图、无 UV、空网格 |

### 2.2 `geometry.preflight`

| 项 | 规格 |
|---|---|
| **职责** | 尺寸、封闭性、自交、非流形、绕向、越界的**唯一裁决者**；产出准入结论 + 可读 issues |
| **反职责** | 不修复（那是 `geometry.repair`）、不静默降级、不因宿主要求而放宽 strict |
| **输入** | `{ capability, modelId | modelPath, mode:"fast"|"full", scene?:{...}, buildVolume?:{...} }` |
| **输出** | `{ admission:"passed"|"blocked"|"manual_repair_required", issues[{code,severity,count,detail}], topology:{boundaryEdges,nonManifoldEdges,selfIntersectionPairs,isClosed}, bboxMm, outOfBounds:bool }` |
| **不变量** | `admission=="passed"` ⟺ 可进入切片；`manual_repair_required` **永不等于 pass**（红线） |
| **失败** | `TOPOLOGY-0010` strict 阻断；`TOPOLOGY-0011` 需人工修复 |
| **承载** | `fast` 进程内（交互路径，目标 <500ms）；`full` subprocess（自交分析是重计算） |
| **已知事实（A）** | 三个必需 OBJ 全部 blocked：`nai_you_new` boundaryEdges=113；`aishen_fudiao` boundaryEdges=3/nonManifold=59；`meigui_fudiao` nonManifold=10940；R3-02 自交对数 8409/19270/5592 |
| **Profile keys** | `buildVolume.*`（由宿主提供，只消费不产生） |
| **测试** | generated fixture 精确触发每类问题 + 三个真实 OBJ 作为 blocked 基线 |

> ⚠️ **`fast` 与 `full` 的一致性红线（P）**：`fast` 只允许**漏报**不允许**误报为通过**——即 `fast=passed` 但 `full=blocked` 是不可接受的。实现上 `fast` 必须是 `full` 判据的子集，且切片提交前**必须**跑过一次 `full`。否则会出现"UI 说能打、切片才拒"。

### 2.3 `scene.transform`（方案 C 的核心，承接原 layout 的真值部分）

| 项 | 规格 |
|---|---|
| **职责** | 变换的**权威求值**：变换后 bbox、实例间碰撞、幅面越界、变换后可打印性增量 |
| **反职责** | 不决定"摆哪里"（宿主职责）、不提供拖拽手感 |
| **输入** | `{ capability:"scene.transform", scene:{...见 CLD_04 §6.1...}, expectedSceneRevision }` |
| **输出** | `{ newSceneRevision, instances[{instanceId, effectiveBBoxMm, outOfBounds:bool}], collisions[{a,b,overlapMm3?}], outOfBoundsInstances[], preflightDelta[{instanceId, admissionBefore, admissionAfter}] }` |
| **不变量** | ① 纯函数：同输入同输出；② 不修改输入场景；③ `expectedSceneRevision` 不符即拒绝，不做部分应用 |
| **失败** | `LAYOUT-0020` 碰撞；`LAYOUT-0021` 超幅面；`LAYOUT-0022` `SceneRevisionStale`；`LAYOUT-0023` 实例数超上限 |
| **承载** | 进程内（交互路径，目标 <200ms @ ≤22 实例） |
| **Profile keys** | `buildVolume.*` |
| **测试** | 碰撞/越界/版本过期/镜像后 bbox/旋转后越界 各正负例 |

> **`preflightDelta` 的设计理由（P）**：镜像与非均匀操作会改变自交/绕向表现（13A-04 已验证）。若只回 bbox 与碰撞，宿主无法知道"这个变换让模型从可打印变成不可打印"。这个出参让 UI 能在拖拽松手时就提示，而不是等到提交切片。

### 2.4 `slice.rgbwsv`

| 项 | 规格 |
|---|---|
| **职责** | 场景（1..N 实例）→ 完整 `p0.rgbwsv.2` 包 |
| **反职责** | 不做 RIP、不做通道化、不写 `printdata.v1` |
| **输入** | `{ capability, scene, output:{contract:"p0.rgbwsv.2", packageDir}, profile:{...}, options:{backend,threads} }`（字段与 CLD_04 §6.2 一致） |
| **输出** | `{ packageDir, manifestPath, layerCount, grid{...}, perInstance[{instanceId, layerRange, printPixels{...}}], reports{...} }` |
| **不变量** | ① 安全发布：`.staging` → 自检 → 原子 rename；② 产物必过自身 `package.verify`；③ 协议字段不变（红线） |
| **失败** | `PROFILE-0030/0031`、`RESOURCE-0040`、`OUTPUT-0050`、`CONTRACT-0060`、`CANCELLED-0070` |
| **承载** | **subprocess（默认）**。理由：内存 8.19–8.74×（Global）、主链路当前不可取消、病态网格崩溃风险 |
| **Profile keys** | `output.*`、`slicingMode`、`texture.*`、`support.*`、`materialClosure.*`、`autoOrient.*` |
| **进度 stage** | `import → preflight → layout → grid → mask → texture → support → compose → write → report` |
| **测试** | 单实例/多实例正例；取消（各 stage 各一次）；磁盘满；产物 SHA-256 可复现 |

### 2.5 `package.verify`

| 项 | 规格 |
|---|---|
| **职责** | S1 接缝校验器：校验 `p0.rgbwsv.2` 包结构、schema、通道、位深、极性、层列表、存储模式 |
| **输入** | `{ capability, packageDir }` |
| **输出** | `{ valid:bool, errors[{code,message}], perLayerChecksum[[u64 x6]], layerCount }` |
| **实现** | **直接复用** `validate_slice_package()`，23 个错误码，逐层 6 通道 `uint64` checksum |
| **承载** | 进程内 |
| **测试** | 正例 + 7 类负例（通道数/位深/顺序/缺层/schema/极性缺失/`.staging` 半成品） |

> ⚠️ **一个必须共同承认的事实（P）**：`package.verify` 复用后，我方 `rip_reader_test` 的 **23 个错误码与逐层 checksum 就成了事实上的对外契约面**。今后修改它们等同于修改接缝契约，需走契约变更流程，不能当内部重构随意改。

### 2.6 `geometry.repair`（可选，默认关闭）

| 项 | 规格 |
|---|---|
| **职责** | 保守修复 → 修复后重新 strict → 产出证据 |
| **红线** | 默认关闭；`manual_repair_required` 不算 pass；confirmed self-intersection **fail fast**，不得修前放行 |
| **承载** | subprocess |
| **启用条件** | 取决于 `SL-10` 的产品决策 |

---

## 3. 横切规格（所有能力共用）

### 3.1 资源与所有权

```text
字符串     UTF-8；调用方分配缓冲；(char* out, int cap, int* out_required) 三态协议（CLD_10 §4）
句柄       不透明；pm_create/pm_destroy、pm_submit/pm_release 必须成对且同一 DLL 内完成
大数据     不过 ABI。层图/包一律走文件路径；模块只在 options.paths.tempDir 下创建临时文件
异常       不得越过 DLL 边界；内部异常一律转错误码
```

### 3.2 线程与取消

```text
pm_module_t   允许多线程并发 pm_submit（内部加锁）
pm_job_t      单线程操作；但允许 A 线程 poll + B 线程 cancel（取消标志 std::atomic<bool>）
DllMain       只 return TRUE；一切初始化放 pm_create（std::call_once）
pm_release    对 running job 必须先取消并 join，不得直接杀线程
取消延迟      目标 ≤2000ms（对齐 C-SPI-08），取消后 .staging 必须不存在（C-SPI-09）
```

### 3.3 日志与可追溯

```text
日志目录   options.paths.logDir（宿主指定，模块自有子目录，便于诊断包整目录收集）
关联字段   correlationId 全链路透传并写入每条日志
libtiff    我方 tiff_io 手写 IFD 不依赖 libtiff；若将来引入，须在 pm_create 装 handler、pm_destroy 恢复
```

### 3.4 幂等与缓存键

建议宿主缓存键（与 DEV_51 一致，切片侧配合提供各分量）：

```text
sourceDigest + normalizedTransform + deviceProfileVersion + algorithmVersion + effectiveProfileVersions
                                                            ↑ 由 pm_module_info.version 提供
```

---

## 4. 逐条答复 `SL-01..SL-10`

| 编号 | 打印侧诉求 | 切片侧答复（P） | 承诺 |
|---|---|---|---|
| **SL-01** | 实现 SPI 薄壳，产出 `slicer_module.dll` + `module.json`（M1 门禁） | ✅ **接受**。严格按 CLD_10：`PM_API` + `PM_CALL __cdecl` + `.def` 恰好 11 个 `pm_*` + `/MD`(Release)/`/MDd`(Debug) + `DllMain` 只 `return TRUE` + 运行时装载（**不提供 import .lib、不用 `/DELAYLOAD`**） | 见 `INT_07` |
| **SL-02** | 子进程后端契约化（stdout 进度行 + 退出码固化） | ✅ **接受**。现有格式 `SLICE_PROGRESS phase=… current=… total=… percent=… elapsedMs=…` 与 `SLICE_TIMING …` **冻结为契约**，纳入 `file_contract_v1`；退出码表随附 | M0-10 前交付 |
| **SL-03** | 主链路取消支持 | ⚠️ **分两步**。首版：subprocess 承载 + 杀进程兜底，**并保证 `.staging` 被清理**（由 worker 启动时清理 + 宿主启动时兜底清理双保险）。二版：在 `SliceRunOptions` 引入 cancel token，随 `CLAUDE_09` R-B 步骤化落地（step 边界是天然取消点） | 首版 M1；完整 R-B |
| **SL-04** | per-instance 统计字段清单（TBD-S1） | ✅ **提供**：`{instanceId, modelId, layerRange:[from,to], printPixels{R,G,B,W,S,V}, emptyPixels{...}, bboxMm, transformApplied}`。若宿主还需成本核算量（材料体积），请指定单位与口径 | M3 门禁前 |
| **SL-05** | `scene.viewdata` 精度与形式（TBD-S2） | ⚠️ **建议首版不要**（同 CLD_27）。若需要，我方可提供两档：① 俯视外轮廓折线（容差可配，默认 0.1mm）；② 降采样三角缓冲。**请 UI 先定渲染方案再定档位** | M3 门禁前 |
| **SL-06** | 实例数上限与内存画像（TBD-S3） | ⚠️ **需实测**。当前仅有 Global 相对倍数（4.09–5.92× 慢 / 8.19–8.74× 内存）与 22 实例阈值（来自 Stage 13 产品设定），**缺绝对内存曲线**。承诺在 M2 期间产出 1/4/11/22 实例的耗时与峰值内存表，供 `backend=auto` 标定 | M2 期间 |
| **SL-07** | 是否允许 mixed-profile（TBD-S4） | ✅ **P0 仅 `scene_profile_only`**，不一致即 fail-closed（`PROFILE-0030`）。与打印侧建议一致 | 即刻确认 |
| **SL-08** | manifest `layers` 与 `tiff.layers` 重复的权威来源 | ✅ **确认：以 `tiff.layers` 为权威**（语义更明确）。顶层 `layers` 保留以兼容既有消费者，但标注为镜像。我方将在 manifest 写入 `"layersMirror": true` 提示，并保证两处一致；不一致时视为我方 bug | M0-02 前 |
| **SL-09** | `--no-production-rgbwsv` flag 实际无效果 | ✅ **确认为文档一致性缺陷**。该 flag 与默认值 `write_production_rgbwsv=false` 重复。将在 CLI 帮助与文档中标注为 no-op 并计划移除 | M1 |
| **SL-10** | 三个必需 OBJ 被 strict 阻断，是否提供 repair 路径 | ⚠️ **技术上可提供，但这是产品决策，且我方建议不靠 repair 解**。理由见下 §4.1 | 需你决策 |

### 4.1 对 `SL-10` 的展开建议（P）

打印侧已判定这是"唯一无法用桩或 fixture 解耦的阻塞"。切片侧的专业意见：

| 选项 | 评价 |
|---|---|
| ① 启用 `geometry.repair` 保守修复 | ⚠️ 可尝试，但 `meigui_fudiao` 非流形边 10940 + 自交 5592 对，属**重度病态**，保守修复大概率只能降级为 `manual_repair_required`，仍不算 pass |
| ② 外部修模（Meshmixer/Netfabb/3-matic 等）后入库 | ✅ **推荐**。一次性成本可控，且产出可审计的"strict-PASS 资产"，同时建立资产入库标准 |
| ③ 换用已 strict-PASS 的模型做集成联调 | ✅ **推荐并行**。我方已有 7 个 strict-PASS 资产（5 个 `xiao_ma_wu_yu_new` + `yecan/3.obj` + `yecan/4.obj`），**足以支撑 M1–M5 全链路联调**，不必等三个必需 OBJ |
| ④ 放宽 strict 准入 | ❌ **禁止**。违反双方共同红线 |

> **关键建议（P）**：把"集成联调"与"三个必需 OBJ 可打印"**解耦**。用 ③ 立刻开工，用 ② 并行治理资产。这样 M2/M3 的 E2E 不被产品决策阻塞——这与打印侧用 `passthrough_rip` 桩解耦 RIP 的思路完全同构。

---

## 5. 切片侧必须修复的 5 个缺陷（我方自认）

打印侧 CLD_10 §7 通读了我方 `tiff_io.cpp` 并发现了两个真实缺陷。切片侧确认并接受，列为必修项。

| # | 缺陷 | 位置（A） | 严重度 | 影响 |
|---|---|---|:--:|---|
| **F-1** | **溢出区未字对齐（已在生产输出中生效）** | `tiff_io.cpp:118-121` | 🔴 高 | tag 270 `ImageDescription` 写 `"RGBWSV"` = **7 字节（奇）**，导致紧随的 tag 273 `StripOffsets`（LONG 数组）落在**奇偏移**。TIFF 6.0 要求 value 起始于偶偏移。我方读取器逐字节 `read_u32` 故无感知，但**下游 `ChannelSplitter` / RIP 用 libtiff**——虽多数平台容忍，但属**未定义行为** |
| **F-2** | **IFD 起始偏移未对齐（当前被偶数通道数掩盖）** | `tiff_io.cpp:404` | 🔴 高 | `ifd_offset = header_size + data.size()`，无补齐。`samples_per_pixel=6`（偶）掩盖了它；一旦出现奇数通道或奇数像素数即暴露。举例：71×71=5041（奇）×6=30246（偶，侥幸）；×7=35287（奇）→ 偏移为奇 |
| **F-3** | 写入后无 `flush()` / `out.good()` 检查 | `tiff_io.cpp` 写出路径 | 🟡 中 | 磁盘满时**静默产出截断文件**，不报错 |
| **F-4** | manifest 逐层统计重复（`layers` 与 `tiff.layers`） | manifest 生成 | 🟡 中 | 消费方不知以哪处为准（=`SL-08`） |
| **F-5** | `--no-production-rgbwsv` flag 无效果 | `apps/slicer_cli/main.cpp` | 🟢 低 | 文档与实际不一致（=`SL-09`） |

### 5.1 F-1 / F-2 的修复会带来一次受控的 golden 重基线（重要，P）

这是一个必须提前讲清的连带后果：

```text
补齐字节 → TIFF 文件字节序列改变 → 现有 golden 的 TIFF SHA-256 全部变化
```

而我方既有硬门是"**生产 TIFF 逐字节不变**"（`run_material_closure_tests.ps1 -Mode RepairDisabled`）。二者直接冲突。处置建议：

| 项 | 处置 |
|---|---|
| 性质判定 | **不是协议变更**：通道顺序/位深/极性/schema 全不变，仅修正文件内字节对齐 → **无需走 G4 协议变更授权** |
| 但需要 | **一次显式授权的 golden 重基线**，并在提交正文【边界】中记录 |
| 验证方式 | 重基线前后**逐像素解码结果必须完全一致**（用我方 reader 与 libtiff **双向**验证），只允许字节布局差异 |
| 顺序 | **必须在 M0-11 之前完成**（见 §5.2），否则 M0-11 会验出 UB |
| 附带收益 | 修完后 `samples_per_pixel=7`（RIP 输出）也天然安全，为 RIP 侧复用我方 writer 思路留门 |

### 5.2 主动响应打印侧 M0-11（libtiff 互操作验证）

打印侧安排了 M0-11（0.5 人日）：用 libtiff 打开我方真实 `layer_000000.tiff`，逐 tag 读取 + 全行 `TIFFReadScanline` + 与我方 reader 逐像素比对 + 记录 libtiff warning。并写明"**若这一步失败，整个手写 IFD 方案需要重新评估**"。

**切片侧立场（P）**：这个验证**非常必要，且应由我方先自查**，不要等打印侧发现。承诺：

1. 在 M0 期间自行完成 libtiff 互操作自查（先修 F-1/F-2，再验）；
2. 把该验证固化为切片侧 CI 用例（`tiff_libtiff_interop_test`），防止回归；
3. 若 libtiff 报任何 warning，即视为我方缺陷，不推给"libtiff 容忍度"。

> **架构层面的反思（P）**：手写 IFD 的根因是早期避免依赖。既然下游消费者用 libtiff，"自产自销的读取器"就构成**验证盲区**——我方 reader 与我方 writer 共享同一套错误假设，永远测不出来。这条经验应写进 `KNOWLEDGE`：**任何自产自销的格式实现，必须至少有一个独立实现做交叉验证。**

---

## 6. 我方交付物清单（对应打印侧里程碑）

| 打印侧里程碑 | 切片侧交付物 | 前置 |
|---|---|---|
| **M0 契约冻结** | ① `file_contract_v1` 的进度行/退出码规格（`SL-02`）② manifest 权威来源确认（`SL-08`）③ `SL-07/09` 确认 ④ **F-1/F-2 修复 + libtiff 互操作自查** ⑤ 提供真实 manifest 样例供 M0-02 校验 | 无 |
| **M1 模块可装载** | `slicer_module.dll` + `module.json` + `slicer_worker.exe`（= 现 `slicer_cli`）+ 依赖 DLL；**过 C-SPI-01..18**。⚠️ 交付包中**不含** `slicer_core`——它是静态库中间产物，已被 DLL 与 Worker 各自静态链接（见 `INT_10` §1.1）| M0-01 定稿 |
| **M2 单模型离线链路** | `package.verify` 薄壳；`slice.rgbwsv` 单实例；实例内存画像表（`SL-06`）；strict-PASS 资产清单 | M1 |
| **M3 多模型** | 联合切片 + per-instance 统计（`SL-04`）；`scene.transform` 完整出参；（可选）`scene.viewdata` | M2 |
| **M4/M5** | 无新增交付；配合联调与缺陷修复 | — |

---

## 7. 需要打印侧回复的事项（反向清单）

| 编号 | 事项 | 影响 |
|---|---|---|
| **PR-01** | 确认接受方案 C 下"碰撞/越界由 `scene.transform` 出参提供"，而非独立 `scene.layout` 能力 | 影响 SPI 能力清单定稿 |
| **PR-02** | `module.json` schema 中 `delayLoad`（默认 true）与 §8.4a"不用 /DELAYLOAD"矛盾——请裁定该字段语义（建议改为 `loadMode: "runtime"` 或直接删除） | 影响清单字段 |
| **PR-03** | C-SPI 到底是 **17 项还是 18 项**（CLD_10 §13 表有 18 行，§14 与 CLD_06 §15 写 17）——请给出唯一口径与报告命名 | 影响 M1 验收 |
| **PR-04** | `profile.device.grayBits` 的请求路径未出现在 CLD_06 §5 的 JSON 示例中——请在 M0-03 固化（切片侧不使用该字段，但会影响 RIP 与 S2 校验） | 影响 S2 |
| **PR-05** | `SL-10` 的产品决策（建议采用 §4.1 的 ②+③ 组合） | 阻塞 M2/M3 的 E2E |
| **PR-06** | per-instance 统计是否需要材料体积/成本量；若需要请给单位与口径 | 影响 `SL-04` 字段 |
| **PR-07** | 是否接受"golden 重基线"作为 F-1/F-2 修复的必要代价（切片侧内部走授权，但需打印侧知晓时间点） | 影响 M0 排期 |

---

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-07-28 | v1.0 | 首版。采纳 CLD_27 方案 C；答复 OPEN-27-04 与 SL-01..SL-10；能力边界详细化为可实现规格；自认 5 个缺陷（含 CLD_10 §7.3 发现的两个 TIFF 对齐缺陷）并提出受控 golden 重基线；提出 PR-01..PR-07 反向清单 |
