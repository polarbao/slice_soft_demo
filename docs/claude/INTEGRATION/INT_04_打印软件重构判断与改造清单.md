# INT_04 打印软件重构判断与改造清单

> 目录：`docs/claude/INTEGRATION/`。日期：2026-07-27。对象：`ry_print_demo / PrintSolution`（`feature/v0.1.1`）。
> 证据等级：A=已核实代码事实，P=判断/建议。

## 1. 结论：**不需要重构，只需"新增前置链路"**

```text
判断：现阶段不做打印链路重构。
理由：打印侧入口已经是"目录契约"，天然与上游解耦；
      接入切片/RIP 属于在其之前【新增】一段前置链路，而不是改造既有链路。
动作：新增 business/platform/ 与 business/prepress/，既有打印链路零改动。
```

## 2. 决定性证据（A）

`PrintService` 的作业入口签名（`business/print/PrintService.h:105-116`）：

```cpp
/** @param sliceFolderPath 切片目录路径。*/
A3D::A3DResult StartJob(const std::string& sliceFolderPath, const std::string& jobName);
A3D::A3DResult StartJob(const std::string& sliceFolderPath, const A3D::PrintConfig&, const std::string&);
A3D::A3DResult StartJobWithScanInfo(const std::string& sliceFolderPath, ...);
bool           HasBreakpoint(const std::string& sliceFolderPath) const;
A3D::A3DResult ResumeFromBreakpoint(const std::string& sliceFolderPath);
```

内部通过 `m_ctrl->ScanSliceFolder(sliceFolderPath)` 扫描目录后再提交（`PrintService.cpp:324`）。

**这意味着**：打印侧从不关心"这个目录是谁生成的"。今天由人工准备，明天由 `切片→RIP→通道化` 自动生成——**对 `PrintService` 而言完全等价**。这是一个已经存在的、干净的**反腐层（anti-corruption seam）**。

三条佐证（A）：

1. 既有 `ChannelSplitter` 的输出正是这种目录（`slice_{newLayer}_{channel}.bmp`，12 通道 1bpp，安全发布：暂存 + 全部成功才原子发布）；
2. 对方正式设计 DEV_51 的**非目标**明确写着：不重写 `PrinterManager` / `PrintService` / `PrinterRuntime` / `VendorEngineAdapter`；
3. 验收项 `AC-28-04` 要求：打印运行时**不需要加载**模型或切片/RIP 引擎即可消费 Ready 输入——正说明打印侧应保持与上游无耦合。

## 3. 为什么"现在重构"是错误选择（P）

| 反对理由 | 说明 |
|---|---|
| 第一阶段已封账冻结 | P0–P12 + LOG/CFG 专项已"软件侧封账"（A）；重构会让已冻结的回归基线失效 |
| 真机验证尚未完成 | `P12-E` 真机验证未完成（A）。**在真机验证通过前重构打印链路，会让"到底是重构引入的还是本来就有的"无法归因** |
| 没有重构触发条件 | 重构应由"新需求无法自然承载"触发。而接入切片/RIP **不需要改打印链路**（§2），触发条件不成立 |
| 风险收益不匹配 | 打印链路直连设备与物理动作，改动风险最高、回报最低（它已经工作） |
| 与对方设计冲突 | DEV_51 已把这四个类列为非目标 |

## 4. 但有三处"必须处理"的（P，属新增/加固，不是重构）

### 4.1 命名冲突（低风险，必须做）

`business/slice/` 当前是**RIP 后通道化**，不是几何切片（`ChannelSplitter.h:6` "RIP 后切片图的通道化拆分模块"）。接入真正的切片引擎后，两个"slice"会造成长期混淆。

- 对方文档已计划改名 `SliceService → PreparedPrintDataService`（PRD_00:76），DEV_51:57 亦要求 `SlicerService` 与既有 `SliceService` **必须命名分离**。
- **建议**：在接入前先做这次改名（纯重命名 + 引用更新，有测试保护），成本低、收益高。新链路阶段名统一用 `Channelize`。

### 4.2 Ready 闸门显式化（必须做）

当前"目录 → 可打印"的判定隐含在 `ScanSliceFolder` 里。接入自动链路后，必须有一个**显式的 Ready 闸门**：只有通过 `PackageVerifier`（S3 校验）的目录才允许进入 `StartJob`。

- 理由：自动生成的目录可能层不连续、通道缺失、尺寸不一致；`ChannelSplitter::ValidateLayers` 已有这些规则（A），应把它提升为**入口守卫**而非仅内部校验。
- 形态：`ReadyGate::Admit(packageDir) → {ok, issues[], stableCode}`，失败 fail-closed，不进 `StartJob`。

### 4.3 作业模型的"跨阶段"扩展（新增，不改旧）

`PrintService` 的 `PrintJobState{Idle, Printing, Paused, Stopping}` 只覆盖**打印阶段**（A）。前置链路需要 `Import/Preflight/Layout/Slice/Rip/Channelize/Verify` 这些阶段。

- **不要**去扩 `PrintJobState`（会污染已冻结的打印状态机）；
- **应当**在 `business/platform/` 新增 `PipelineJob`（跨阶段），把 `PrintJobState` 作为其最后一个阶段的**内部状态**投影出来。

```mermaid
flowchart LR
  PJ["PipelineJob（新增·跨阶段）<br/>Import→Preflight→Layout→Slice→Rip→Channelize→Verify→Ready→Print"]
  PS["PrintJobState（既有·不改）<br/>Idle/Printing/Paused/Stopping"]
  PJ -->|"Print 阶段内部投影"| PS
```

## 5. 最小改造清单（P）

> 原则：**只新增、不改既有打印链路**。下表"改动类型"一栏是关键。

| # | 项目 | 位置 | 改动类型 | 优先级 |
|---|---|---|---|---|
| 1 | `SliceService → PreparedPrintDataService` 改名 | `business/slice/` | 重命名（有测试保护）| P0 |
| 2 | 新增 `business/platform/` | 新目录（Qt-free）| **新增** | P0 |
| 2.1 | `ModuleRegistry`（DLL 发现/装载/版本协商/自检）| 同上 | 新增 | P0 |
| 2.2 | `ProfileService`（单一 Profile 真源 + 按模块投影）| 同上 | 新增 | P0 |
| 2.3 | `PipelineOrchestrator`（跨阶段作业/进度/取消/恢复）| 同上 | 新增 | P1 |
| 2.4 | `SeamValidator`（S1/S2/S3 三接缝校验）| 同上 | 新增 | P0 |
| 2.5 | `ErrorTranslator`（统一错误码 `PM-<模块>-<类别>-<码>`）| 同上 | 新增 | P1 |
| 3 | 新增 `business/prepress/`（`SlicerService`/`RipService`/`PackageService`）| 新目录 | **新增** | P0 |
| 4 | `ReadyGate` 显式闸门 | `business/platform/` | 新增 | P0 |
| 5 | `PrepressJobRepository`（SQLite 作业留档）| `data/repository/` | 新增（沿用既有仓储范式）| P1 |
| 6 | UI 新增 Prepress 页（导入/排版/预览/作业监控）| `presentation/views/` | **新增页面**，不改既有页 | P1 |
| 7 | CMake：模块目录部署 + 延迟加载 | `PrintApp/CMakeLists.txt` | 小幅新增（照抄 A3DSDK 范式）| P0 |
| — | `PrintService` / `PrinterManager` / `PrinterRuntime` / `VendorEngineAdapter` | — | **禁止改动** | — |

## 6. 禁改清单（P，红线）

```text
禁止改动 PrintService / PrinterManager / PrinterRuntime / VendorEngineAdapter 的现有行为与签名
禁止扩展 PrintJobState 以容纳前置阶段
禁止让 business/prepress/ 或 business/platform/ 链接 A3DSDK / PrintSDK / motionControlSDK（DEV_51:20）
禁止把几何/编排代码放进 presentation/（有架构守卫测试 test_presentation_source_boundary.cpp 会拦）
禁止让切片/RIP 模块成为 PrintApp 的加载期强依赖（AC-28-04 → 必须 /DELAYLOAD）
禁止在未过 ReadyGate 的情况下调用 StartJob
```

## 7. 什么时候才该重构打印链路（P）

给出明确触发条件，避免"永远不重构"或"过早重构"：

| 触发条件 | 届时应做 |
|---|---|
| P12-E 真机验证通过并封账 | 才具备安全重构的基线 |
| 需要多设备并行作业队列 | `PrinterManager` 的"每设备串行"模型需升级为真正队列 |
| 需要真正的断点续打（跨进程/跨重启）| `BreakpointManager` 语义需重做 |
| `PrintService` 状态机被前置阶段反复污染 | 说明 §4.3 的分层没守住，需重新划界 |
| 出现第二种设备/厂商 SDK | `VendorEngineAdapter` 需抽象为多实现 |

在这些条件出现之前，**保持打印链路稳定就是最大的工程价值**。

## 8. 一句话总结（P）

**打印软件现在不需要重构——它的作业入口 `StartJob(sliceFolderPath)` 已经是一个干净的目录契约反腐层，切片与 RIP 的接入是在它【之前】新增一段前置链路。要做的是：改一个误导性命名、加一个显式 Ready 闸门、新增 platform/prepress 两层，然后一行都不碰既有打印代码。**
