# LOGDUMP 日志与崩溃转储可复用模块专项任务清单

> 建立日期：2026-09-14；更新日期：2026-09-15；修订：v1.8；专项状态：SLICER DELIVERY COMPLETE / MERGED TO LOCAL PRODUCT / PRINT INTEGRATION DEFERRED。
> 本表是任务状态唯一真源；准备完成不代表运行时能力已实现。
> 方案：[设计及实施准备](../../slice/DOC/DOC_PREP_LOGDUMP_日志与崩溃转储模块设计及实施准备.md)。
> 新增设计：[DLL 日志回调与双层复用接口草案](../../slice/DOC/DOC_DESIGN_LOGDUMP_DLL日志回调与双层复用接口草案.md)。

## 0. 当前处理情况（2026-09-15）

- 当前G01..03全部完成：product已从d28b6451快进到7354a616，切片侧交付收口。清洁源码Release全目标构建退出0；相同67项62通过/5条既有失败；完整包启动落盘与新SDK独立构建、真实打印后端消费2/2通过。见[G产品合入收口](../../slice/REPORT/REPORT_LOGDUMP_G_产品合入与复用交付收口.md)。未推送，打印E01B前置缺失不变。下列F及更早轮次“未合入/未提交”均为历史。

- F01..03 已完成本地交付修复、任务分组提交和产品合入判断，见 [F 收口报告](../../slice/REPORT/REPORT_LOGDUMP_F_提交收口与产品合入判断.md)。Release 全目标重建退出0，最终67项62通过/5失败（150.66s），全部剩余原因逐项记录；新增诊断验收通过。产品基线已合入专项分支，product 本身未移动、未推送。限定切片侧合入为 CONDITIONAL GO；若要求 Stage14 全绿则仍 NO-GO。
- 当前追加执行 F 收口：用户已授权按任务提交及判断产品合入。[F 准备](../../slice/DOC/DOC_PREP_LOGDUMP_F_本地收口提交与产品合入准备.md)已完成；补齐旧模块包动态 helper 部署缺口，目标为 product/packaged-slicer 的 d28b6451，不隐式带入其他 P0FIX 提交。下列“未提交/未合入”是此前轮次记录，最终情况以 F 任务行及新报告为准。
- 用户再次授权按计划继续。本轮先完成 [E02 实施准备](../../slice/DOC/DOC_PREP_LOGDUMP_E02_可迁移源码包与成功作业验收准备.md)，再完成 E02-01/02 两项并行：23 文件源码 SDK、独立构建和真实成功切片。最终 CTest 2/2 PASS，生成 3 个 TIFF，Package Reader 通过，18 条成功作业日志转交且注销后宿主继续写入。详见 [E02 交付报告](../../slice/REPORT/REPORT_LOGDUMP_E02_源码SDK交付与成功切片验收.md)。E01B 业务入口状态保持不变。
- 前一轮先完成 LD-E01B-P：[业务挂接与新基线交付准备](../../slice/DOC/DOC_PREP_LOGDUMP_E01B_业务挂接与新基线交付准备.md)。两个代理只读并行核对打印入口和切片基线，已明确拥有者、关闭顺序、交叠文件与待执行验收；正式 E01B 仍缺实际 P23 loader，未开发产品业务入口。
- 原切片工作树已进入 codex/feature-p0fix-contract-robustness 且继续更新；固定 554847f3 快照与本专项有 6 个共同修改的 tracked 文件，P0FIX 合同修订仍在继续。实际整合必须重新固定目标 SHA，不能照搬旧测试或覆盖新 UI/进度/部署改动。这里只做比较，未执行合并。
- 2026-09-14 已完成 [E01 实施准备](../../slice/DOC/DOC_PREP_LOGDUMP_E01_PrintApp真实日志适配准备.md) 和 E01A 开发/验收，建立打印隔离分支 codex/slicer-logging-adapter。真实 SpdlogMgr + 切片 DLL 的 RelWithDebInfo 验收 1/1 PASS，父工程默认 OFF 配置及 ON 适配目标构建 PASS；切片宏兼容改动的 Release 回归 2/2 PASS。详见 [E01A 验收报告](../../slice/REPORT/REPORT_LOGDUMP_E01A_PrintApp真实日志适配验收.md)。这些是当日历史数据；本轮 E02 使用新导出源码重建两个验收 EXE，未重新编译切片 DLL。
- 打印软件尚无几何切片 DLL loader，E01 拆为已完成的真实日志后端适配/验收 E01A 与未来业务入口挂接 E01B；总项 PARTIAL_COMPLETE，不将组件验收当作正式产品业务挂接。

- 准备 LD-00..03、准入 LD-A00 已完成；功能已在独立 feature 工作树实现，未提交或合入原分支。
- 两层日志、DLL/Worker 诊断通道、宿主/CLI/Worker 转储接线、历史会话清理与符号归档已完成。Debug 和 Release 所需目标构建均退出 0。
- 最终 Debug 专项回归 16/16 PASS；Release 组合回归 17/18 PASS，唯一失败为已有 S1 测试目录期望过时。实际产出目录的 Reader 单独验证 PASS。另一次补充回归 8/9 PASS，唯一失败为已有 ViewData 安全测试期望不一致。
- 独立 Release 包已部署至 `runtime/logdump/Release`，RIP 完整性、版本、Qt 依赖和诊断符号核对通过；没有覆盖原工作树运行包。
- Release fixture 20 层、真实 segment_101 184 层各完成日志 off/info 对照 8 次（含各 1 次预热），全部 TIFF 字节一致、Reader PASS。真实模型中位耗时从 4.218 s 增至 4.593 s（+8.88%），CLI 峰值工作集增加 774,144 B；这是首轮本地观测，非全场景性能承诺。
- 中文路径、仅系统 PATH 的独立运行包验证 PASS：依赖/许可 SHA256、PDB 不随包发布、Worker 合同查询、Host 启动、reporter 就绪、模块日志绑定、Qt 消息和正常关闭均通过；自检不保存用户工作区设置。
- 真实 PrintAppLogging 组件已在 E01A 验证；正式 PrintApp GUI、其他 Windows ACP、干净机器与物理打印尚未验证，不把这些项目记为 PASS。

详细证据：[本地实现与验证状态](../../slice/REPORT/REPORT_LOGDUMP_本地实现与验证状态.md)。完成状态以第 2 节表格为准。

## 1. 授权与工作树

2026-09-15 用户先要求阶段判断与准备，完成 E01B-P；随后再次授权按计划继续，准备后执行 E02 可独立交付部分。两个代理分工导出和成功验收，根执行者控制构建并修复审查问题。已有授权无需重复请求；E01B 暂不接线的原因是正式产品入口尚未实现，并非重新置为 WAITING_AUTH。

2026-09-14 追加授权：用户同意按计划继续任务、每项先准备、由执行者决定并行。此前准备期“未授权开发/依赖”的描述仅为历史。A00..C02 按 [A00 开发准入决策](../../slice/DOC/DOC_DECISION_LOGDUMP_A00_开发准入与日志扩展定案.md) 完成本地实现/依赖接入/测试；本轮继续授权推进 E01A，允许在打印隔离工作树实现可选日志适配及无设备验收。正式业务 loader 建设、硬件操作、提交和合入不在本轮范围内。

准备期授权参考 `E:/__Code/__Work/ry_print_demo/PrintSolution` 的 spdlog/DUMP 实现，先创建专项并准备，从当前分支拉取 feature 分支。当前授权按上一段执行；打印修改仅在新工作树 `E:/__Code/__Work/ry_print_demo-slicer-logdump`，未修改原打印工作树，未提交或推送。

用户进一步明确 DLL 内部日志与软件内部日志两部分，并要求参考打印 SDK 回调、优化复用方案。已按此修订并实现；原“首版不增加 DLL 日志回调”的建议撤销。当前新增独立可选 C 日志扩展，原 SPI v1 的 11 个签名保持。

| 项目 | 实际值 |
|---|---|
| 创建专项时原分支 | `codex/host-visibility-navigation`；2026-09-15 原树已转到 `codex/feature-p0fix-contract-robustness` |
| 起点 | 原分支已提交 HEAD `e2546797` |
| 专项分支 | `codex/feature-logging-dump` |
| 独立工作树 | `E:/__Code/__Work/slice_test_demo/slice_soft_demo-logdump` |
| 未提交工作隔离 | 原工作树 UI/RIP/文档/模型/构建配置等修改留在原位，未纳入本分支 |
| 累计产物 | 准入决策、双层日志/Worker 通道、Windows helper、软件接线、测试、打包/符号工具、PrintApp 日志适配与使用说明 |
| 2026-09-15 产物 | E01B-P 准备；E02 源码 SDK/导出验证、独立成功作业验收与结果报告；未新增正式产品业务入口 |

不重开 MEMFLOW、几何采样、RIP 合同调整或 VSCode 编译优化。原工作树的 RIP/UI 改动在后续集成前重审，不通过本专项隐式合入。

## 2. 任务拆分

状态约定：COMPLETE 为该行限定的实现/本地验证或准备交付已完成且有证据，不代表所有环境或原 G0-G5 全矩阵通过；IN_PROGRESS 为已准备并进入实施；PARTIAL_COMPLETE 为仅部分子项完成；ENTRY_NOT_IMPLEMENTED 为依赖的产品入口尚不存在；WAITING_DEPENDENCY 为等待已标明前置；PREPARED 为可供开发准入评审；PROPOSED 为后续设计任务；WAITING_AUTH 为后续授权项。原 S1/ViewData 失败及未验证面仍明确保留，不通过更名状态规避门禁。

| ID | 任务 / 交付物 | 前置 | 状态 | 完成日期 | 验证 / 完成标准 |
|---|---|---|---|---|---|
| LD-00 | 原分支检查和隔离 feature 工作树 | 当前用户授权 | COMPLETE | 2026-09-14 | worktree 创建退出 0；新分支起点 e2546797，原工作树未切换 |
| LD-01 | 打印 spdlog/DUMP 与切片当前能力调查 | LD-00 | COMPLETE | 2026-09-14 | 读取 SpdlogMgr、main.cpp、构建配置及切片 SPI/Worker/RIP 边界；方案第 1-3 节记录事实和缺口 |
| LD-02 | 边界、依赖比较、验收与任务准备 | LD-01 | COMPLETE | 2026-09-14 | 文档路径/链接、状态与 diff 检查；没有代码、依赖或合同变更 |
| LD-03 | SDK 日志回调调查、双层职责与兼容文档修订 | 用户本轮补充要求 | COMPLETE | 2026-09-14 | 核对 A3DSDK 现用头文件/IMPORTED 构建及 VendorEngineAdapter；新增回调草案，修订任务/入口；文档检查记录见第 5 节 |
| LD-A00 | 可选 C 日志扩展定案与 ABI 增量准入 | LD-03 + 开发阶段授权 | COMPLETE | 2026-09-14 | A00 决策与 C 头固定 3 个导出/返回码/16KiB 事件及队列预算、注销规则与兼容矩阵；运行验证随 A01D |
| LD-A01 | 通用事件/宿主 adapter、私有 spdlog 后端与单测 | LD-A00 + 依赖接入确认 | COMPLETE | 2026-09-14 | Debug/Release UTF-8、轮转、有界队列、失败隔离、宿主注入及会话清理测试 PASS；使用现有 baseline spdlog 1.17.0/MIT；磁盘永久阻塞不承诺有界关闭 |
| LD-A01D | DLL 实例日志出口与真实内部调用点 | LD-A00 | COMPLETE | 2026-09-14 | Debug dispatcher/真实 DLL/Worker 回调通过；C 头、双宿主、精确14导出、SPI/Worker合同共13项通过；注销超时/并发/重入/双实例和丢弃计数已覆盖 |
| LD-A02 | 软件自身日志、DLL 回调绑定、Qt 桥接与 job 关联 | LD-A01、LD-A01D | COMPLETE | 2026-09-14 | 双宿主/RAII 注销/域隔离 Debug、Release PASS；Debug UI 实际日志 PASS；自检参数已改为 --diagnostics-self-test 复用禁止持久化规则 |
| LD-A03 | Worker 诊断 IPC、RIP 故障持久化、诊断入口 | LD-A02 + 重审最新 RIP/UI 已提交改动 | COMPLETE | 2026-09-14 | 真实 Worker 来源/作业身份、独立环境、IPC 降级、超长 UTF-8 RIP stdout/stderr 分块与原业务 capture 保持的测试在 Debug/Release PASS；原工作树未提交 RIP 变化未混入 |
| LD-B01 | Windows crash helper 生命周期/IPC 原型 | LD-A01 + 确认 dump 默认策略 | COMPLETE | 2026-09-14 | Debug真实访问异常、中文路径并发、异常/线程/模块流、PDB GUID/age和ControlledCrashSite符号、启动/捕获超时、原过滤器保护/显式接管恢复通过 |
| LD-B02 | Host/Worker/CLI 转储接入和异常覆盖矩阵 | LD-B01、LD-A02 | COMPLETE | 2026-09-14 | Debug/Release 专用子进程访问异常及 PDB 定位 PASS；CLI Release reporter 就绪、完整包 UI/Worker 验证 PASS；仅 AV 实测支持，其他异常不承诺支持 |
| LD-C01 | 部署清单、PDB 归档、双宿主复用 SDK 样例 | LD-A03、LD-B02 | COMPLETE | 2026-09-14 | 独立 Release 包部署成功；Debug/Release 模拟双宿主及符号正负例 PASS；中文路径与受限 PATH 包验证 PASS，6 组二进制/匹配 PDB 归档；真实 PrintApp 另见 E01 |
| LD-C02 | 语义回归、Release A/B、用户手册和专项收口 | LD-C01 | COMPLETE | 2026-09-14 | Debug/Release fixture 与真实资产 TIFF 字节一致，Reader PASS；任务/报告/手册及源码行数、文档链接、diff 检查完成；S1/ViewData 两项既有失败、额外 ACP/干净机器等未验证面保留 |
| LD-E01 | 真实 PrintApp 日志适配与业务挂接总项 | LD-C02 + 本轮继续授权 | PARTIAL_COMPLETE | - | 2026-09-14 E01A 完成；E01B 缺少产品几何切片 loader，不将后端组件验收等同业务集成 |
| LD-E01A | 真实 PrintAppLogging 适配器及 DLL/版本/共存验收 | LD-C02、E01 准备完成 | COMPLETE | 2026-09-14 | 真实后端+DLL 验收 1/1 PASS；父工程 OFF 配置/ON 适配目标构建 PASS；中文日志目录、双实例、UTF-8 Worker 负例、等级/降级/注销和宿主所有权通过；切片兼容回归 2/2 PASS，无设备调用 |
| LD-E01B | 正式打印业务 loader 的日志生命周期挂接 | 几何切片业务 loader 及集成产品方案 | ENTRY_NOT_IMPLEMENTED | - | 2026-09-15 代码和 MOD-22/P23 复核仍无入口；E01B-P 已完成，不代表业务接线；不能以图像通道化 SliceService 冒充几何切片 |
| LD-E01B-P | 正式挂接前置复核、新基线交付和验收准备 | 用户本轮要求、E01A 完成 | COMPLETE | 2026-09-15 | 两代理只读复核；真实打印入口仍未实现，固定快照6文件交叠；已有1/1、2/2与包smoke记录及DLL SHA复核；明确所有权/关闭/版本/重验清单；9份文档67个链接、围栏/空白及两专项工作树diff检查通过，未重新构建 |
| LD-E01B-01 | 实际产品 loader 的创建、停机和失败回收接线 | P23 loader、产品拥有者与目标版本就绪 | ENTRY_NOT_IMPLEMENTED | - | 复用既有 adapter；clear 非OK保活；在打印日志Shutdown之前完成退出，不建设整个P23替代入口 |
| LD-E01B-02 | 正式产品流程及最终包日志共存验收 | E01B-01、固定最终SDK/目标分支 | WAITING_DEPENDENCY | - | 创建/有效业务/取消/失败/注销，日志与DUMP所有权、新版本SHA及部署验收；不复用旧E01A结果冒充完成 |
| LD-E02-01 | 可迁移日志/可选转储源码 SDK | E01A、E02 准备完成 | COMPLETE | 2026-09-15 | 23 文件清单/SHA/尺寸、拒绝覆盖/交叠/reparse 均通过；中文目录 SDK 日志/转储客户端/helper 构建退出0，预定义宿主宏兼容；未新做异常注入，不替代二进制模块包 |
| LD-E02-02 | 真实成功作业及源码 SDK 消费验收 | E01A、E02 准备、E02-01 | COMPLETE | 2026-09-15 | SDK 消费重建后原负例+新成功作业 CTest 2/2 PASS、0.93s；3 TIFF、Reader valid、18条事件及注销后共存通过；重用证据目录拒绝且SHA保持，修复动态进度缓冲测试假设 |
| LD-F01 | 模块二进制包补齐 crash helper 和日志扩展头 | F 准备 | COMPLETE | 2026-09-15 | 核心Release构建退出0，独立包 helper/SHA/依赖扫描/PDB隔离通过；同步旧脚本Git派生PATCH校验，与现有运行包规则一致 |
| LD-F02 | 专项按任务拆分提交 | F01、专项复核 | COMPLETE | 2026-09-15 | 运行时19065268、部署48e175eb、SDK cf019c48、文档34392284、声明隔离557b3eed、交付校验859fa483；打印适配c63b2510；不含运行数据/模型，不推送 |
| LD-F03 | 产品基线整合及合入判断 | F02、固定产品基线 | COMPLETE | 2026-09-15 | eafbfec7在专项分支整合product；Release全目标构建退出0，67项62通过/5失败，剩余为4项基线红灯及S1依赖门；限定合入CONDITIONAL GO，完整Stage14验收仍NO-GO；product未移动 |
| LD-G01 | 产品合入准备与复用规范复核 | 用户实际合入授权、F03 | COMPLETE | 2026-09-15 | 确认日志树干净、product 为 d28b6451、8提交可快进；修正helper旧说明，明确C接口/sink/注销/转储所有权；不引入新接口或依赖 |
| LD-G02 | 日志专项合入本地产品分支 | G01 | COMPLETE | 2026-09-15 | 隔离树切换product，ff-only从d28b6451前进到7354a616；原P0FIX修改保留、不推送、不删除分支 |
| LD-G03 | 产品构建、运行包与可复用SDK交付收口 | G02 | COMPLETE | 2026-09-15 | 清洁源码Release全目标重建退出0；67项62通过/5失败、162.60s；0.2.471-dev完整包部署及实际启动落盘PASS；23文件SDK独立三目标构建和打印真实消费2/2 PASS、0.60s；保留既有红灯及打印外部待接线 |

执行顺序：LD-A00 → LD-A01 与 LD-A01D → LD-A02 → LD-A03；日志接口稳定后推进 LD-B01/B02，最终 C01/C02。A00..C02 阶段使用两个代理并行完成 DLL/IPC 与 helper/打包，根执行者负责宿主、公共 CMake 和任务表；单一执行者控制构建目录，代理交叉审查。

E01A 同样先完成准备；两个代理并行处理适配器与真实后端验收，根执行者控制两个仓库的构建/文档，交叉审查补强实例归属和完整终态断言。

## 3. 文件责任与冻结面

| 任务组 | 拟修改范围 | 不允许顺带修改 |
|---|---|---|
| A00/A01D | contracts/slicer_logging.h、src/slicer_module/logging、slicer_module.def、模块内部调用点与 ABI 测试 | 不改原 11 个 pm_* 签名；扩展精确列白名单，不笼统放开未知导出 |
| A01 | src/diagnostics、tests/diagnostics、cmake/SliceSoftDiagnostics.cmake、根 CMakeLists.txt、vcpkg.json | 不升级整个 baseline，不重构切片算法，不复制第三方二进制 |
| A02/A03 | apps/slicer_ui_host_sim、apps/slicer_host_sim、apps/slicer_worker、apps/slicer_cli、src/slicer_module 的可选加载/诊断通道边界 | 不覆盖原工作树尚未提交的 UI/RIP 演进，不改 Worker 文件 schema，不在子进程使用宿主 callback/context 地址 |
| B01/B02 | src/diagnostics/windows、apps/slicer_crash_reporter、故障测试子进程 | 不在 DLL/DllMain 安装过滤器，不替外部 rip_cli 承诺异常捕获，不改系统 WER |
| C01/C02 | 部署/符号配置、测试、复用样例、使用手册、专项报告 | 不自动提交私有 PDB、DUMP、用户日志和模型内容 |
| E01A | 打印隔离树 integrations/slicer_logging、主 CMake 可选入口；切片复用源宏保护和文档 | 不改 PrintApp 默认日志实现、SDK、图像 SliceService、设备业务或正式产品版本准入 |
| E01B-P | 本准备文档、专项任务与入口状态 | 不改两个原工作树、不接管P0FIX合同修订、不实现P23全业务、不提交或合入 |
| E02 | 切片 sdk/diagnostics 与导出脚本；打印独立成功验收/CMake/fixtures；专项文档 | 不引入依赖、不修改算法/业务合同、不将工程成功作业当正式产品集成 |

保持原 SPI v1 的 11 个导出签名、S1/S2、生产 package 协议、通道顺序、uint8、black_is_print 和业务成功/失败判定。日志扩展新增独立版本可选 C 导出，导出表和三个精确白名单已更新；不是“完全不改 ABI”。

## 4. 进入开发前的裁定项

1. 用户已明确的目标：DLL 内部事件与软件内部业务日志均纳入首版；借鉴打印 SDK 回调，宿主管理文件与等级，不能再缩为宿主外部错误转写。
2. 推荐技术方案：版本化可选 C 回调 + 通用 host adapter + 独立软件的私有 spdlog 后端；在 LD-A00 固化导出/注销合同，在 LD-A01 确认现有 baseline 下依赖接入。
3. 推荐日志默认 info，本地持久化；app.log/slicer_module.log 分域且共享总预算。打印宿主使用自己的 sink/目录，不默认双写两套日志。
4. 推荐 Windows 预启动 helper，完整内存 dump 默认关闭；reporter 仅由 EXE 显式启用，嵌入打印宿主时让出所有权。
5. 跨打印软件复用同时交付 C 回调头和宿主 adapter；不共享 C++/Qt/spdlog 对象 ABI。A00..C02 首轮只做切片本地实现，后续 E01A 按独立准备及本轮授权在打印隔离工程完成可选适配；正式业务挂接仍为 E01B。

以上已由本轮执行授权和 A00 决策固定。各任务按实际验证更新；未知环境和外部验收不冒充本地 PASS。

## 5. 验证记录

### 2026-09-15 G 产品合入

- 用户授权实际合入后，准备/复用说明提交7354a616；隔离工作树切换product并快进，原P0FIX树不切换、不纳入用户修改。
- 清洁源码Release全目标重建退出0，版本0.2.471-dev；67项62通过/5失败（与F失败集一致），162.60s。记录完整失败，不宣称Stage14全绿。
- 新完整运行包位于产品工作树runtime/slicesoft/Release，中文目录/受限PATH启动落盘及helper验证PASS。SDK23文件及拒绝用例PASS，独立日志/可选转储三目标构建通过。
- 打印隔离树从新SDK重新构建消费端，并使用新产品DLL，成功作业和原负例2/2 PASS、0.60s；不代表正式打印GUI/P23接线或物理打印完成。
- 最终6份文档22个仓内链接及围栏检查通过，git diff --check通过；收口提交仅更新状态文档，不修改已验证源码身份。

### 2026-09-15 F 收口

- 补齐旧模块包 helper、两个 C 头和依赖扫描；修复旧验包脚本的 Git 派生版本比较，并实际完成纯 C 宿主 3 层切片/Reader 验证。
- 首次全目标构建暴露声明头依赖，修复后 Release 全目标重建退出0。最终67项62通过/5失败，150.66s；失败名称及与基线的关系见 F 报告，不记为全绿。
- `runtime/logdump-f/Release` 独立部署与中文路径/受限PATH验证通过；新DLL与真实打印后端正负验收均退出0。源码门禁0 ERROR/74 WARNING；源码SDK23文件导出/拒绝测试重验通过。
- 用户原树在执行中出现新的 P0FIX 代码改动，全部保留原位；两个专项树按任务提交，不移动product、不推送。

### 2026-09-15 E02 执行

- 已复核两个专项工作树、现有源码/构建与 E01A 证据；两个原树的 P0FIX、模型和资料改动保留原位。
- E02 实施准备完成后两代理并行开发并交叉审查；SDK 三个目标和打印两个验收 EXE 构建退出0，最终 CTest 2/2 PASS、0.93s。逐步实际结果和产物身份见 E02 报告。
- SDK 导出拒绝测试通过；实际消费四个通用诊断源均来自导出包；新成功用例保存3 TIFF与18条事件。源码门禁0 ERROR/74 WARNING，新成功CPP 378行；14份相关文档80个仓内链接、围栏/空白及两个专项工作树diff检查通过。
- 首次264字符报告路径失败已记录；本轮缩短测试根，不修生产长路径。旧 PackageSlicerModule helper 部署、正式产品loader、新P0FIX基线、GUI/真机等边界保留。

### 2026-09-15 E01B-P 准备验证

- 代码和正式文档交叉复核完成；现有 E01A 测试记录及 DLL SHA 复核，不将旧测试计作本轮重跑。
- 9 份 LOGDUMP 文档的 67 个仓内相对链接均存在，代码围栏配对、无行尾空白；两个专项工作树的 `git diff --check` 均退出 0。
- 本轮仅修改专项文档和入口，未运行 configure/build/CTest、GUI 或设备操作，未提交、合入或修改两个原工作树。

### 初始准备期历史记录

- 已读取原工作树状态与实际分支，创建独立 worktree 成功；未移动原分支或处理其未提交文件。
- 已阅读打印项目 spdlog 封装、异常过滤器和构建入口，以及切片 SPI、Worker、RIP 控制与版本入口。
- 初版文档验证：两份新增文档的 5 个仓内相对链接均存在；git diff --check 退出 0。初版仅 4 个 Markdown 文件，本轮增加接口草案后变为 5 个，初版检查不冒充修订后的验证。
- v0.2 文档验证：三份文档 9 个仓内相对链接均存在、无行尾空白、代码围栏配对；5 个 SDK 参考文件存在。双层职责、接口范围、任务依赖和入口交叉核对完成，git diff --check 退出 0（仅 LF/CRLF 提示）；当前仅本专项 5 个 Markdown 文件变更。
- 初始准备期未运行 configure/build/CTest、日志压测、故障注入、干净机器或打印软件集成测试；后续实际执行见第 0 节及对应验收报告。
- 初始准备期未修改 C++/CMake/vcpkg 或打印项目，未提交、未推送；不代表后续开发状态。

## 6. 修订记录

| 日期 | 修订 |
|---|---|
| 2026-09-15 v1.8 | G01..03完成：实际快进合入本地product，清洁Release全目标重建及独立运行包落盘验证，新SDK构建与打印消费2/2通过；切片侧收口，打印E01B和既有红灯显式保留，不推送 |
| 2026-09-15 v1.7 | 用户明确授权实际合入product；完成G01准备，新增G02/G03并开始执行。修正SDK/手册的helper旧描述，复用保持可选C回调、宿主sink和EXE转储所有权 |
| 2026-09-15 v1.6 | F01..03完成：修复helper交付、版本校验、声明头依赖与冻结样本LF；分任务提交、整合product基线并完成全目标Release构建与67项回归。限定合入CONDITIONAL GO但完整Stage14仍有5条失败；E01B缺P23入口不变，product未移动 |
| 2026-09-15 v1.5 | 用户续办并授权任务提交/合入判断；新增 F01..03，完成 F 准备并开始修复旧模块包遗漏。264 字符路径属于既有通用 I/O 限制，单列待办，不修改系统策略；正式 E01B 仍等待 P23 loader |
| 2026-09-14 | 创建 LOGDUMP 专项，完成独立分支与参考调查；拆分日志、应用接入、转储、部署和打印复用任务，准备阶段结束、开发未启动 |
| 2026-09-14 v0.2 | 完成 LD-03；按用户要求补足 DLL 内部回调与软件日志两部分，新增 LD-A00/A01D，调整 A01/A02/A03 前置及 L0..L5 验证；首版无回调限制撤销，未开发/构建/提交 |
| 2026-09-14 v0.3/v0.4 | 用户授权继续执行；逐项准备后实现，独立worktree并行协作；Debug13项与UI/CLI初验通过；审查修复日志注销错误和RIP大块输出截断，Release/打包验证进行中 |
| 2026-09-14 v0.5 | 按用户要求优先刷新可见任务进度，新增当前处理情况和独立验证报告；A01/A02/A03 完成，Debug 16/16、Release 17/18、独立部署与 fixture A/B 已记录；明确两项既有失败及剩余验证 |
| 2026-09-14 v1.0 | B02/C01/C02 本地收口：Release 真实模型 184 层、fixture 20 层 off/info 逐层一致；中文路径仅系统 PATH 包验证 PASS，记录实际耗时和内存开销；原 S1/ViewData 失败及外部矩阵保留，未合入/提交 |
| 2026-09-14 v1.1 | 用户授权按计划继续；E01 准备调查发现真正 SlicerService/模块 loader 尚不存在，拆 E01A 日志后端适配验收和 E01B 产品业务挂接；打印隔离分支建立，A 开发中、B 前置缺失 |
| 2026-09-14 v1.2 | E01A 完成：真实 PrintAppLogging/切片 DLL 中文日志和双实例 Worker 来源验收 1/1 PASS，父工程 OFF/ON 构建边界通过，切片宏兼容回归 2/2 PASS；修复验收导出、CRT 映射与 Windows 路径问题。源码门禁0错误/74警告、8份文档62链接及两工作树diff检查通过。E01 总项部分完成，E01B 保持入口缺失；未修改原工作树、未提交或合入 |
| 2026-09-15 v1.3 | 完成E01B-P准备：核对PrintApp MOD-22/P23仍无几何loader、两个专项工作树和旧证据；固定554847f3记录6个交叠文件，后续P0FIX合同修订单列动态风险；拆E01B-01/02并固化生命周期与重验准入；9份文档67个链接和两专项工作树diff检查通过。正式业务仍未开工，未重新构建/测试或合入 |
| 2026-09-15 v1.4 | 准备后并行完成 E02-01/02：23文件可迁移源码SDK及三个目标构建通过，真实打印消费验收2/2 PASS、3 TIFF/Reader/18条日志通过；修复宏兼容、导出尺寸和验收证据/动态缓冲读取；14文档80链接及diff检查通过；保留长路径与旧二进制helper缺口，E01B仍缺正式入口；未提交或合入 |
