# DOC_DECISION_14_UI UI 宿主模拟改造专项

> 文档状态：✅ **ACTIVE**（2026-08-04 随 Stage 14 激活成立）
> 版本：v1.3 ｜ 日期：2026-08-04 ｜ 双视图纹理修订：2026-08-05
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

### 1.1 双重定位：验证工具 **+ 打印软件的参考实现**（v1.1 强化）

`apps/slicer_ui_host_sim/` 不只是我方自用的验证工具，它同时是**交付给打印侧的参考实现**：
打印软件应当能对照它完成对切片能力包的**全方位调用**。

由此产生四条额外要求（区别于普通内部工具）：

```text
① 调用路径必须【完整走公开 ABI】—— 不得使用任何内部快捷方式或未公开约定
② 每个能力至少有一处可读的调用示例，含参数构造、错误处理与资源释放
③ 公共类型与函数带 Doxygen；错误分支不得静默吞掉，须演示错误码如何解读
④ 14E-06 产出的"可移植模块清单"须精确到【文件级】，标明哪些可直接复制、
   哪些是切片专有需改写 —— 打印侧据此评估移植成本
```

> 换句话说：**代码质量标准按"对外交付物"要求，不按"内部试验品"要求。**

## 2. 承载方式决策：独立 app target，**不开分支**（2026-08-04 定案）

```text
✅ 新建  apps/slicer_ui_host_sim/     Qt 宿主模拟，【只】链 slicer_module.dll
✅ 保持  apps/slicer_debug_ui/        主干一行不改，继续直连 slicer_core
```

### 2.1 为什么不用长命特性分支（P）

| 理由 | 说明 |
|---|---|
| **与既有原则冲突** | `INT_07` §3.2 原则第一条即「**不做长命分支**」。原"UI 模拟分支"方案恰恰是长命分支 |
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

### 2.4 时序前置：等最小能力包封装完成（v1.1 新增 · 用户决策 2026-08-04）

**现阶段继续使用当前 UI 布局，UI 层处理整体后置。**

原 14E 前置只写「14C-06」，过早 —— 那时 Worker 尚未打通，UI 拿不到可调用的完整能力包。
本专项采用 **M-MVP-CANDIDATE → M-MVP** 两级 Gate：

```text
M-MVP-CANDIDATE
  = 14C-06 全绿（C-SPI-01..18，DLL 薄壳可被独立套件验证）
  + 14D-05 完成（Worker 端到端跑通一次切片：提交 → 进度 → staging → 自检 → 原子发布）

14E-01 使用纯 C 控制台宿主，只通过 11 个 pm_* 导出完成
【导入 → 变换 → 切片 → 取包 → 校验】闭环。

M-MVP = M-MVP-CANDIDATE + 14E-01 PASS；14E-02 及后续 Qt UI 才可启动。
```

| 里程碑 | 含义 | UI 侧动作 |
|---|---|---|
| M-MVP-CANDIDATE 之前 | DLL/Worker 尚不可端到端调用 | **主干 UI 布局与功能保持原样，不做任何改造** |
| M-MVP-CANDIDATE 达成 | 可运行公开 ABI 闭环验证 | 执行 14E-01 控制台宿主 |
| M-MVP 达成 | 14E-01 已证明公开 ABI 可用 | 启动 14E-02，新建 `slicer_ui_host_sim` |

> 这条时序同时降低了返工风险：能力面在 14A–14D 期间还可能微调，
> 等 M-MVP 稳定后再写 Qt 宿主，避免 UI 追着 ABI 改。

## 3. 目标目录与依赖守卫（B）

```text
apps/slicer_ui_host_sim/
├─ main.cpp
├─ ModuleClient.{h,cpp}               运行时 LoadLibrary + GetProcAddress（11 符号）
├─ SceneInteractionController.{h,cpp} 交互编排：本地乐观显示 + 提交式权威求值
├─ TransformCommitPolicy.{h,cpp}      三车道（Transient / Commit / Production）
├─ camera/CameraController.{h,cpp}    3D 相机：orbit / pan / zoom / 预设视角（见 §6）
├─ camera/ViewModeSwitch.{h,cpp}      俯视 ⇄ 3D 切换（见 §6.5）
├─ render/TopViewRenderPolicy.{h,cpp} 俯视渲染数据获取与刷新节流
├─ render/SceneRenderPolicy.{h,cpp}   3D 场景渲染（平台/网格/坐标轴/越界高亮）
├─ render/AppearanceCache.{h,cpp}     mesh/appearance/texture identity 与 GPU 资源缓存
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

## 4. 能力覆盖清单（B）

原 14E-02 只说"建 `ModuleClient`"，未规定要打通哪些能力。现按 `DEV_14 §5` 的 15 项能力分档：

| 档 | 能力 | 承载 | 为什么在这一档 |
|:--:|---|---|---|
| **P0** | `model.import` | DLL（待 14B-00）| 端到端最小闭环入口 |
| **P0** | `scene.apply_operation` | DLL | 交互核心，三车道的验证对象 |
| **P0** | `scene.get_snapshot` | DLL | 权威状态回读，Stale 回滚依赖它 |
| **P0** | `slice.rgbwsv` | **Worker** | 长时作业：进度 / 取消 / 崩溃恢复全在这条路径 |
| **P0** | `package.verify` | DLL | 产出自检，闭环终点 |
| **P1** | `scene.get_viewdata` | DLL | 俯视渲染 **与 3D 渲染**（见 §6.4 能力面缺口）|
| **P1** | `geometry.collision` | DLL | 静止场景显式检查；拖拽期只用本地 bbox 非权威预测 |
| **P1** | `geometry.preflight(fast)` | DLL | 导入即时体检 |
| **P1** | `package.get_layer_descriptor` | DLL | 层预览导航 |
| **P1** | `package.render_layer_preview` | DLL | 层预览渲染 |
| **P2** | `model.get_metadata` / `model.release` | DLL | 生命周期附带，随 P0 自然覆盖 |
| **P2** | `package.get_summary` / `read_report` | DLL | 只读查询，风险低 |
| **P2** | `geometry.preflight(full)` / `geometry.repair` | **Worker** | 长时作业第二条路径，可复用 slice 的进度/取消验证 |

**出口要求**：P0 全部打通且可端到端演示；P1 全部打通；P2 至少各调用一次并记录返回。
对带纹理模型，P1 的 `scene.get_viewdata` 只有在 top 与 three_d 都显示纹理后才算通过。

> P0 里刻意包含了 `slice.rgbwsv`（Worker 承载）与四项 DLL 进程内能力 ——
> 这样最小闭环就**同时覆盖了两种承载方式**，能尽早暴露跨进程契约问题。

## 5. 可测验收标准（B）

| 编号 | 指标 | 阈值 | 测法 |
|---|---|---|---|
| **UI-M1** | 拖拽期（`mouse-move`）跨 DLL 调用次数 | **恒为 0** | `ModuleClient` 内置调用计数器，拖拽起止取差值断言 |
| **UI-M2** | 变换提交（Commit 车道）往返延迟 P95 | **≤ 150 ms** | 提交时间戳 → `apply_operation` 响应时间戳，采样 ≥ 50 次；正常成功不追加快照 |
| **UI-M3** | 俯视渲染帧率 | **≥ 主干 `slicer_debug_ui` 的 90%** | 同模型同视角，各测 30 秒取均值 |
| **UI-M4** | `SceneRevisionStale` 回滚 | **可演示且状态一致** | 构造并发修改 → 断言回滚后快照与权威一致 |
| **UI-M5** | 切片取消响应 | **≤ 2 s 且无 `.staging` 残留** | 与 14D-04/05 同口径 |
| **UI-M6** | DLL 缺失时启动 | **优雅报错，不崩溃** | 移走 `slicer_module.dll` 后启动 |
| **UI-M7** | **相机操作（orbit/pan/zoom）期间跨 DLL 调用次数** | **恒为 0** | 同 UI-M1 计数器；见 §6.3 |
| **UI-M8** | **3D 视角交互帧率**（10 万三角面场景）| **≥ 30 FPS** | 连续 orbit 30 秒取 P5 |
| **UI-M9** | top 带纹理显示 | 纹理可见；白/近白区域与背景可辨 | checker + 圣诞节浮雕 + 白纹理 fixture |
| **UI-M10** | three_d 带纹理显示 | UV、材质分组和纹理方向正确 | 同一纹理资产与基准截图/采样点对照 |
| **UI-M11** | top/three_d 切换 | scene/selection 不变，mesh/texture 缓存复用 | identity 与调用计数断言 |
| **UI-M12** | 纹理失败 | 显式错误，不出现静默灰模 | missing-texture / no-UV 负例 |
| **UI-M13** | 默认视图设置 | 重启后恢复 | session config round-trip |

UI-M1 是三车道交互是否真正落地的硬证据；UI-M9/M10/M12 则是双视图纹理是否真正落地的
硬证据。拖拽期出现跨 DLL 调用，或带纹理模型出现成功灰模，均不得放行。

UI-M3 取 90% 而非 100%：新宿主渲染后端与主干 QPainter 路径不同，完全持平并非合理合同；
低于 90% 则说明本地缓存、批量上传或刷新节流存在问题。逐帧渲染本身不得跨 ABI。

## 6. 3D 视角能力（v1.1 新增需求 · 用户 2026-08-04 提出）

**现状（A）**：UI 只有 `+Z` 俯视工作区（`REPORT_13A_02`，`ModelTopViewWidget`），
底层由 `src/slicer_core/scene/SceneViewGeometry.*` 产出俯视几何。

**目标（B）**：宿主提供 `top` 与 `three_d` 两种模型显示规格，在设置层选择默认规格，
并允许在中央画布即时切换。对带纹理模型，两种规格都必须显示模型纹理，这是硬性验收标准。

### 6.1 相机操作（参考 Cura / PrusaSlicer / Bambu Studio / Chitubox / Lychee）

| 操作 | 业界常见绑定 | 本项目建议 | 说明 |
|---|---|---|---|
| 轨道旋转 Orbit | 右键拖拽（Cura/Bambu）或左键空白拖拽（PrusaSlicer）| **右键拖拽** | 避开左键选择冲突；须可配置 |
| 平移 Pan | 中键拖拽 / `Shift`+右键 | **中键拖拽** | |
| 缩放 Zoom | 滚轮 | **滚轮，以光标位置为中心** | 「以光标为中心」是体验分水岭，不可省 |
| 适应窗口 Fit | 双击空白 / 快捷键 | 快捷键 + 工具栏 | 按当前选中或全场景两种模式 |
| 视角复位 Home | 快捷键 | 回默认等轴测 | |

### 6.2 预设视角、投影与场景要素

```text
预设视角  顶 / 底 / 前 / 后 / 左 / 右 / 等轴测（七向），以 ViewCube 或工具栏呈现
投影      透视（Perspective）⇄ 正交（Orthographic）可切换
场景要素  打印平台网格 + 幅面边界 · 坐标轴指示 · 原点标识 · 越界高亮
模型 gizmo 选择（单选/框选）· 移动 · 旋转 · 缩放 · 镜像 · 落台(drop to bed)
```

双视图网格的机器可读单一真源为 `contracts/slicer_ui_view_spec.json`：

| 项 | top | three_d |
|---|---|---|
| 平面 | XY @ Z=0 | XY @ Z=0，另画构建体积线框 |
| 范围 | `scene.buildVolume.widthMm × heightMm` | 同左；有 `zLimitMm` 时线框延伸到 Z 上限 |
| 小格 / 大格 | 1 mm / 10 mm | 1 mm / 10 mm |
| 自适应 | `<4 px/mm` 隐藏小格，大格常显；`≥2 px/mm` 显示刻度标签 | 同左，并启用深度测试 |

`230 × 100 × 60 mm` 只是产品 fallback，不得写死在渲染器；设备 Profile 可覆盖。网格仅为显示辅助，
不得参与几何真值、切片采样或 TIFF 输出。两种视图共享同一 XY 网格规格，避免切换时尺度感跳变。

> ⚠️ **正交投影对切片软件是硬需求，不是可选项。** 透视会让"是否越界""两个实例是否对齐"
> 产生视觉误判。**俯视模式必须强制正交**；3D 模式默认正交、允许切到透视做观感预览。

### 6.3 关键架构判断：3D 视角**不新增任何权威求值**（P）

这是本节最重要的一条：

```text
3D 视角新增的全部是【呈现层 + 拾取层】：
  · 相机 orbit / pan / zoom  → 纯本地矩阵运算，零 DLL 调用
  · 拾取（射线求交）          → 本地几何，仅用于选中反馈
  · 模型变换                  → 仍走 Transient → Commit 三车道，权威判定仍在 DLL/Worker
  · 拖拽期碰撞提示             → 宿主本地 bbox 预测，非权威，零 DLL 调用
  · Commit 后碰撞 / 越界       → scene.apply_operation 权威返回
  · 静止场景显式碰撞检查       → 可调用 geometry.collision
```

因此 UI-M1（拖拽期零跨 DLL 调用）在 3D 下依然成立，并新增 **UI-M7**：相机操作期间同样恒为 0。

本项目已有的实例变换权威求值（13A 系列 + `layout/` + `scene/`）**全部复用，不重写**。
3D 只是换了一种把同一份权威状态画出来的方式。

### 6.4 受控合同修订：从几何 ViewData 扩展为带纹理双视图

14A-04 已冻结 position/normal/index、LOD、local mesh、双身份缓存和 blob 分块，
但没有 UV、材质和纹理，因此只能生成灰模。用户确认双视图纹理为硬标准后，
通过 `DOC_DECISION_14A_04_R1_双视图纹理ViewData合同修订.md` 执行受控 minor 修订：

```text
top       surfacePreview RGBA8/sRGB（合同响应必需，不由宿主自行投影替代）
three_d   position + normal + texcoord0 + index + submesh/material/texture
缓存      meshIdentity / appearanceIdentity / textureIdentity / previewIdentity 分离
传输      mesh、texture、surfacePreview 均复用 read_blob 分块，不新增 ABI 导出
失败      声明纹理但读取/解码/UV 失败时显式失败，不得静默灰模
```

新增 `14B-03A TexturedSceneViewDataProvider` 负责真正提供上述数据。只有字段冻结而没有
Provider 不能解锁 14E-04c。

### 6.5 视角切换设置项

```text
位置    设置层（与现有 Profile / 输出设置同级）
选项    默认视角：[ 俯视 ] / [ 3D ]
        投影：   俯视强制正交（不可改）；3D 默认正交，可切透视
        相机绑定：预设方案（Cura 风格 / PrusaSlicer 风格）+ 自定义
持久化  写入 session config，随工作区恢复
规格    contracts/slicer_ui_view_spec.json（视图枚举、网格、纹理对比与切换不变量）
```

中央画布同时提供 `[俯视 | 3D]` 即时分段切换；设置项只决定下次进入工作区的默认值。
切换必须**保持场景状态、选中集、纹理缓存与作业状态不变** —— 只换相机和呈现策略，不动数据。

白色/近白纹理不得与背景混淆：画布使用非纯白平台背景、轮廓线和选中高亮；透明区域使用棋盘格，
但这些辅助显示不得改变纹理像素或生产数据。

### 6.6 打印幅面：默认 230 × 100 × 60 mm（v1.2 新增 · 用户 2026-08-04 指定）

3D 视角要绘制打印平台与构建体积边界，必须有明确的幅面尺寸。

```text
默认幅面   X = 230 mm   Y = 100 mm   Z = 60 mm
可配置     是 —— 后续按设备 Profile 修改，不得硬编码在渲染层
来源       buildVolume.source = "device_profile"
```

#### 🔴 6.6.1 阻塞缺口：现有 schema 装不下 Z（A · 本轮新发现）

`src/slicer_core/scene/MultiModelScene.h:149-160`：

```cpp
/**
 * @brief Optional printable XY volume and its provenance.   // ← 注释明写 XY
 */
struct SceneBuildVolume
{
    BuildVolumeSource     source{BuildVolumeSource::Unresolved};
    std::optional<double> widthmm;    // X
    std::optional<double> heightmm;   // Y  ⚠️ 是 Y，不是 Z
    BuildVolumeOrigin     origin{...};
    BuildVolumeAxisDirection xdirection{...};
    BuildVolumeAxisDirection ydirection{...};
    bool isfixture{false};
};
```

**`buildVolume` 是二维幅面，没有任何 Z 限高字段。** 因此：

| 后果 | 严重度 |
|---|---|
| 3D 视角**画不出构建体积盒子**（只能画一张平面）| 中 |
| **模型超高无法判定** —— 切片会成功，但实物打不出来 | **高** |
| `buildVolume.heightMm` 极易被误读为"高度=Z"，实为 Y | 中 |

> ⚠️ 不要拿 `autoOrient.maxHeightMm`（现有配置里是 9.0 / 6.0）当限高用 ——
> 那是**自动定向的目标高度**，语义是"尽量把模型压到这么矮"，
> 与"设备物理上最高能打多高"完全是两回事。混用会同时错两处。

#### 6.6.2 处置：新增 `zLimitMm`，不重命名既有字段（P）

```cpp
struct SceneBuildVolume
{
    // ... 既有字段保持不变 ...
    std::optional<double> zlimitmm;   // 新增：Z 方向物理限高（mm）
};
```

对应 JSON（`slicesoft.multimodel_scene.13b.1`）：

```jsonc
"buildVolume": {
  "source": "device_profile",
  "widthMm": 230.0,     // X
  "heightMm": 100.0,    // Y  —— 既有命名，含义是 Y，不是 Z
  "zLimitMm": 60.0,     // Z  —— 新增
  "origin": "...", "xDirection": "...", "yDirection": "..."
}
```

**为什么叫 `zLimitMm` 而不是 `depthMm` 或 `heightMm`（P）**：

```text
业界惯例是 X=width / Y=depth / Z=height，但本项目已把 Y 命名为 height 并广泛使用。
此时再把 Z 叫 height 会直接冲突，叫 depth 又与业界 depth=Y 相反 —— 两条路都制造歧义。
zLimitMm 显式带轴名，读者不需要记忆本项目的历史命名，一眼可辨。

【不做破坏性重命名】：把 heightMm 改成 depthMm 会波及 schema、配置、
fixture、golden 与打印侧对接，收益远小于代价。命名瑕疵就地标注即可。
```

**向后兼容**：`zLimitMm` 为 `std::optional`，缺省时行为与现状完全一致（不做 Z 校验），
既有场景配置与 golden **零影响**。

#### 6.6.3 由此产生的能力面影响

| 项 | 影响 |
|---|---|
| `scene.get_snapshot` / `apply_operation` | 响应中的 `buildVolume` 需带 `zLimitMm` |
| **Z 超限判定** | 见 §6.6.4 —— **与视角解耦** |
| 3D 渲染 | 用 `widthMm × heightMm × zLimitMm` 画构建体积线框 |
| **俯视渲染** | **忽略 Z**，只画 `widthMm × heightMm` 平面（用户 2026-08-04 确认）|
| 14A-02 JSON Schema | 须覆盖新字段 |

#### 6.6.4 渲染与判定必须分开（用户澄清 + 我方补充）

用户指出「幅面 Z 信息只在 3D 视角有用，俯视可忽略 Z」。**渲染层面完全采纳**：

```text
3D 视角    画 width × height × zLimit 的构建体积线框
俯视视角   只画 width × height 平面，【完全忽略 Z】—— 不画高度、不做任何 Z 相关提示装饰
```

**但判定层面不能跟着视角走。** 若把 Z 超限判定也绑到 3D 视角，就会出现这种情形：

```text
用户全程用俯视排版作业 → 从不切到 3D → 永远看不到超高提示
→ 切片成功、包合法、RIP 通过 → 到打印环节才发现打不出来
```

因此：

| 维度 | 是否随视角变化 |
|---|---|
| **构建体积的绘制** | ✅ 随视角 —— 3D 画盒子，俯视只画平面 |
| **Z 超限判定与告警** | ❌ **不随视角** —— 任何视角下都判、都提示 |

**判定强度取"告警"而非"硬阻断"**（P）：切片包本身是合法的 `p0.rgbwsv.2` 产物，
设备也可能更换；`zLimitMm` 缺省时则完全不判。这样既不误伤，也不让问题静默通过。

> 一句话：**Z 在"怎么画"上跟视角走，在"对不对"上不跟视角走。**

### 6.7 落地位置与范围

| 项 | 决定 |
|---|---|
| 首版实现位置 | **`apps/slicer_ui_host_sim/`** —— 作为打印软件的参考实现（§1.1）|
| 主干 `slicer_debug_ui` | **本阶段不动**（§2.4）。是否回迁随后续 UI 迁移决策 |
| 权威求值 | 复用既有，**不新增** |
| 对 14A-04-R1 的约束 | 双视图纹理字段按 §6.4 与 `DOC_SCHEMA_14_SceneViewData网格DTO规格.md` v1.2 冻结 |
| 对 14A-11 的约束 | `SceneBuildVolume` 须新增 `zLimitMm`，默认幅面 230×100×60 mm（§6.6）|

### 6.8 渲染后端决策

| 候选 | CMake/依赖 | 许可证与部署 | 维护风险 | 结论 |
|---|---|---|---|---|
| `QOpenGLWidget + QOpenGLFunctions` | 使用现有 Qt5 Widgets/Gui，不新增 vcpkg 包 | 沿用项目 Qt 5.15 策略 | shader、拾取和资源生命周期由项目维护 | **首选** |
| Qt3D | 新增 Qt3D 组件与 Runtime | 仍为 Qt，但扩大部署面 | API/模块维护和打印侧移植风险较高 | 首版不采用 |

`CameraController`、`ViewModeSwitch`、`SceneInteractionController` 不得依赖 OpenGL 类型；
渲染器只消费宿主本地 DTO 和缓存资源，便于打印侧改用自己的渲染后端。

### 6.9 信息架构

行业依据与正式决策见 `DOC_DECISION_14A_04_R1_双视图纹理ViewData合同修订.md` §7。
宿主首屏直接进入可操作工作台，不增加营销/说明首页：

```text
顶部：准备 | 切片预览 | 模块诊断；导入、切片、取消、模块状态
左侧：模型/实例列表、可见性、选择和场景身份
中央：top/three_d 带纹理画布；相机、视角、适应窗口和构建体积
右侧：唯一 Context Inspector，显示变换、Profile、预检、碰撞和越界
底部：任务详情 | 层预览 | 模块日志
```

“准备”负责模型和场景；“切片预览”只读生产 Package/TIFF；“模块诊断”展示 ABI/Worker
版本、能力和错误，不与业务设置混在同一面板。

## 7. 与其它任务的接口

| 关联 | 说明 |
|---|---|
| **M-MVP-CANDIDATE（14C-06 + 14D-05）** | 解锁 14E-01；14E-01 PASS 后才形成 M-MVP |
| **14A-04-R1** | 冻结 top/three_d 的 UV、材质、纹理和 surfacePreview |
| **14B-03A** | 实现 TexturedSceneViewDataProvider；是 14E-04c 的硬前置 |
| **14B-00** | `model.import` 归属结论会影响 P0 第一项走 DLL 还是 Worker；不影响是否覆盖 |
| **14E-05 拆分** | 主干 `MainWindow`(3659) / `UiSmokeTestRunner`(6963) 的拆分**与本专项解耦** —— 本专项不改主干，拆分按 `INT_11` 独立推进 |
| **14B-06 行数门禁** | 新 app 的文件从第一天起就受门禁约束（≤ 500 行/文件），不进白名单 |
| **14E-06 可移植清单** | 按 §1.1 要求精确到文件级，供打印侧评估移植成本 |
| **13A 系列** | 实例变换与选择的权威求值全部复用，3D 不重写 |

## 8. 风险

| 编号 | 风险 | 等级 | 缓解 |
|---|---|---|---|
| **UI-R1** | 两套 UI 代码并存，短期维护成本上升 | 中 | 明确边界：新 app **只做宿主模拟与参考实现**，不追求功能完整，不承担生产职责 |
| **UI-R2** | 新 app 沦为"只跑得通的玩具"，暴露不出真实问题 | **高** | 由 §4 覆盖档与 §5 十三项可测指标共同约束；交互调用计数、双视图纹理、负例与 FPS 必须留证 |
| **UI-R3** | 跨 ABI 开销导致交互延迟不可接受 | 中 | UI-M2/M3 提前量化；若不达标，结论本身就是对能力面设计的有效反馈 |
| **UI-R4** | 几何 DTO 已冻结但缺 UV/纹理，双视图只能出现灰模 | **高** | `14A-04-R1` 已补充 appearance/texture/surfacePreview；仍需 14B-03A 实现 |
| **UI-R5** | 3D 渲染依赖影响参考实现可移植性 | 中 | 首版定为 QOpenGLWidget；相机/交互与渲染后端解耦 |
| **UI-R6** | **`buildVolume` 无 Z 限高 → 模型超高不可判定，切片成功但打不出来** | **高** | §6.6 已定处置（新增 `zLimitMm`，optional 向后兼容）；由 **14A-11** 落地。**该风险独立于 3D 视角，即使不做 3D 也应修** |
| **UI-R7** | 白色/近白纹理与白色画布背景混淆 | 中 | 非纯白平台、轮廓、选中高亮和透明棋盘格；不修改纹理像素 |

## 9. 不在本专项范围

```text
✗ 迁移现有 slicer_debug_ui（INT_07 U0–U5，已降级为可选后置）
✗ 拆分主干 UI 大文件（14E-05，按 INT_11 独立推进）
✗ 在主干 UI 上实现 3D 视角（本阶段主干不动，见 §2.4 / §6.6）
✗ 新 app 追求与主干 UI 功能对等
✗ 新 app 承担任何生产职责
```

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-04 | v1.0 | 首版。定案「独立 app target，不开分支」并给出三条理由；澄清与 `INT_07` U0–U5 降级方案的关系；补 15 项能力的 P0/P1/P2 覆盖清单；以 UI-M1..M6 六项可测指标替换原「手感不回归」；定义三条 CI 依赖守卫；登记 UI-R1..R3 |
| 2026-08-04 | v1.2 | ①§6.6 设定默认打印幅面 **230 × 100 × 60 mm**（可配置，来源 `device_profile`）；②**发现并登记 UI-R6** —— `SceneBuildVolume` 只有 `widthMm`/`heightMm`（XY），**无 Z 限高**，导致 3D 画不出构建体积盒子且**模型超高不可判定**；定处置为新增 `zLimitMm`（optional，向后兼容，不重命名既有字段）并说明命名取舍；③网格 DTO 规格已独立成文 `DOC_SCHEMA_14_SceneViewData网格DTO规格.md`，UI-R4 降为已缓解 |
| 2026-08-04 | v1.1 | 按用户四点补充：①§2.4 时序前置改为里程碑 **M-MVP**（14C-06 + 14D-05），M-MVP 前主干 UI 一律不动；②§1.1 强化双重定位为**打印软件参考实现**，加四条对外交付级要求；③新增 §6 **3D 视角能力**（相机绑定/预设视角/投影/场景要素/gizmo 的业界口径、"不新增权威求值"架构判断、视角切换设置项），并新增 UI-M7/M8 两项指标；④**发现并登记能力面缺口 UI-R4** —— `scene.get_viewdata` 的网格 DTO 从未定义，列为对 14A-04 的硬性输入 |
| 2026-08-05 | v1.3 | 受控修订已冻结合同：top/three_d 均强制纹理；增加 appearance/texture/surfacePreview、14B-03A Provider、UI-M9..13；修正 M-MVP 循环、拖拽碰撞和 Commit 额外快照；定案 QOpenGLWidget 与三工作区信息架构 |
