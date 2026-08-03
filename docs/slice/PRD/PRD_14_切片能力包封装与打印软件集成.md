# PRD_14 切片能力包封装与打印软件集成

> 文档状态：**PREPARED / PENDING USER AUTHORIZATION**
> 版本：v1.0 ｜ 日期：2026-08-03
> 作者：Claude 起草；生效需用户授权
> 决策依据：`docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md`
> 详细分析：`docs/claude/INTEGRATION/INT_06..17`

---

## 1. 目标

把切片软件从"独立可执行的调试工具"演进为**可分发的能力包**，供打印软件（`ry_print_demo / PrintSolution`，C++20 / Qt5.15 / MSVC）以模块形式集成。

```text
交付形态：modules/slicer/{ slicer_module.dll, slicer_worker.exe, module.json, 依赖 DLL }
接入方式：宿主运行时装载（LoadLibraryEx + GetProcAddress），零编译期耦合
能力边界：几何真值 + 切片 + 包查询；不含排版策略、设置管理、RIP、通道化
```

## 2. 用户与场景

| 用户 | 场景 |
|---|---|
| 打印软件开发 | 装载模块 → 协商能力 → 导入模型 → 交互变换 → 提交切片 → 查询包/预览 → 交 RIP |
| 打印软件终端用户 | 在打印软件内完成"导入 → 摆放 → 切片 → 打印"，不感知模块边界 |
| 切片侧开发 | 独立迭代切片引擎，替换 `slicer_worker.exe` 而不触碰宿主 |
| RIP 侧开发 | 消费 `p0.rgbwsv.2` 包，产出 `rip.ch7.1` |

## 3. 功能需求

| 编号 | 需求 | 优先级 |
|---|---|:--:|
| FR14-01 | 提供统一 SPI（11 个 C 导出），宿主可运行时装载并协商版本、运行时、能力 | P0 |
| FR14-02 | 提供模型导入与元数据查询（OBJ/STL/3MF/MTL/贴图）| P0 |
| FR14-03 | 提供几何预检：fast（交互提示）与 full（权威准入裁决）| P0 |
| FR14-04 | 提供变换的权威求值：变换后 bbox、实例间碰撞、幅面越界、变换后可打印性增量 | P0 |
| FR14-05 | 提供切片能力，产出符合 `p0.rgbwsv.2` 的完整包 | P0 |
| FR14-06 | 提供包查询与预览：摘要、层描述、层预览（**从生产 TIFF 解码/合成**）、报告读取 | P0 |
| FR14-07 | 提供 S1 包校验（复用既有严格 Reader）| P0 |
| FR14-08 | 支持三车道交互：Transient（宿主本地）/ Commit（`operationId` + `expectedSceneRevision`）/ Production（只接受已提交 `sceneHash`）| P0 |
| FR14-09 | 支持协作式取消：`Cancelling` 与 `Cancelled` 状态分离，取消后临时目录清理 | P0 |
| FR14-10 | 切片引擎（`slicer_worker.exe`）可独立迭代替换，无需重编 DLL 或改宿主 | P1 |
| FR14-11 | 提供网格修复能力，**默认关闭** | P2 |
| FR14-12 | 提供俯视显示几何数据（`scene.get_viewdata`）| P2 |

### 3.1 明确非目标

```text
不提供：排版摆放策略（packing 引擎）—— 归宿主（排版在 PRD_21..29 零命中）
不提供：设置与 Profile 管理 —— 设备域参数只有宿主知道
不提供：渲染、拾取、相机、gizmo 手感 —— 归宿主 UI
不提供：RIP（分色/半色调/墨量/墨滴量化）—— 归 RIP 模块
不提供：7 逻辑→12 物理通道化 —— 归宿主既有 ChannelSplitter
不提供：作业队列、多任务、持久化 —— 归宿主
不提供：任何 Qt 类型跨边界
本阶段不改：p0.rgbwsv.2 / RGBWSV 顺序 / uint8 / black_is_print / legacy 默认
```

## 4. 非功能需求

| 编号 | 需求 | 判据 |
|---|---|---|
| NFR14-01 | 宿主纯打印路径**不装载**切片模块 | `EnumProcessModules` 检查 DLL 未装载 |
| NFR14-02 | 模块缺失/损坏时宿主仍可正常启动 | 删除 `modules/` 后应用启动且给出提示 |
| NFR14-03 | 切片崩溃/OOM 不得影响宿主进程 | Worker 进程隔离；强杀后宿主可继续 |
| NFR14-04 | 交互能力延迟满足手感 | `scene.apply_operation` 目标 P95 < 200ms @ ≤22 实例（**待 14A 实测标定**）|
| NFR14-05 | 拖拽过程零跨 DLL 调用 | 采集 `Host→DLL` 调用次数，`mouse-move` 期间为 0 |
| NFR14-06 | 取消响应及时 | `pm_cancel` 后 ≤2000ms 进入 `cancelled` |
| NFR14-07 | 无内存泄漏 | `pm_create/destroy` 循环 100 次增长 < 1MB |
| NFR14-08 | ABI 稳定 | 恰好 11 个 `pm_*` 导出，无 C++ 修饰名；不依赖 Qt/PrintSDK |
| NFR14-09 | 产物可复现 | 同输入同 Profile 同引擎版本 → 逐字节一致 |

> ⚠️ **数值预算说明**：NFR14-04 等具体阈值必须在 14A 实测后冻结，本 PRD 不虚构。当前只给目标量级。

## 5. 验收口径

| 编号 | 验收项 |
|---|---|
| AC14-01 | `slicer_module.dll` 通过 C-SPI-01..18 一致性套件（宿主提供）|
| AC14-02 | 宿主可完成"装载 → 能力清单 → 自检"闭环 |
| AC14-03 | 单模型全链路：导入 → 预检 → 变换 → 切片 → S1 校验通过 |
| AC14-04 | 多模型场景：联合切片产出单一包，含 per-instance 统计 |
| AC14-05 | 取消：各阶段各取消一次，均 ≤2s 且无 `.staging` 残留 |
| AC14-06 | 稳定性：**边打印边切片**——切片崩溃/OOM 不影响进行中的打印 |
| AC14-07 | 引擎替换：更换 `slicer_worker.exe` 后通过 E-01..08，宿主无需改动 |
| AC14-08 | 干净机安装：仅拷贝 `modules/slicer/` 即可工作 |
| AC14-09 | `fast` 预检结果标注 `authoritative: false`；准入结论仅来自 Worker 内 `full` |

## 6. 依赖与外部输入

| 依赖 | 状态 |
|---|---|
| 打印侧 `print_module_spi.h`（`CLD_10` 定稿）| ✅ 可同步 |
| 打印侧 `business/platform/` 模块装载器 | ⏳ 对方 M1 |
| RIP 六项确认（墨滴量化/白区语义/压缩/grayBits/dropRange）| 🔴 **阻塞 14F** |
| 设备 buildVolume / 原点 / 轴向 | 🔴 由宿主提供 |
| 三个必需 OBJ 的处置 | 🔴 产品决策；可用 7 个 strict-PASS 资产解耦 |

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。12 条 FR、9 条 NFR、9 条 AC；明确 7 项非目标；标注数值预算待 14A 实测冻结 |
