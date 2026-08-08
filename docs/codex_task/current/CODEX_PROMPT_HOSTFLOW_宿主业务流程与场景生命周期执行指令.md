# CODEX_PROMPT_HOSTFLOW 宿主业务流程与场景生命周期执行指令

> 文档状态：**ACTIVE / H-A COMPLETE / H-B-01..05 COMPLETE / H-B-06 NEXT**
> 版本：v1.8 ｜ 日期：2026-08-08
> **定位：独立补充专项的执行入口，不属于 Stage 14 任何任务组，不占阶段编号。**
> 任务卡：`docs/codex_task/current/TASKS_HOSTFLOW_宿主业务流程与场景生命周期补齐任务清单.md`

---

## 0. ⛔ 开工前置（未满足不得动代码）

```text
① 用户已明确指定要执行的原子卡号（例如「执行 H-B-04」）
② 若指定的是 H-A 组任何一张卡 → 必须先确认 HQ-01 已授权
   （H-A 修改已冻结契约 contracts/slicer_capability_dtos.json，
     需要与 14A-04-R1 同级的用户显式授权）
③ 本文档与 TASKS_HOSTFLOW 已于 2026-08-07 转为 ACTIVE；仍只执行用户点名的单卡
```

**若用户只说「继续」或「按优先级做」而未指定卡号 → 停下来询问，不要自选。**

---

## 1. 为什么有这个专项

Stage 14 的 14E 交付了 `apps/slicer_ui_host_sim`，但它是**技术验证壳**，不是可操作的业务宿主：

```text
已有   ABI 装载（ModuleClient）· 三车道交互 · 视图渲染 · 能力覆盖批处理
缺失   导入入口 · 实例列表 · Profile 选择 · 参数设置 · 切片提交 · 结果查看
证据   HostMainWindow.h 自述 "Minimal Qt reference shell"；
       HostMainWindow.cpp 中 QFileDialog / QPushButton / slice 零命中；
       CapabilityCoverageRunner 自述 "Executes the frozen capability checklist"
```

根因：14E 九张卡全是「能力/机制」导向，**没有一张是「业务流程」导向**。
14E-04b「端到端打通」被合理实现为自动跑批 —— 技术上满足卡片，但人不能用它干活。

**本专项补的就是这一层。**

## 2. 两个 ABI 缺口（H-A 组要解决的）

### 2.1 缺 `addInstance` / `removeInstance`（A）

`contracts/slicer_capability_dtos.json:89`：

```json
{ "path": "operations[].type",
  "type": "enum:translate|rotateZ|uniformScale|mirror", "required": true }
{ "path": "operations[].instanceId", "type": "string", "required": true }
```

**只有四种变换，且每个 operation 必须带已存在的 `instanceId`。**
`model.import` 只返回 `modelId`，**ABI 层没有把模型加进场景的通路**。

后果：宿主只能靠传完整 `scene` 对象绕过 → 必须理解 scene 内部结构 →
**「打印软件最少改动移植」这一 Stage 14 核心目标不成立。**

### 2.2 缺 `scene.layout` 规则排版（A）

打印侧 `CLD_04` §4.2 声明能力包应提供 8 项能力，其中
`scene.layout`「规则排版（行列 + 间距）+ 碰撞检测 · 进程内」**在我方 15 项中无对应项**。

切片侧**有实现**（`GridLayoutPolicy`，13B-03 的 11×2 排版），但未经 ABI 暴露。
宿主只能自己算落位再逐个 `translate` → **排版算法两侧各写一遍，结果可能不一致**。

### 2.3 采纳方案：扩展枚举，不新增能力

```jsonc
H-A-01 / DTO v1.5:
  "enum:addInstance|removeInstance|translate|rotateZ|uniformScale|mirror"

H-A-04（后续独立卡，不得偷渡进 H-A-01）:
  在后续受控修订中再加入 applyGridLayout
```

```text
✅ PM_SPI_VERSION 不变   ✅ 11 个导出不变   ✅ 15 项能力不变
✅ minor 兼容（新枚举值 + 可选字段，旧宿主与旧请求行为逐字节不变）
场景创建隐含：无 sceneHandle + scene 缺省或 {} + 至少一个 addInstance → 创建并返回 sceneHandle
场景释放：scene session 绑定 pm_module_t，由 pm_destroy 清理；pm_release 只释放 pm_job_t
```

## 3. 执行范围

### 要做

```text
H-A   ABI 场景生命周期（4 卡）—— 需 HQ-01 授权
      H-A-01 契约扩展 · H-A-02 Facade 实现 · H-A-03 空场景闭环 · H-A-04 scene.layout
H-B   宿主业务流程 UI（8 卡）—— 不改契约，无需额外授权
H-C   移植交付物（3 卡）
```

### ⛔ 明确不要做

```text
✗ 不新增第 16 项能力，不新增第 12 个导出符号
✗ 不修改 p0.rgbwsv.2 协议、不修改生产 TIFF 输出
✗ 不改动主干 apps/slicer_debug_ui/（它是【功能规格来源】，只读不写）
✗ 不在宿主内 include slicer_core/**（14E 的 CI 依赖守卫已在，不得绕过）
✗ 不实现作业队列 / 多任务 / 持久化设备 Profile / 多设备
  —— CLD_04 §4.2 已明确划归 PrintApp，【不属于切片能力包】
✗ 不覆盖切片专有诊断类（HQ-03 已定：只做核心流程），排除清单见 TASKS §6.6.1
✗ 不写渲染后端（归 TASKS_RENDER 的 R-D 组）
```

## 4. 功能规格来源：主干 UI 就在 HEAD 上

**不要去翻历史 commit。** 14E 的决策是「主干一行不改」，所以旧版 UI 功能**当前仍在 HEAD**。

```text
apps/slicer_debug_ui/     68 个头文件，覆盖完整业务流程
用法                       ① 作为功能规格逐项对照
                          ② 与 slicer_ui_host_sim 【并排运行做 A/B 对照】
```

按业务环节的文件映射见 `TASKS_HOSTFLOW` §1.1；三桶分类（可移植 / 需改走 ABI / 切片专有）见 §1.2。

## 5. 业务级验收硬标准（替代 API 级的「能力覆盖」）

```text
一个没读过本项目文档的操作员，能在 slicer_ui_host_sim 中独立完成：
  空场景 → 导入模型 → 排版 → 选 Profile → 调参数 → 切片 → 看到结果包并确认校验通过
全程【不需要手写任何 JSON】，【不需要命令行】，【不需要查阅源码】。
```

三条附加约束：

```text
① 宿主全程只经 11 个公开导出，不得 include slicer_core（CI 守卫已在）
② 宿主不得构造 scene JSON（H-A-03 的直接体现）
③ 拖拽与相机操作期跨 DLL 调用恒为 0（UI-M1 / UI-M7 不得因新增流程而破坏）
```

> ⚠️ 卡片验收写「对照主干 XXX 行为一致」时，**必须真的把主干跑起来比对**，
> 不得凭读代码推断。A/B 对照差异逐条记录到 H-C-03。

## 6. 五个容易踩的坑

### 坑 1 · `buildVolume` 的权威归属是宿主，不是切片模块

`CLD_04`:360 明确：**`buildVolume` 由打印软件提供，切片模块只消费并 fail-closed 校验。**
14A-11 给的 230×100×60 是**默认值**，不是权威源。

H-B-05 的设置界面必须体现这个归属，否则参考实现会给打印侧错误暗示。

H-A-02 的隐式场景同样受此约束。HQ-07 已接受 14F-R2 `sceneContext` 修订；当前 DTO v1.7
由宿主提供 `resolvedProfileId` 与权威 `buildVolume`；实现不得回退到写死
`230 x 100 x 60 mm` 或默认 Profile。

### 坑 2 · Profile 目录归宿主，模块能力必须经 ABI 查

主干 `ScenarioRegistry` 直接读 `samples/scenarios/slicer_scenarios.json`。
**宿主不能这么做** —— 那是内部资产路径，打印侧拿不到。宿主 Profile 必须来自 PrintApp
自有目录，模块能力必须来自 `pm_module_info`，可用性由二者结构化求交。

> **2026-08-08 授权结论**：用户已授权 HQ-08-A。Profile 目录与安全等级归宿主，模块只经
> 既有 ABI 自述能力；不得改用内部 JSON、模块内 Profile 目录或字符串包含式能力推断。

### 坑 3 · H-B 大部分卡不依赖 H-A，别串成一条链

只有 **H-B-01（导入）** 硬依赖 H-A-03，**H-B-03 的规则排版部分**依赖 H-A-04。
`H-B-04 → 05 → 06 → 07 → 08` 这条链**完全不依赖 H-A**，用既有 fixture scene 即可开工。
不要因为等 H-A 授权就把整个 H-B 停住。

### 坑 4 · 三车道机制已存在，H-B-03 只补 UI 入口

`SceneInteractionController` 与 `TransformCommitPolicy`（14E-03）已实现。
H-B-03 是**补操作入口**，不是重写交互机制。动了它们就会破坏 UI-M1。

### 坑 5 · 新 app 从第一天起受行数门禁约束

`apps/slicer_ui_host_sim/` 在 14B-06 的受保护目录内，**不得进白名单**。
单文件 ≤ 500 行，超出即拆。

### 坑 6 · 场景创建后不得静默切换 Profile/buildVolume

H-B-05 已把宿主 Profile 与设备 buildVolume 注入首次 `sceneContext`。场景创建后两者成为
权威身份；参数面板允许继续修改 DPI、层厚、输出目录和材料策略，但 Profile/buildVolume 异值
必须提示新建场景后生效。H-B-06 不得绕过该状态提交切片。

## 7. 停止条件

出现以下任一情况**立即停止并回报**：

```text
✗ 发现必须新增第 16 项能力或第 12 个导出才能实现  → 设计有误，回来复核
✗ 发现必须修改 p0.rgbwsv.2 或生产 TIFF 输出        → 越界
✗ 宿主出现 include slicer_core 才能编过的情况       → 边界被破坏
✗ 主干 apps/slicer_debug_ui/ 出现任何改动           → 违反「主干不改」
✗ H-A 相关卡但 HQ-01 未授权                        → 无授权不得动冻结契约
✗ UI-M1 / UI-M7（拖拽与相机期零跨 DLL 调用）被破坏  → 三车道设计被违反
```

## 8. 开工顺序

```text
【需 HQ-01 授权】
第 1 批   H-A-01 → H-A-02 → H-A-04 → H-A-03
          （H-A-04 排在 03 之前：空场景闭环最好一次把排版也验上）

【无需额外授权，可立即开工】
第 2 批   H-B-04（已完成）→ H-B-05 → H-B-06 → H-B-07 → H-B-08
          H-B-02 · H-B-03（变换部分）可并行

【H-A 完成后】
第 3 批   H-B-01（导入建场景）· H-B-03 的规则排版部分

【全部业务流程跑通后】
第 4 批   H-C-01 → H-C-02 → H-C-03
```

> **若 HQ-01 尚未授权，直接从第 2 批开始** —— 这是当前不需要任何新决策就能推进的路径。

## 9. 完成后必须更新

```text
① TASKS_HOSTFLOW 对应卡：状态、完成日期、实际验证命令与结果、剩余风险
② docs/codex_task/README.md 的「切片侧收口后的接续专项」进度
③ 若涉及契约改动：contracts/ 物料 + 打印侧回签清单同步
④ 未运行的验证不得写成 PASS
```

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-08 | v1.8 | H-B-05 完成宿主切片参数、设备 buildVolume、有效 Profile 自哈希预览和场景绑定保护；Debug/Release 联合门禁与主干设置 A/B smoke 通过。下一卡为 H-B-06。 |
| 2026-08-08 | v1.7 | 用户授权 HQ-08-A；H-B-04 完成宿主 Profile 目录、ABI 模块能力求交、生产安全等级和不可用原因 UI，选择期零 DLL 调用；联合门禁与主干 A/B smoke 通过。下一卡为 H-B-05。 |
| 2026-08-07 | v1.6 | H-B-04 准备审计发现 ABI Profile 发现协议缺口，新增 HQ-08 与停止条件；H-B-04 标记为准备完成但实现阻断，推荐宿主目录与 ABI 模块能力求交。 |
| 2026-08-07 | v1.5 | H-A-03 完成：纯 C/Qt 宿主从空场景仅经 11 导出完成生产闭环，宿主手工 scene builder 已移除；权威 snapshot scene 可不透明透传，Debug/Release 与边界门禁通过。H-A 全组完成，H-B-01 成为下一候选卡，但仍须显式启动。 |
| 2026-08-07 | v1.4 | H-A-04 完成：`applyGridLayout` 经既有 Commit lane 暴露 11×2 规则排版，DTO v1.7、22 实例、replay 与 fail-closed 门禁通过；H-A-03 成为下一独立卡。 |
| 2026-08-07 | v1.3 | 用户授权 HQ-07 与 H-A 后续开发；DTO v1.6 和 H-A-02 运行时完成并通过 Debug/Release 门禁。下一独立原子卡为 H-A-04，完成后再执行 H-A-03；H-B/H-C 依赖关系同步收敛。 |
| 2026-08-07 | v1.2 | H-A-02 准备审计新增 HQ-07 停止条件：隐式场景必须取得宿主权威 Profile/buildVolume；登记 14F-R2 `sceneContext` 受控修订提案，未经授权禁止运行时实现。 |
| 2026-08-07 | v1.1 | 用户授权 HQ-01 并点名执行 H-A-01，专项转 ACTIVE；H-A-01 与 H-A-04 的合同修订分离；补充兼容型隐式建场景条件；纠正 `pm_release` 不能关闭整数 sceneHandle，场景会话改由 `pm_destroy` 统一清理。 |
| 2026-08-07 | v1.0 | 首版。补齐本专项缺失的执行入口；明确 HQ-01 授权为 H-A 前置；写入两个 ABI 缺口的 A 级证据与「扩展枚举不新增能力」方案；列 7 条禁止项（含 CLD_04 已划归 PrintApp 的作业队列等）；给出 5 个坑（`buildVolume` 归属宿主、Profile 必须经 ABI 查、H-B 大部分不依赖 H-A、三车道已存在、行数门禁）；分 4 批开工顺序并标出**无需授权即可启动的第 2 批** |
