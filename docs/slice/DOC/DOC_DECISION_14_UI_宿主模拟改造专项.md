# DOC_DECISION_14_UI UI 宿主模拟改造专项

> 文档状态：✅ **ACTIVE**（2026-08-04 随 Stage 14 激活成立）
> 版本：v1.0 ｜ 日期：2026-08-04
> 定位：**Stage 14 的 14E 任务组唯一权威设计文档。**
> 上游：`DOC_DECISION_14`（封装结构）、`DEV_14` §5（承载分派）、`DOC_DECISION_14_S2`（S2 条款）
> 证据等级：A=已核实事实，B=目标设计，P=判断

---

## 1. 本专项要解决什么

切片能力包封装完成后，需要有人**以打印软件的身份**去调用 `slicer_module.dll`，
验证能力面是否够用、契约是否可实现、交互延迟是否可接受。

**由我方 UI 充当第一消费者**，好处是问题在交付给打印侧之前就暴露：

```text
① ABI 会物理性地强制架构边界 —— 比文档约定可靠得多
② 能力面缺失、DTO 字段不足、进度/取消语义含糊，都会在真实交互中立刻显形
③ 打印侧拿到的是已被真实 UI 跑通的模块，而不是只过了自测套件的模块
```

## 2. 承载方式决策：独立 app target，**不开分支**（2026-08-04 定案）

```text
✅ 新建  apps/slicer_ui_host_sim/     Qt 宿主模拟，【只】链 slicer_module.dll
✅ 保持  apps/slicer_debug_ui/        主干一行不改，继续直连 slicer_core
```

### 2.1 为什么不用长命特性分支（P）

| 理由 | 说明 |
|---|---|
| **与既有原则冲突** | `INT_07` §3.2 原则第一条即「**不做长命分支**」。14E 原方案的"UI 模拟分支"恰恰是长命分支 |
| **主干在持续演进** | Stage 15 刚在 `slicer_debug_ui` 落地 `TextureWhitePreflightService` 与 `EffectiveConfigGenerator` 改造。分支会立刻开始分叉，且需反复合并 |
| **CI 守不住两边** | 依赖方向守卫（禁 include `slicer_core`）只能对一个目标生效；分支上的守卫不会阻止主干回归 |
| **失去对照能力** | 独立 app 与主干可**同时存在、同时运行**，同一模型同一 Profile 直接 A/B 对比；分支做不到 |

### 2.2 为什么也不用主干内编译开关（P）

会在 UI 内长期保留「直连 core」与「走 DLL」两条后端分支逻辑 ——
这正是 `14D-06`（取消 `backend=inprocess`）在切片侧刚刚要消除的那类问题。不应在 UI 侧重新引入。

### 2.3 与 `INT_07` U0–U5 的关系（重要澄清）

```text
INT_07 U0–U5  = 把【现有】slicer_debug_ui 整体迁移到 DLL
                → 已于 2026-08-03 降级为「可选后置」，本阶段【不执行】
本专项         = 【新建】一个只走 DLL 的宿主模拟 app
                → 不是迁移，不触碰现有 UI
```

二者不冲突。本专项先用真实交互证明 DLL 能力面够用；
`slicer_debug_ui` 是否最终迁移，留到 DLL 稳定后再作独立决策。

## 3. 目标目录与依赖守卫（B）

```text
apps/slicer_ui_host_sim/
├─ main.cpp
├─ ModuleClient.{h,cpp}               运行时 LoadLibrary + GetProcAddress（11 符号）
├─ SceneInteractionController.{h,cpp} 交互编排：本地乐观显示 + 提交式权威求值
├─ TransformCommitPolicy.{h,cpp}      三车道（Transient / Commit / Production）
├─ TopViewRenderPolicy.{h,cpp}        俯视渲染数据获取与刷新节流
├─ MoveOptimizationPolicy.{h,cpp}     拖拽期本地预测与回滚
└─ CMakeLists.txt
```

**三条硬性依赖约束（CI 强制）**：

```text
① CMake：不得出现 target_link_libraries(slicer_ui_host_sim ... slicer_core ...)
② 源码：不得出现 #include "slicer_core/..." 或 <slicer_core/...>
③ 产物：dumpbin /DEPENDENTS 中不得出现 slicer_core 相关静态符号泄漏；
        对 slicer_module.dll 必须是【运行时装载】，不得出现在导入表
```

约束 ③ 的第二句尤为关键 —— 它验证的是「宿主可以在 DLL 缺失时优雅降级」这一真实场景。

## 4. 能力覆盖清单（B · 填补原 14E 的最大缺口）

原 14E-02 只说"建 `ModuleClient`"，未规定要打通哪些能力。现按 `DEV_14 §5` 的 15 项能力分档：

| 档 | 能力 | 承载 | 为什么在这一档 |
|:--:|---|---|---|
| **P0** | `model.import` | DLL（待 14B-00）| 端到端最小闭环入口 |
| **P0** | `scene.apply_operation` | DLL | 交互核心，三车道的验证对象 |
| **P0** | `scene.get_snapshot` | DLL | 权威状态回读，Stale 回滚依赖它 |
| **P0** | `slice.rgbwsv` | **Worker** | 长时作业：进度 / 取消 / 崩溃恢复全在这条路径 |
| **P0** | `package.verify` | DLL | 产出自检，闭环终点 |
| **P1** | `scene.get_viewdata` | DLL | 俯视渲染 |
| **P1** | `geometry.collision` | DLL | 拖拽期碰撞反馈 |
| **P1** | `geometry.preflight(fast)` | DLL | 导入即时体检 |
| **P1** | `package.get_layer_descriptor` | DLL | 层预览导航 |
| **P1** | `package.render_layer_preview` | DLL | 层预览渲染 |
| **P2** | `model.get_metadata` / `model.release` | DLL | 生命周期附带，随 P0 自然覆盖 |
| **P2** | `package.get_summary` / `read_report` | DLL | 只读查询，风险低 |
| **P2** | `geometry.preflight(full)` / `geometry.repair` | **Worker** | 长时作业第二条路径，可复用 slice 的进度/取消验证 |

**出口要求**：P0 全部打通且可端到端演示；P1 全部打通；P2 至少各调用一次并记录返回。

> P0 里刻意包含了 `slice.rgbwsv`（Worker 承载）与四项 DLL 进程内能力 ——
> 这样最小闭环就**同时覆盖了两种承载方式**，能尽早暴露跨进程契约问题。

## 5. 可测验收标准（B · 替换原「手感不回归」）

原 14E-04 的验收写作「手感不回归」，不可测量。改为：

| 编号 | 指标 | 阈值 | 测法 |
|---|---|---|---|
| **UI-M1** | 拖拽期（`mouse-move`）跨 DLL 调用次数 | **恒为 0** | `ModuleClient` 内置调用计数器，拖拽起止取差值断言 |
| **UI-M2** | 变换提交（Commit 车道）往返延迟 P95 | **≤ 150 ms** | 提交时间戳 → `get_snapshot` 返回时间戳，采样 ≥ 50 次 |
| **UI-M3** | 俯视渲染帧率 | **≥ 主干 `slicer_debug_ui` 的 90%** | 同模型同视角，各测 30 秒取均值 |
| **UI-M4** | `SceneRevisionStale` 回滚 | **可演示且状态一致** | 构造并发修改 → 断言回滚后快照与权威一致 |
| **UI-M5** | 切片取消响应 | **≤ 2 s 且无 `.staging` 残留** | 与 14D-04/05 同口径 |
| **UI-M6** | DLL 缺失时启动 | **优雅报错，不崩溃** | 移走 `slicer_module.dll` 后启动 |

UI-M1 是三车道设计是否真正落地的**唯一硬证据** —— 只要拖拽期出现任何跨 DLL 调用，
说明 Transient 车道没做对，手感问题必然随之而来。

UI-M3 取 90% 而非 100%：跨 ABI 必然有开销，要求完全持平不现实；
低于 90% 则说明渲染数据获取路径需要优化（多半是没做节流或批量）。

## 6. 与其它任务的接口

| 关联 | 说明 |
|---|---|
| **14C-06** | 本专项全部任务的前置 —— 必须 C-SPI-01..18 全绿后才开始 |
| **14B-00** | `model.import` 归属结论会影响 P0 第一项走 DLL 还是 Worker；不影响是否覆盖 |
| **14E-05 拆分** | 主干 `MainWindow`(3659) / `UiSmokeTestRunner`(6963) 的拆分**与本专项解耦** —— 本专项不改主干，拆分按 `INT_11` 独立推进 |
| **14B-06 行数门禁** | 新 app 的文件从第一天起就受门禁约束（≤ 500 行/文件），不进白名单 |
| **14E-06 可移植清单** | 本专项产出的 `SceneInteractionController` / `TransformCommitPolicy` 等即为可移植候选 |

## 7. 风险

| 编号 | 风险 | 等级 | 缓解 |
|---|---|---|---|
| **UI-R1** | 两套 UI 代码并存，短期维护成本上升 | 中 | 明确边界：新 app **只做宿主模拟验证**，不追求功能完整；不承担生产职责 |
| **UI-R2** | 新 app 沦为"只跑得通的玩具"，暴露不出真实问题 | **高** | 由 §4 的 P0/P1 覆盖档与 §5 的六项可测指标共同约束；UI-M1/M3 尤其难以靠"玩具"蒙混 |
| **UI-R3** | 跨 ABI 开销导致交互延迟不可接受 | 中 | UI-M2/M3 提前量化；若不达标，结论本身就是对能力面设计的有效反馈（可能需要批量/流式接口） |

## 8. 不在本专项范围

```text
✗ 迁移现有 slicer_debug_ui（INT_07 U0–U5，已降级为可选后置）
✗ 拆分主干 UI 大文件（14E-05，按 INT_11 独立推进）
✗ 新 app 追求与主干 UI 功能对等
✗ 新 app 承担任何生产职责
```

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-04 | v1.0 | 首版。定案「独立 app target，不开分支」并给出三条理由；澄清与 `INT_07` U0–U5 降级方案的关系；补 15 项能力的 P0/P1/P2 覆盖清单；以 UI-M1..M6 六项可测指标替换原「手感不回归」；定义三条 CI 依赖守卫；登记 UI-R1..R3 |
