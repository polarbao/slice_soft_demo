# DOC_ANALYSIS 能力边界与 UI 融合：四问核实

> 文档状态：**ACTIVE / ANALYSIS** ｜ 版本：v1.0 ｜ 日期：2026-08-10
> 定位：回答「排版显示该不该进库 / 3D 为何粗糙 / 能否迁移 / 布局如何对齐」四问
> 证据等级：A=已核实代码事实，B=目标设计，P=判断

---

## 1. 排版与显示应否进切片库

### 1.1 先纠正一个前提（A）

`src/slicer_module/module.json.in` 的 15 项能力**全文**：

```text
model.import · model.get_metadata · model.release
scene.apply_operation · scene.get_snapshot · scene.get_viewdata
geometry.preflight · geometry.collision · geometry.repair
slice.rgbwsv
package.verify · package.get_summary · package.get_layer_descriptor
package.render_layer_preview · package.read_report
```

**没有独立的 `scene.layout` 能力。** 排版是 `scene.apply_operation` 的一个 operation 取值
（`applyGridLayout`，H-A-04 加入）；显示是 `scene.get_viewdata` 一项。

⇒ 这同时回答了打印侧 `CLD_42` 的 **T-01**（该文自称未能读到 15 项清单）：
**排版没有占用独立能力位，"要不要 layout" 不是"多一项能力"的问题。**

### 1.2 显示必须进库 —— 理由不是功能，是几何真源唯一性（P）

```text
若库不提供 get_viewdata，宿主要显示模型就必须【自己再解析一次 OBJ/3MF】
⇒ 出现两个几何真源
⇒ 用户看到的模型 与 实际被切的模型 可能不是同一个
   （自动定向 rotate_x_90、高度限制 maxHeightMm=9、修复后资产替换……
     这些变换都发生在库内，宿主自己解析的原始文件里没有）
```

**这是致命的，不是体验问题。** 所以显示必须进库。

但边界要说准：**库提供的是「显示数据」，不是「渲染」。**

```text
库负责   网格 / UV / 材质 / 纹理 / bbox / 轮廓 / 世界矩阵   ← 数据
宿主负责 相机 / 光照 / 拾取 / 后端选择 / 帧率               ← 渲染
```

当前 `IRenderBackend` 与三车道设计正是这个切法，方向正确。

### 1.3 排版应进库 —— 但理由常被说错（P）

常见说法是"排版属于切片前处理"。**这个论证站不住** —— 按同样的逻辑，
文件对话框也算前处理。

**真正的理由是：排版结果要参与碰撞与幅面校验，而这两个判定必须与切片用的是同一套几何。**

```text
若宿主自己算落位再逐个 translate：
  宿主用【自己的 bbox】判不碰撞  →  库用【变换后真实几何】判碰撞
  ⇒ 两边结论可能相反，且宿主先斩后奏，用户看到的是"排好了"，切片时才报错
```

`geometry.collision` 已经在能力表里 —— **有碰撞判定却没有排版执行，是不完整的**。

⚠️ **但要区分「策略」与「执行」**：

```text
排版策略（行列数 / 间距 / 起始角）  → 归【宿主】，是产品与设备参数
排版执行（按给定策略落位并返回权威结果）→ 归【库】，因为它要与碰撞同源
```

当前 `applyGridLayout` 的参数（`maxColumns/maxRows/columnGapMm/rowGapMm`）由宿主给，
库只执行 —— **形态是对的**。

### 1.4 对打印侧 CLD_27「方案 C 只要 5 项」的评估（P）

方案 C 去掉 layout 后，**22 实例排版的碰撞判定失去权威来源**。
打印侧要么自己实现一遍（两套算法必然漂移），要么放弃排版期碰撞检查（生产风险）。

⇒ **建议保留 layout**。若打印侧坚持精简，可退一步：
保留 `scene.apply_operation/applyGridLayout` 但**在打印侧不暴露 UI**，
仅作为内部保障 —— 成本为零，因为它已经实现并交付了。

---

## 2. 🔴 3D 显示粗糙的根因：整个 ViewData 只有面法线

### 2.1 A 级证据

`src/slicer_core/api/viewdata/SceneViewMeshBuilder.cpp`：

```cpp
:35   std::optional<Vec3> ComputeNormal(const Triangle& triangle)   // ← 逐【三角面】
:366  const std::optional<Vec3> normal = ComputeNormal(triangle);
:385  ... *normal ...        // ← 同一个法线发给该三角的【三个顶点】
```

⇒ **每个三角面都是平的，曲面被渲染成一堆刻面。**
甲片浮雕（爱神 / 玫瑰 / 奶油）恰恰是**大面积连续曲面**，是这种缺陷最刺眼的场景。

### 2.2 为什么会这样（A）

`src/slicer_core/model.h` 与 `src/slicer_core/scene/MultiModelScene.h` 中
**`normal` 零命中** —— 切片库的模型表示**根本不存法线**，OBJ 的 `vn` 在导入时即丢弃。

**这在切片语境下是合理的**：切片只需三角形做射线/平面求交，法线无用。
但渲染路径因此**只能现场推导**，而现在推导的是面法线。

### 2.3 这同时解释了 R-A-02 的一个异常（A）

`REPORT_RENDER_R_A_02` 记录：顶点共享后实测 **105.15 B/三角**，远未达理论 28 B/三角，
文中归因为「当前 ViewData 只有面法向，真实曲面大部分顶点不能安全共享」。

顶点 key 是 `MakeVertexKey(point, normal, uv)`（`:243`）——
**面法线导致相邻三角的顶点位置相同但法线不同，永远无法合并。**

### 2.4 ⇒ 一个修复同时解决两个问题（P）

```text
计算【平滑顶点法线】：按面积/角度加权平均相邻面法线 + crease angle 阈值（硬边保留分裂）

收益①  曲面不再刻面 —— 这是"粗糙"的主因，视觉改善最大
收益②  顶点 key 真正可合并 —— 105 B/三角 有望向 28 B/三角 靠拢，
        进而缓解纹理/网格预算压力
```

⚠️ **crease angle 必须可配**：甲片模型既有连续曲面也有明确硬边（边缘、底面），
一刀切平滑会把硬边抹圆，反而更不像。建议默认 30°–45°，实测调。

### 2.5 次要因素（P，不是主因）

```text
· 22 资产聚合场景返回 lod2（每模型简化到 10k 面）
  但 R-B-02 后这是 meshoptimizer 属性感知简化，不再是破洞跳采样 —— 影响远小于法线
· cpu_raster 后端无各向异性过滤/mipmap，纹理在斜视角会糊
  → 属 R-D 后端议题，不应与法线问题混为一谈
```

**结论：先修法线，再谈其他。** 不修法线而去换渲染后端，粗糙感依然存在。

---

## 3. 能否迁移到打印软件

### 3.1 两个不同的迁移面，成本差一个量级（A）

```text
主干 slicer_debug_ui        77 头文件 → A=6 可直接复制 / B=41 需改写 / C=30 不移植
                            H-C-02 估 38–59 人日
参考宿主 slicer_ui_host_sim  本就只经 11 个导出、不 include slicer_core
                            【边界已经是干净的】—— 这才是给打印侧参照的那份
```

⚠️ **不要用 38–59 人日去估参考宿主的迁移成本**，那是主干 debug UI 的数字。

### 3.2 三种融合形态（P）

| 形态 | 做法 | 评价 |
|---|---|---|
| 甲 · 源码移植 | 参考宿主的 widget 复制进 PrintApp | 可行（两侧同为 Qt5.15/C++20/MSVC），但要逐个处理主题、导航、设备上下文 |
| **乙 · 作为一个 Page 嵌入** | 把切片工作区包成 `SlicePage` 塞进 `m_pageStack` | ✅ **推荐**，见 §4 |
| 丙 · 独立进程 | 切片器单独起进程 | 体验割裂；且已有 Worker 进程，再加一层无收益 |

### 3.3 🔴 真正的接缝：设备中心 vs 场景中心（A）

`PrintApp/src/presentation/views/MainWindow.h`：

```cpp
QString m_curDevId;                          // :89
QMap<QString, QtSdkBridge*> m_bridges;       // :90  每设备一个 bridge
void SwitchToDevice(const QString& devId);   // :49
```

**PrintApp 是设备中心的**；而参考宿主是**场景中心的，完全没有设备概念**。
而 `buildVolume` 在 PrintApp 里来自**选中的设备**。

⇒ 切片页必须处理一条两边现在都没设计的生命周期：

```text
当前设备切换 → buildVolume 变化 → 已有场景的幅面校验失效 → 场景要重建或重新校验
```

而 `H-B-05` 已确立「场景创建后 Profile/buildVolume 成为权威身份，异值须新建场景」——
**这条规则与 PrintApp 的自由切设备直接冲突，必须先对齐。**

### 3.4 ⚠️ 另一个预期差异（A）

`MainWindow.h:64` 已存在 `void OnImportSliceRequested();`

⇒ **打印侧当前的设计预期是「导入切片产物」，不是「内嵌切片器」。**
这与切片侧"打印软件将参照参考宿主完成移植"的假设不一致。**这个差异必须先书面对齐**，
否则 §3.2 选哪个形态都会白做。

---

## 4. 参照打印软件布局改造参考宿主

### 4.1 两侧布局现状（A）

```text
PrintApp（MainWindow.h:5-8 注释即为权威描述）
    TopBar (40px)
    NavSidebar(64) | PrinterListPanel(260) | ContentArea(QStackedWidget) | RightPanel(320)
    StatusFooter (28px)
    页：PrintControlPage · FileManagerPage · SettingsPage · DeviceDiagPage · IntegratedTestPage

参考宿主（HostMainWindow.cpp:85-216）
    QTabWidget【工作区 / 结果 / 设置 / 模块诊断】+ QSplitter
    —— 是一个【顶层分页的独立 app】，不是 shell 内的一页
```

### 4.2 建议的映射（P）

| 参考宿主现状 | 改造目标 | 说明 |
|---|---|---|
| 「工作区」Tab | **`SlicePage`**，进 `m_pageStack` | 切片器整体降为**一页**，不再是顶层 app |
| 模型列表面板 | 左侧列表位（仿 `PrinterListPanel` 260px） | 结构同构，便于复用主题与交互习惯 |
| Profile / 参数面板 | 右侧检查器位（仿 `RightPanel` 320px） | 同上 |
| 「结果」Tab | 并入 `PrintControlPage` 或 `FileManagerPage` | 生产包属于打印域，不属于切片页 |
| 「模块诊断」Tab | 并入 `DeviceDiagPage` | 同上 |
| 「设置」Tab | 并入 `SettingsPage` | 同上 |

**改造后参考宿主仍能独立运行**（保留一个极简 shell 承载 `SlicePage`），
这样两边都能跑，A/B 对照不丢。

### 4.3 收益与代价（P）

```text
收益   打印侧移植时【不需要重新设计信息架构】，只需把 SlicePage 挂进 m_pageStack
       主题、Toast、状态栏、导航全部复用 PrintApp 既有设施
代价   参考宿主要做一次结构性重排；H-D/H-E 刚完成的接线要跟着调整位置
       （逻辑不变，只是容器换了 —— 属中等工作量，不是重写）
```

### 4.4 ⛔ 但不建议现在就做

**前置未满足**：§3.4 的预期差异（打印侧要"导入产物"还是"内嵌切片器"）**尚未对齐**。
若答案是"导入产物"，则 §4.2 的整套映射**完全没有必要做**。

⇒ **先对齐 §3.4，再决定 §4 是否立项。** 这是一次跨项目的产品决策，不是切片侧的技术任务。

---

## 5. 派生动作

| 编号 | 动作 | 归属 | 前置 |
|---|---|---|---|
| **N-01** | **平滑顶点法线 + crease angle**（§2）| 切片侧 RENDER 专项，建议新卡 `R-F-01` | 无 —— **可立即开工，收益最大** |
| N-02 | 把 §1.1 的 15 项能力清单回复打印侧，关闭其 T-01 | 跨项目 | 无 |
| N-03 | 书面对齐「导入产物 vs 内嵌切片器」（§3.4）| **跨项目产品决策** | 无 |
| N-04 | 设备切换 ⇄ buildVolume ⇄ 场景生命周期的冲突规则（§3.3）| 跨项目 | N-03 |
| N-05 | 参考宿主改为 `SlicePage` 形态（§4.2）| 切片侧 | **N-03 结论为"内嵌"才立项** |

## 6. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-10 | v1.0 | 首版。核实 15 项能力全文（**无独立 `scene.layout`**，回答打印侧 T-01）；论证显示进库的理由是几何真源唯一性、排版进库的理由是与碰撞同源，并区分策略/执行归属；🔴 定位 3D 粗糙的根因为 `SceneViewMeshBuilder.cpp:366/385` **只有面法线**，且模型层根本不存法线，同时解释 R-A-02 的 105 B/三角异常；区分主干 UI 与参考宿主两个迁移面；核实 PrintApp 为设备中心 + Page 导航壳，指出 `buildVolume` 生命周期冲突与 `OnImportSliceRequested` 暴露的预期差异；给出布局映射方案并说明为何应先对齐预期再立项 |
