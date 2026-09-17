# LOGDUMP E02 可迁移源码包与成功作业验收准备

> 2026-09-15，PREPARATION COMPLETE。用户继续执行授权覆盖本轮组件交付与验收；不建设 P23 产品装载器。
> 状态真源：[专项任务清单](../../codex_task/current/TASKS_LOGDUMP_日志与崩溃转储可复用模块专项任务清单.md)。
> 执行后更新：E02-01/02 已完成，详见[实际交付与验收报告](../REPORT/REPORT_LOGDUMP_E02_源码SDK交付与成功切片验收.md)。下文状态为开工前准备快照。

## Implementation Plan

### Problem Type

E01B 仍缺实际业务入口，但可提前完成两项组件交付：将已实现的日志/转储源码导出为可独立消费的包，并补齐真实打印日志后端下的成功切片验收。避免再次只复核相同阻塞而没有可用交付物。

### Layer(s) Involved

日志宿主适配层、可选 Windows 崩溃客户端/helper、源码交付脚本、独立无设备工程验收。产品装载、UI、设备控制和几何算法不在修改范围内。

### Official Documents

[E01 准备](DOC_PREP_LOGDUMP_E01_PrintApp真实日志适配准备.md)、[E01B 准备](DOC_PREP_LOGDUMP_E01B_业务挂接与新基线交付准备.md)、[A00 定案](DOC_DECISION_LOGDUMP_A00_开发准入与日志扩展定案.md)、现行 C 合同和日志使用说明。

### Historical Documents

[E01A 报告](../REPORT/REPORT_LOGDUMP_E01A_PrintApp真实日志适配验收.md)的成功是日志组件验收；唯一真实 Worker 请求故意使用非法 jobId，未证明成功切片。旧独立运行包为应用包，不是源码 SDK。

### AI Workspace Evidence

两个专项仍在 `slice_soft_demo-logdump` / `ry_print_demo-slicer-logdump`，分支为 `codex/feature-logging-dump` / `codex/slicer-logging-adapter`，起点分别 e2546797 / 22bfcd3d235d。已有修改原位保留。本轮原切片工作树仍有 P0FIX 合同、版本与测试编辑，另有未跟踪模型/资料；不操作它们。

### Current Code Reality

- E01A PrintAppSlicerLogging 复用四组通用源码，依赖消费者既有 spdlog/fmt/nlohmann_json，无 Qt 或设备 SDK。
- 当前 runtime 不含头文件/adapter 源码；消费 CMake 依赖完整切片根路径，用户还需手工选择最小文件集合。
- 崩溃客户端和 helper 已实现，可单独构建，但宿主必须显式决定异常处理器所有权。
- 原 PackageSlicerModule 脚本没有显式列出动态启动的 crash helper；本轮源码包不能被描述成该旧二进制模块包的修复或替代。应用完整包已带 helper。
- E01A 的非法 jobId 用例必须保留；成功验收需使用有效请求，不得通过修改业务合同使负例变绿。

### Current State

本地实现与 E01A 完成，E01B 前置准备完成但业务入口不存在。E02 的两项独立工作已具备源码、依赖和可信 DLL，可在原基线隔离实施。

### Target State

LD-E02-01：新增显式源码导出脚本、包内相对路径 CMake 和说明；生成全新中文目录的 SDK，文件清单和 SHA 可复核。提供日志 host target 和默认关闭的 crash target，复用源文件，不形成第二套日志实现。

LD-E02-02：新增独立成功作业验收，真实 PrintAppLogging + 真实 DLL/Worker 走有效模型导入、场景建立、切片、包校验和关闭。与原负例同时保留；进一步用导出的 SDK 作为诊断源码根重建验收，证明不依赖完整切片源码目录。

### Historical State

源码最小集合此前仅在手册列出，未形成可消费 SDK；模拟或失败请求 PASS 不代表成功模型链路。E02 不回填或改写 E01A 历史数字。

### Pending Confirmation

本轮没有待用户确认参数，不引入/升级依赖。P23 产品 loader、最终产品分支和正式部署验收继续归 E01B，不能用 E02 工程 EXE 冒充。

### Risk Points

- 只导出明确白名单，不递归复制模型、日志、DUMP、PDB、运行时或完整仓库；拒绝覆盖已有目标目录。清单记录源码快照与实际文件 hash，不以 HEAD 代表未提交内容。
- 源码 SDK 的 SPI/log API 与测试 DLL 身份分别记录；不同版本二进制不得因源码包存在而自动放行。
- 注入宿主 sink 仍保留 spdlog 编译依赖；不安装库、不改变全局 logger、线程池或默认异常处理器。
- 新验收沿用有效业务 Profile/hash 规则。Profile 包含动态绝对模型/输出路径，不能使用脱离路径的固定 hash；测试仅用既有 HostBuildProfile 布局的固定规范模板和 CMake 内建 SHA256，真实 Worker 再校验，不建立新的通用 Profile 序列化实现，也不链接完整 slicer_core。
- 所有 job 在模块和 DLL 前释放；clear 成功才关闭 adapter，最后 Flush/Shutdown。等待具有测试截止时间；取消仍按现有 SPI 执行。
- 工程成功作业不能替代 PrintApp GUI、硬件、干净机器或最终 P0FIX 基线验收。

### Files To Change

- 切片专项：`sdk/diagnostics/` 构建模板和说明、`scripts/ExportSliceSoftDiagnosticsSdk.ps1`、必要的导出验证；既有实现只复制到产物，不修改算法和业务合同。
- 构建复核发现 `SessionRetention.cpp` 重复定义打印宿主已有的 Windows 宏；补 `#ifndef` 保护，与已有公共文件规则一致。仅调整预处理兼容性，消费构建需覆盖宿主已定义宏的场景。
- 打印专项：`PrintSolution/integrations/slicer_logging/` 成功验收文件、必要 fixture、CMake 与说明；保留原负例。PrintApp 正式业务和全局日志实现不修改。
- 打印根 `.gitignore` 为成功验收的 OBJ 源夹具增加单文件例外，避免将它当作编译中间产物漏交付。
- 根执行者更新任务/报告/手册，单独管理 configure/build/CTest 和交付验证。

### Verification Plan

1. 导出前检查白名单及源版本，目标全新；检查完整清单、hash、无私有运行数据，已有目录拒绝覆盖。
2. 使用已有依赖与 MSVC/Visual Studio 18 2026，在独立 build 目录消费中文 SDK；构建日志和显式 crash targets，记录退出码。
3. 新增成功作业与原负例的两个验收 target，RelWithDebInfo 构建退出 0 后执行 CTest；真实切片包执行模块 Reader 校验，并确认日志来源和注销后宿主仍可写入。
4. 从导出 SDK 路径重建验收，记录 DLL/源码包与输出身份；不把运行记录迁移到新 P0FIX 分支。
5. 两个代理分工成功验收和源码导出，根执行者审查交叉边界并完成构建；最后更新任务状态、实际结果与 diff 检查。

## 任务拆分

| ID | 任务 | 准备结果 |
|---|---|---|
| LD-E02-01 | 可迁移日志/可选转储源码 SDK | PREPARED，交付入口与验证边界已明确 |
| LD-E02-02 | 真实成功作业及源码 SDK 消费验收 | PREPARED，原负例保留，按现有 SPI 和 Profile 合同执行 |

准备审查结论：两项均可独立开工，无须等待正式产品 loader；在专项隔离树内实施，最终业务集成状态仍由 E01B 判断。
