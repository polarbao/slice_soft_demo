# DEMO_14 切片能力包封装与打印软件集成验收方案

> 文档状态：**PREPARED / NOT EXECUTED**
> 版本：v1.1 ｜ 日期：2026-08-03 ｜ 双视图纹理修订：2026-08-05
> 上游：`PRD_14`、`DEV_14`、`DOC_DECISION_14`
> 证据纪律：**未执行项保持 `NOT RUN`，不得以 PREPARED 代替 PASS**

---

## 0. 验收矩阵总览

| 组 | 主题 | 用例数 | 依赖 |
|---|---|---:|---|
| D14-A | 契约物料与合规 | 6 | 无 |
| D14-B | Facade、ViewData Provider 与 base/engine 分层 | 11 | 14B |
| D14-C | ABI 与一致性（C-SPI）| 18 | 14C |
| D14-D | Worker、取消与引擎替换 | 12 | 14D |
| D14-E | 交互、双视图纹理与可移植性 | 18 | 14E |
| D14-F | 打包、稳定性与三方联调 | 11 | 14F |
| **合计** | | **76** | |

---

## 1. D14-A 契约物料与合规

| 编号 | 用例 | 判据 | 状态 |
|---|---|---|---|
| D14-A-01 | `contracts/print_module_spi.h` 可被 C 与 C++ 编译器分别编过 | 无错误无告警 | NOT RUN |
| D14-A-02 | 该头**不含**任何 STL / Qt / `slicer_core` 内部类型 | 静态检查通过 | NOT RUN |
| D14-A-03 | `p0.rgbwsv.2.schema.json` 校验真实 manifest 样例 | 通过 | NOT RUN |
| D14-A-04 | `file_contract_v1.md` 含请求/结果 schema、进度行、退出码全表 | 内容完整 | NOT RUN |
| D14-A-05 | 第三方许可证与 NOTICE（assimp / miniz / libtiff）成文可分发 | 审查通过 | NOT RUN |
| D14-A-06 | 与打印侧 `print_module_spi.h` **逐字节相同** | diff 为空 | NOT RUN |

## 2. D14-B Facade 与分层

| 编号 | 用例 | 判据 | 状态 |
|---|---|---|---|
| D14-B-01 | 每个 facade 有正/负例单测 | 全绿 | NOT RUN |
| D14-B-02 | facade 不抛异常越界；一律 `ApiResult` | 静态 + 单测 | NOT RUN |
| D14-B-03 | 所有耗时 facade 接受 `ICancelToken` | 接口审查 | NOT RUN |
| D14-B-04 | `slicer_base` 编译单元不 include engine 头 | CI 依赖检查 | NOT RUN |
| D14-B-05 | `slicer_module` 链接闭包无 engine 符号 | link map 分析 | NOT RUN |
| D14-B-06 | 新增源文件必须显式归入 base 或 engine | CI 检查 | NOT RUN |
| D14-B-07 | `slicer_cli` 改走 facade 后行为不变 | TIFF 逐字节不变 + full 回归 | NOT RUN |
| D14-B-08 | `model.import` 归属结论（base 或 Worker）有明确证据 | 14B-00 结论文档 | NOT RUN |
| D14-B-09 | `TexturedSceneViewDataProvider` 为 top 返回可解码 `surfacePreview`，世界边界与透明语义完整 | checker 3MF + 圣诞节浮雕正例通过 | NOT RUN |
| D14-B-10 | Provider 为 three_d 返回 `mesh + texcoord0 + submeshes + materials + textures`，identity 可复用 | UV/材质绑定和纹理方向与源资产一致 | NOT RUN |
| D14-B-11 | 声明纹理但缺文件、解码失败或 UV 无效 | 稳定返回 `INPUT-0001/0002`，不得成功返回灰模 | NOT RUN |

## 3. D14-C ABI 与一致性（C-SPI-01..18）

由宿主提供的 `test_module_spi_conformance` 对 `module.json` 运行，18 项全绿。

| 编号 | 检查 | 状态 |
|---|---|---|
| C-SPI-01 | `pm_spi_version()` == 宿主 `PM_SPI_VERSION` | NOT RUN |
| C-SPI-02 | `pm_module_info` 合法 JSON 且过 schema | NOT RUN |
| C-SPI-03 | `runtime`/`buildConfig` 不符时宿主拒绝装载 | NOT RUN |
| C-SPI-04 | `pm_create`/`pm_destroy` 循环 100 次，内存增长 < 1MB | NOT RUN |
| C-SPI-05a | `out=nullptr, cap=0` → `PM_ERR_BUFFER_SMALL` 且 `required>0` | NOT RUN |
| C-SPI-05b | `cap = required`（差 1）→ 错误码且**缓冲未被写**（哨兵验证）| NOT RUN |
| C-SPI-05c | `cap = required+1` → 返回 `required`，末尾 `'\0'` | NOT RUN |
| C-SPI-06 | 提交 → 轮询到终结 → `pm_result` 闭环 | NOT RUN |
| C-SPI-07 | 进度单调不回退（记录全序列）| NOT RUN |
| C-SPI-08 | `pm_cancel` 后 ≤ `cancelLatencyMs` 进 `cancelled` | NOT RUN |
| C-SPI-09 | 取消后 `.staging` 不存在 | NOT RUN |
| C-SPI-10 | 错误码匹配 `^PM-[A-Z]+-[A-Z0-9]+-\d{4}$` | NOT RUN |
| C-SPI-11 | 非法请求 JSON（12 变体）→ 稳定码，不崩溃 | NOT RUN |
| C-SPI-12 | 所有函数传 `nullptr` 句柄 → `PM_ERR_INVALID_ARG` | NOT RUN |
| C-SPI-13 | `pm_result` 在 running 态 → `PM_ERR_INVALID_STATE` | NOT RUN |
| C-SPI-14 | `pm_destroy` 时仍有未 release 的 job → 不崩溃不泄漏 | NOT RUN |
| C-SPI-15 | `pm_cancel` 重复调用 → 均返回 `PM_OK` | NOT RUN |
| C-SPI-16 | `dumpbin /EXPORTS` 恰好 11 个 `pm_*`，无 C++ 修饰名 | NOT RUN |
| C-SPI-17 | `dumpbin /DEPENDENTS` 不含 `PrintSDK.dll` / `Qt5*.dll` | NOT RUN |
| C-SPI-18 | `pm_self_test` 返回合法 JSON，无持久化副作用 | NOT RUN |

## 4. D14-D Worker、取消与引擎替换

| 编号 | 用例 | 判据 | 状态 |
|---|---|---|---|
| D14-D-01 | `--contract-info` 返回合法 JSON，major 与 DLL 匹配 | 通过 | NOT RUN |
| D14-D-02 | 篡改 Worker major → DLL 拒绝执行并报稳定码 | fail-closed | NOT RUN |
| D14-D-03 | Worker minor 更高 → 允许运行 | 通过 | NOT RUN |
| D14-D-04 | graceful cancel：各阶段各取消一次 | ≤2s 进 `cancelled` | NOT RUN |
| D14-D-05 | 取消后 `.staging` 清理干净（DLL 与 Worker 双保险）| 无残留 | NOT RUN |
| D14-D-06 | 强杀 Worker → 宿主感知、报错、无僵尸进程 | 通过 | NOT RUN |
| D14-D-07 | 磁盘满 → 预期退出码 + staging 清理 | 通过 | NOT RUN |
| D14-D-08 | 重复 `jobId` → 幂等或明确拒绝，identity 不混淆 | 通过 | NOT RUN |
| D14-D-09 | **引擎替换**：换 Worker 后过 E-01..08，宿主零改动 | 通过 | NOT RUN |
| D14-D-10 | E-03 golden checksum：一致，或 release note 显式声明并更新基线 | 无静默漂移 | NOT RUN |
| D14-D-11 | Worker 独立运行 `--spi-request` 可调试 | 可附加调试器 | NOT RUN |
| D14-D-12 | `result.json` 含 `engineVersion` 可追溯 | 字段存在 | NOT RUN |

## 5. D14-E 交互与可移植性

| 编号 | 用例 | 判据 | 状态 |
|---|---|---|---|
| D14-E-01 | `slicer_host_sim` 仅通过 11 个导出完成导入→变换→切片→取包→校验 | 通过后由 M-MVP-CANDIDATE 晋级为 M-MVP | NOT RUN |
| D14-E-02 | **`mouse-move` 期间 `Host→DLL` 调用次数为 0** | 计数为 0（可证伪）| NOT RUN |
| D14-E-03 | Commit 返回 `SceneRevisionStale` 时 UI 重取重试成功 | 通过 | NOT RUN |
| D14-E-04 | 碰撞/越界时回滚显示 + 稳定错误码文案 | 通过 | NOT RUN |
| D14-E-05 | `fast` 预检返回标注 `authoritative: false` | 字段存在 | NOT RUN |
| D14-E-06 | 可移植模块**不 include** `slicer_core/**` | CI 依赖检查 | NOT RUN |
| D14-E-07 | 可移植模块清单交打印侧并确认可移植 | 对方确认 | NOT RUN |
| D14-E-08 | 同一带纹理模型在 `top` 与 `three_d` 都显示真实纹理 | checker 3MF 与圣诞节浮雕均通过；无静默灰模 | NOT RUN |
| D14-E-09 | 白色/近白纹理与背景、透明区可辨 | 非纯白平台、轮廓/高亮、透明棋盘格有效；纹理像素未改写 | NOT RUN |
| D14-E-10 | 设置页选择默认 `top` / `three_d` 后重启 | session config round-trip，默认视图恢复 | NOT RUN |
| D14-E-11 | 中央画布即时切换 top/three_d | scene revision、选中集、实例变换和作业状态不变；mesh/texture identity 复用 | NOT RUN |
| D14-E-12 | orbit/pan/zoom 与视图切换期间 `Host→DLL` 调用次数 | 恒为 0 | NOT RUN |
| D14-E-13 | three_d 连续 orbit（10 万三角面）| 30 秒采样 P5 ≥ 30 FPS | NOT RUN |
| D14-E-14 | top 连续平移/缩放 | 帧率均值 ≥ 主干 `slicer_debug_ui` 的 90% | NOT RUN |
| D14-E-15 | 缺纹理、纹理解码失败、无有效 UV 负例 | 显式错误与恢复动作；不得显示成功灰模 | NOT RUN |
| D14-E-16 | 准备、切片预览、模块诊断三个工作区及固定区域 | 导入排版、层检查、模块自检各有唯一主入口；状态可切换且不丢失 | NOT RUN |
| D14-E-17 | UI 渲染依赖边界 | 使用 Qt5 Widgets/Gui + QOpenGLWidget；不链接 Qt3D、不新增 vcpkg 依赖、不 include `slicer_core/**` | NOT RUN |
| D14-E-18 | 正常 Commit 与恢复调用序列 | 成功直接采用 `apply_operation` 响应，不追加快照；仅 Stale/显式刷新/恢复调用 `get_snapshot` | NOT RUN |

## 6. D14-F 打包、稳定性与联调

| 编号 | 用例 | 判据 | 状态 |
|---|---|---|---|
| D14-F-01 | 干净机仅拷 `modules/slicer/` 即可装载 | 通过 | NOT RUN |
| D14-F-02 | 删除 `modules/` → 宿主正常启动并提示 | 不崩溃 | NOT RUN |
| D14-F-03 | **纯打印路径下切片 DLL 未被装载**（AC-28-04）| `EnumProcessModules` 验证 | NOT RUN |
| D14-F-04 | **边打印边切片**：切片崩溃/OOM 不影响进行中打印 | 打印不中断 | NOT RUN |
| D14-F-05 | 大场景（接近实例上限）自动走 Worker，宿主不 OOM | 通过 | NOT RUN |
| D14-F-06 | 长时连续 10 作业无内存增长/句柄泄漏 | 通过 | NOT RUN |
| D14-F-07 | 单模型 E2E：导入→预检→变换→切片→S1 校验 | 通过 | NOT RUN |
| D14-F-08 | 多模型 E2E：联合切片单一包 + per-instance 统计 | 通过 | NOT RUN |
| D14-F-09 | 拓扑阻断模型 strict 拒绝，**不静默降级** | fail-closed | NOT RUN |
| D14-F-10 | 与打印侧 M1 联调（装载 + 能力 + 自检）| 对方门禁通过 | NOT RUN |
| D14-F-11 | 与 RIP 联调 S2（C1–C7，含 W/S/V 上限 N05）| 通过 | NOT RUN |

## 7. 协议回归（每轮必跑）

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8 (uint8)
polarity = black_is_print / printValue=0 / emptyValue=255
legacy 保持默认；Global 保持显式 opt-in
严格 RIP Reader PASS
30 层 TIFF SHA-256 不变（RepairDisabled）
```

## 8. 缺陷分级与放行

| 级别 | 定义 | 放行 |
|---|---|---|
| S1 致命 | 数据错位 / 材料错误 / 宿主崩溃 / 静默降级 | **必须清零** |
| S2 严重 | 接缝漏判 / 取消残留 / 内存泄漏 / ABI 越界写 | 必须清零 |
| S3 一般 | 文案 / 交互 / 性能不达预期但不影响正确性 | 可带入下一阶段 |
| S4 轻微 | 视觉与措辞 | 记录 |

**阶段放行门槛**：S1/S2 清零 + D14-C 全 18 项绿 + D14-F-03/F-04 通过。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 首版。62 个用例分 6 组；含 C-SPI 18 项、引擎替换 E-01..08 挂钩、`mouse-move` 零跨 DLL 可证伪指标、边打印边切片稳定性专项 |
| 2026-08-05 | v1.1 | 扩为 76 个用例：增加 TexturedSceneViewDataProvider、top/three_d 纹理、白色纹理辨识、默认/即时视图切换、缓存与调用计数、QOpenGL 依赖边界及 Commit 调用序列验证 |
