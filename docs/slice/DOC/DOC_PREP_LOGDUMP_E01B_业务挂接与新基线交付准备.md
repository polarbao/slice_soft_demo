# LOGDUMP E01B 业务挂接与新基线交付准备

> 日期：2026-09-15；LD-E01B-P PREPARATION COMPLETE；正式挂接 LD-E01B 仍为 ENTRY_NOT_IMPLEMENTED。
> 状态唯一真源：[专项任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)。
> 本文记录已完成的前置复核、实施拆分与验收设计；不是新基线已集成或 PrintApp 业务已实现的报告。

## Implementation Plan

### Problem Type

用户要求判断当前专项阶段，具备条件则按计划继续，并先完成准备。LOGDUMP 本地实现和真实打印日志后端适配已完成；剩余任务是正式业务挂接，以及未来交付时的目标基线适配。此次完成可独立执行的准备，不把不存在的业务入口当作可接线代码。

### Layer(s) Involved

切片 DLL 的可选 C 日志扩展、宿主生命周期适配层、PrintApp 的应用级装配/关闭层，以及交付构建和证据。切片几何、通道数据、RIP 墨量和打印设备控制均不由该准备任务改变。

### Official Documents

- [A00 日志扩展定案](DOC_DECISION_LOGDUMP_A00_开发准入与日志扩展定案.md)、[E01 准备](DOC_PREP_LOGDUMP_E01_PrintApp真实日志适配准备.md)、[E01A 验收](../REPORT/REPORT_LOGDUMP_E01A_PrintApp真实日志适配验收.md)。
- 打印仓库 `docs/20_公司正式文档_v3/11_MODULE/MOD_22_模型切片接入.md` §1.2/8.1：本产品适配层和模块装载层未开始。
- 同仓 `docs/20_公司正式文档_v3/08_TEST_RELEASE/ROADMAP_阶段路线图_v3.md` P23：装载、协商、作业、取消和清理由产品接入阶段负责，LOGDUMP 不代建整个 P23。
- 原切片分支 `bc247544` 的 `docs/slice/DOC/DOC_DECISION_P0FIX_R1_模块自述与部署清单能力集受控修订.md`：声明面正在对齐既有 RGBWSVT 实现；其验证和完成状态归 P0FIX 维护。

### Historical Documents

9 月 14 日的 E01A 测试和独立包证据仍可追溯；只证明当时源码/产物。打印 MOD-22 的“11 个业务导出、15 项能力、固定运行库数量”等旧交付事实不能替代当前 SDK 清单。日志已有独立可选 3 导出；能力集及部署内容需按最终 P0FIX 基线重新核对。

### AI Workspace Evidence

| 工作区 | 本轮核对结果 |
|---|---|
| 切片专项 | `E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump`，`codex/feature-logging-dump`，HEAD `e2546797`；专项源码和文档仍未提交 |
| 打印适配 | `E:/__Code/__Work/ry_print_demo-slicer-logdump`，`codex/slicer-logging-adapter`，HEAD `22bfcd3d235d767c45f6a3af3ad7bed6f275a581`；主 CMake 和 integrations 改动仍未提交 |
| 原打印仓库 | `codex/test-engineering-consolidation`，同一 `22bfcd3d235d`；仅既有未跟踪资料，没有新增几何 loader |
| 原切片仓库 | 当前工作在 `codex/feature-p0fix-contract-robustness`；本轮从 `b213b36b` 观察到 `2459f77d`、`554847f3` 和 `bc247544`，另有 P0FIX 合同修订继续进行。下面文件比较固定为 `554847f3`，不宣称它始终是最新 HEAD |

所有比较和证据读取均为只读，未切换、合并、变基、提交或覆盖原工作树。原树有缓存、协作文档和模型资料，另有其他任务进行中的源码/合同编辑，保持原位。

### Current Code Reality

1. PrintApp `AppBootstrap.h:226` 仍创建 `SliceService`，其 TaskKind 仅 None/Scan/SplitChannels，没有几何 DLL/pm_* 生命周期；实际打印装配层不存在可挂日志 adapter 的模块拥有者。
2. 日志 adapter 借用 DLL 和 module，内部拥有回调绑定及有界队列；不负责模型导入、装载准入、任务调度和设备。这是当前实现边界，不是缺少几行 UI 接线。
3. `PrintApp/src/main.cpp:582` 在部分局部对象析构前调用 SpdlogMgr Shutdown。未来 loader 必须在这之前显式停止和释放日志资源，不能依赖任意成员的隐式析构顺序。
4. 日志合同要求 clear 成功后才可释放 context/module 或卸载 DLL。timeout 只限制回调注销等待，不能保证任意磁盘 sink 有界排空；这些规则不因接入产品而放宽。
5. 原切片分支开始 P0FIX 声明和真实探针修订。9 月 14 日 E01A 只验收版本/CRT、日志和共存，不是完整 module.json/schema/自检/有效模型业务准入。

### Current State

**阶段：本地工程功能完成、真实日志适配完成，正式交付/产品业务接入待前置条件。**

LD-00..03、A00..A03（含 A01D）、B01..02、C01..02、E01A 保持 COMPLETE。E01 总项 PARTIAL_COMPLETE。此次 E01B-P 准备完成，E01B 业务接线仍不具备开工输入。

本轮重新读取 E01A `summary.json` 和 CTest 记录：1/1 PASS、实例分别转交 7/1 条；切片兼容回归记录为 2/2 PASS，独立包 smoke 为 PASS。包内 `slicer_module.dll` SHA256 仍为 `1E7435461811725D591AB410A2C8E2339BBDF8E0B0D733751D512A4BA1A89CA3`，版本 0.2.450-dev、SPI 1、日志 API 1。本轮未重新运行构建或测试。

### Target State

正式 P23 loader 提供经过业务准入的 DLL/module/作业拥有者后，LOGDUMP 只将现有 adapter 接到它的创建、停机和失败清理节点。目标是无需复制新日志实现、无需在切片 DLL 中安装全局异常处理器。

先完成新基线交付准备；实际整合时重新固定源/目标 SHA、处理下表交叠并重建产物，再用新 DLL 刷新 E01A 证据。完整 PrintApp 挂接通过后才能完成 E01B；不能把单独组件验收改名为产品完成。

### Historical State

最早 E01 的 WAITING_AUTH 已被此前继续执行授权覆盖；现在阻碍业务挂接的是实际产品入口和基线尚未稳定，不是重新请求同一日志开发授权。A00 初版“只改切片仓库”的范围已经由 E01A 扩展；本轮没有因此扩大为打印 P23 全产品开发。

### Pending Confirmation

准备工作无需新增确认。正式实施前需要真实产品输入：P23 loader 实现/拥有者、对应产品分支、明确的关闭入口，以及最终 SDK 产物和目标 SHA。这些是可核验的交付条件，不能由旧报告或假造一个 loader 补齐。

### Risk Points

| 已确认的交叠文件（固定 `554847f3`） | 集成时必须同时保留的内容 |
|---|---|
| `AGENTS.md` | 新主线/任务状态与 LOGDUMP 入口；不回退其他专项进度 |
| `CMakeLists.txt` | P0FIX JSON 深度测试与诊断 targets/测试；不得通过整文件选择丢失一方 |
| `apps/slicer_ui_host_sim/CMakeLists.txt` | 新 UI/HostUX targets、`slice_soft_test.exe` 名称和兼容旧名；日志源、链接依赖、符号和部署接线 |
| `apps/slicer_ui_host_sim/Main.cpp` | 高 DPI、自测/设置持久化规则与诊断启停生命周期 |
| `apps/slicer_ui_host_sim/HostRipJobController.cpp` | 分阶段进度展示与 stdout/stderr、退出、失败日志；保留原业务 capture |
| `scripts/PrepareSliceSoftRuntime.ps1` | 新程序名、兼容副本、指南资源与 reporter/spdlog/fmt/许可证/符号归档；按真实二进制核对 PDB，兼容副本不重复计作新目标 |

这里只确认文件交叠，没有执行合并，不能宣称“仅 6 个冲突”或“可自动合并”。在该快照下 CLI、ModuleClient、Worker/SPI、vcpkg 和版本文件无已提交变化；随后 P0FIX 合同/探针及 Worker 后续计划仍可能交叠，实际整合时必须重查，不能复用这张静态表替代 diff。

原树 `output/p0fix/p0-01-ctest.log` 记录 247 项、7 项失败，其中旧 S1 路径和 ViewData 期望问题仍在。本轮读取现有日志，不改判定、不标全量 PASS，也不把新基线失败自动全部归为既有。

### Files To Change

本轮仅新增本文、更新 LOGDUMP 任务卡、AGENTS 和 Codex 入口。代码、依赖、运行包、打印产品文档及两个原工作树不修改。

未来接线涉及的现有候选文件是打印 `AppContext.h`、`AppBootstrap.h`、`main.cpp`、P23 实际 loader 文件及其测试；只有产品拥有者确定后才落定具体文件所有权。复用 `PrintAppSlicerLogAdapter`，不把通道化 SliceService 改成几何服务。

### Verification Plan

本轮：原/专项工作树状态、正式文档与代码前置交叉核对、已有证据和 SHA 检查、Markdown 链接/围栏、diff --check。采用两个代理只读并行复核打印入口与切片基线，根执行者维护文档。

实际文档检查：9 份 LOGDUMP 文档、67 个仓内相对链接均通过，围栏配对且无行尾空白；两个专项工作树的 `git diff --check` 均退出 0。本轮未运行构建或运行时测试。

正式整合后的验证顺序：

1. 固定基线及模块清单，用真实 DLL 探针核对 SPI、module_info、manifest、版本/CRT、能力和独立日志扩展；不硬编码旧 15 项能力/旧运行库数量。
2. 构建诊断、DLL/Worker、CLI/Host 和新基线 HostUX 相关目标，确认退出码为 0 后执行对应 CTest；保留 P0FIX 输入加固回归。
3. 重验回调注销/双实例/Worker 来源、业务错误与取消、RIP 进度和日志并存，随后做 UI 自检及有效模型输出对照。
4. 按实际新程序名生成独立包，校验匹配 PE/PDB、许可和依赖；中文路径/受限 PATH smoke。使用新 DLL SHA 重新运行 E01A，旧包的 PASS 不能迁移。
5. 正式 PrintApp loader 流程单独验收下表场景；不启动设备连接作为日志验证的隐式副作用。干净机器、其他 ACP 和物理打印继续单独记账。

## 业务挂接规则

| 节点 | 准备要求 |
|---|---|
| 拥有者 | P23 产品应用级模块拥有者持有库/模块/作业和 adapter；不归某台打印设备，也不由回调反向持有拥有者形成环 |
| 装载 | 使用产品模块根（MOD-22 目标为 modules/slicer）；DLL/Worker/清单/依赖成套，路径转 Windows 原生分隔符；正式版本及完整性规则属于 loader |
| 建立日志 | 宿主 logger 已初始化、业务模块已准入并成功 create 后，调用 adapter Create；保留打印 Off=6 与切片 Off=-1 映射 |
| 日志降级 | 缺失/未知日志扩展仅关闭模块日志，不掩盖业务错误，也不能使不合格模块通过装载准入 |
| 执行线程 | 回调只复制/入队；Heavy 调用和停机等待由非 GUI 的受控任务承担。GUI 只接收状态/诊断投影 |
| 正常退出 | 停止新任务并等待/取消在途作业，按业务合同释放作业；clear 成功后销毁 adapter、模块及库，最后宿主 Flush/Shutdown |
| 失败清理 | 初始化失败仅回收已取得的资源；clear 非 OK 时保留 context/模块/库并协调重试，不先卸载；不以无限阻塞 GUI 或强杀日志线程处理 |
| 日志与 DUMP | app/module 文件及全局 logger/filter 归 PrintApp；不启动第二个宿主异常处理器。Worker 是另一进程，诊断策略由其部署合同决定；E01A 中显式关闭 Worker 私有会话只是测试设置，不能当作产品默认决定 |
| 同名依赖 | 正式宿主已有 TIFF/日志依赖时核对已加载模块和包内版本；单独进程验收不证明与完整 PrintApp 的同名依赖共存。不得通过扩展 PATH 或覆盖系统库绕过问题 |

## 任务与准入

| 子项 | 处理内容 | 状态 / 进入条件 |
|---|---|---|
| LD-E01B-P | 当前阶段、P23 入口、旧证据、新基线交叠、拥有者/关闭/验证准备 | COMPLETE，2026-09-15；无业务代码改动 |
| LD-E01B-01 | 在实际 loader 上接入 Create/Close 与失败回收 | ENTRY_NOT_IMPLEMENTED；P23 loader、产品拥有者和目标版本就绪后可开工 |
| LD-E01B-02 | 真实产品流程与最终部署验证 | WAITING_DEPENDENCY；依赖 -01 和明确的最终包；原 E01A 结果不替代 |

以下为 -02 的待执行验收清单，不是已运行测试：

- 正常创建/作业/关闭，多个实例的日志身份与源 PID/TID 正确，注销后原宿主 logger 仍可使用。
- 模块日志缺失/未知版本降级，主模块缺失/不兼容则业务失败；两者错误严格区分。
- 取消、初始化部分失败、clear 超时和错误重试；回调 context 保活，退出等待不占用 GUI。
- 有效模型业务和故意失败请求分别保持原结果；中文路径/诊断文本不改变业务 jobId 规则。
- PrintApp 现有日志、Qt 消息处理器和崩溃处理器共存；全局 Shutdown 发生在模块日志生命周期结束后。
- 最终版本/清单/依赖、产物 SHA、符号归档和运行日志可相互追溯。

## 准备结论

可继续且已完成的是 LD-E01B-P。正式 E01B 仍缺 P23 产品入口，新基线整合还需重新固定完成中的 P0FIX 目标。保持 E01A COMPLETE 与 E01B 未实现的区分，不重复构建旧组件代替产品准入，不自动提交或合入。
