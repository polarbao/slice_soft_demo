# TASKS_RIPFLOW 切片后外置 RIP 集成专项任务清单

> 文档状态：**SLICER_SIDE_COMPLETE / EXTERNAL_VALIDATION_DEFERRED**
> 版本：v1.9 ｜ 日期：2026-08-17 ｜ 用户授权：2026-08-17
> 定位：独立补充专项，不占 Stage 编号
> 权威决策：`docs/slice/DOC/DOC_DECISION_RIPFLOW_切片后外置RIP模块与自动处理边界.md`
> 准备文档：`docs/slice/DOC/DOC_PREP_RIPFLOW_外置模块设置与自动后处理准备.md`
> 规则：**本清单是 RIPFLOW 任务状态的唯一真源。**报告、Decision、代码注释不得覆盖此处状态。

---

## 0. 用户授权与当前判定

用户授权按“计划 -> 准备 -> 开发”连续推进，并允许用多 agent 提高效率。2026-08-17 已完成
计划完整性和准备 Gate 复核：

```text
RIPFLOW-00 / A / B / C / D COMPLETE
下一张本地卡              无
本地候选集成             SLICER_SIDE_COMPLETE
外部分发                 BLOCKED_EXTERNAL
生产/打印侧外部验收      BLOCKED_EXTERNAL
```

多 agent 只允许并行处理文件所有权不重叠、依赖已满足的卡。完成任一卡后必须更新该卡状态、完成
日期、实际验证结果和本文修订记录；禁止多个 agent 同时修改宿主主窗口、CMake 或 Runtime 脚本。

## 1. 状态定义

| 状态 | 含义 |
|---|---|
| `PLANNED` | 已拆分，前置或首卡顺序未满足 |
| `READY` | 准备完成，可进入实现 |
| `IN_PROGRESS` | 正在实现，只允许一个 owner |
| `COMPLETE / PASS` | 实现和该卡验证全部通过 |
| `BLOCKED_EXTERNAL` | 只能由外部输入、许可证或目标环境证据解除 |
| `DEFERRED` | 当前范围明确不实施，需新授权恢复 |

专项本地出口最多为 `SLICER_SIDE_COMPLETE / EXTERNAL_VALIDATION_DEFERRED`；只有取得真实外部
证据后才能使用 `EXTERNAL_ACCEPTED`，任何本地测试不得写成 `PRODUCTION_READY`。

## 2. 固定边界

```text
采用 QProcess 进程外调用，禁止宿主进程内加载 RipSlicer.dll；
自动 RIP 默认关闭；手动与自动复用同一严格路径；
保留 package/layers，不重命名、不改 manifest；同级输出固定 package/rip；
RIP 只在切片成功且 package strict 加载成功后启动；
RIP 设置独立于切片 Profile 和 HostWorkspaceState v6；
S1/S2、PM_SPI_VERSION、11 导出、15 能力和 Worker 合同不变；
真实输出任何不合格均 fail-closed，不发布半成品；
外部分发和生产验收未闭合，保持 BLOCKED_EXTERNAL。
```

## 3. 任务总览与依赖

| Wave | 目标 | 卡片 | 当前出口 |
|---|---|---|---|
| RIPFLOW-00 | 计划、审计、决策和准备 | 00-01..04 | COMPLETE |
| RIPFLOW-A | 可迁移运行时与机器合同 | A-01..03 | COMPLETE |
| RIPFLOW-B | 设置、持久化和 UI | B-01..03 | COMPLETE |
| RIPFLOW-C | 执行、验证、发布和自动接线 | C-01..04 | COMPLETE |
| RIPFLOW-D | 矩阵、Runtime 迁移、本地收口和缺陷修复 | D-01..04 | COMPLETE |
| RIPFLOW-E | 外部分发与生产验收 | E-01..02 | BLOCKED_EXTERNAL |

依赖主链：

```text
00 -> A-01 -> A-02 -> A-03
          \-> B-01 -> B-02 -> B-03
A-03 + B-03 -> C-01 -> C-02 -> C-03 -> C-04
C-04 -> D-01 -> D-02 -> D-03 -> D-04
D-03 + 外部材料 -> E-01/E-02
```

## 4. RIPFLOW-00 计划与准备

### RIPFLOW-00-01 资产、参数与依赖盘点

**状态：COMPLETE（2026-08-17）**

**内容：** 读取 `rip_project` 全部文件，登记 EXE/DLL/TIFF/资源、公开 API、CLI 参数、默认值、
错误码、线程和路径约定，区分当前文件与 README 自述。

**验收：** 文件清单完整；参数可追溯到 `rip_cli.c`/`rip_slicer.h`；缺失源码、构建输入和许可证
不得伪记存在。

**实际结果：** 14 个文件完成盘点；确认批量/单张、ICC、intent、transparent、colormode、继续、
quiet 等参数；确认 DLL 为二进制依赖，完整实现源与分发材料不在目录中。

### RIPFLOW-00-02 S1/S2 与真实行为差距审计

**状态：COMPLETE（2026-08-17）**

**内容：** 对照冻结 `p0.rgbwsv.2` 和 S2 C1-C7，登记输入、命名、尺寸、量化、白语义、极性、
输出组织及真实 TIFF 检查缺口。

**验收：** 明确哪些是本地可适配、哪些必须 fail-closed、哪些只能由外部证据解除。

**实际结果：** 登记 `slice.N.tiff` 命名差异、tiled 不支持、非 4 对齐扩宽、W 上限超约束、
`colormode`/透明映射未知和描述符不等于真实输出等问题；生产状态保持外部延期。

### RIPFLOW-00-03 架构、目录与计划完整性裁决

**状态：COMPLETE（2026-08-17）**

**内容：** 比较进程外、进程内和源码重编路线；冻结模块目录、输出所有权、设置边界、状态机、
失败处置和开发波次。

**验收：** 四项用户需求逐一有任务和 Gate；不修改 Stage 14 冻结合同；任务可由不同 owner 原子执行。

**实际结果：** 采纳 QProcess + `modules/rip`；保留 `layers`，同级固定 `rip`；自动默认关闭；
设置与切片 Profile 分离；计划 Gate PASS。

### RIPFLOW-00-04 准备 Gate

**状态：COMPLETE / PASS（2026-08-17）**

**内容：** 冻结文件所有权、schema、执行状态机、路径约束、真实 TIFF Gate、构建/测试入口、并行边界
和停止条件。

**验收：** 准备文档无未分配的本地设计问题；首张开发卡可独立执行；外部问题有明确阻塞卡。

**实际结果：** `PREPARATION GATE PASS / IMPLEMENTATION READY`；A-01 转 `READY`；许可证和目标
打印链不阻塞本地候选开发，但继续阻塞外部分发与生产验收。

## 5. RIPFLOW-A 可迁移运行时与合同

### RIPFLOW-A-01 模块、设置和结果机器合同

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** RIPFLOW-00-04

**内容：** 新增 `slicesoft.rip.module.1`、`slicesoft.rip.settings.1`、
`slicesoft.rip.result.1` schema、正例 fixture 和负例合同测试；冻结字段、默认值、枚举、相对路径、
hash、外部状态和错误码。

**验收：** 未知/缺失字段、路径逃逸、非法 intent/transparent/colorMode、自动默认开启、错误外部
状态均 fail-closed；不修改现有 S1/S2 合同和能力 DTO。

**实际结果：** 已新增 module/settings/result 三份 JSON Schema、默认设置、运行时依赖声明、三份
正例 fixture 和标准库 Python 合同门禁。`ValidateRipflowContracts.py` 实测
`positive=3 negative=4` PASS；默认自动关闭、路径逃逸、非法 colorMode、生产状态误报均已覆盖。

### RIPFLOW-A-02 `modules/rip` 本地工程包

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** A-01

**内容：** 实现 `PackageRipModule.ps1`，从 `rip_project` 复制受控文件到 staging，生成 manifest、
依赖清单和 SHA-256 后原子发布；私有 `tiff.dll` 只留在模块目录。

**验收：** 包内文件完整且无仓库绝对路径；篡改/缺失检测稳定；不覆盖宿主根的 tiff.dll；本地包
明确标记 `LOCAL_ENGINEERING_ONLY` 和许可证阻塞。

**实际结果：** 已新增 `PackageRipModule.ps1` 和 `TestRipModulePackage.ps1`，从忽略的本地
`rip_project` 复制 11 个受控运行文件，生成 SHA-256 manifest、独立 `tiff.dll`、默认设置和
许可证阻塞声明，再通过同父目录 rename 原子发布。本机工程包生成 PASS，11/11 文件/hash 和
`rip_cli --help` 自检 PASS；未复制到宿主根，状态保持 `LOCAL_ENGINEERING_ONLY`。

### RIPFLOW-A-03 模块发现、自检与迁移入口

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** A-02

**内容：** 按应用根相对发现 `modules/rip`，校验 schema/hash/架构/资源，提供命令行自检和打印软件
目录迁移说明；接入 `PrepareSliceSoftRuntime.ps1` 的隔离子目录。

**验收：** 空格/中文路径和独立 Runtime 可发现；不依赖 PATH、当前工作目录或仓库；缺文件和版本
不符 fail-closed；未闭合许可证时不产生外部分发声明。

**实际结果：** 宿主按应用根相对发现 `modules/rip`，逐个验证 module schema、11 个运行文件的
大小/SHA-256、入口/DLL/资源和本地安全状态；CMake 构建后只把私有 `tiff.dll` 放入模块子目录。
模块自检 CTest PASS，`PrepareSliceSoftRuntime.ps1` 已接入可选模块打包与独立状态清单；SDK 缺失时
Runtime 仍可生成但 RIP 保持禁用。

## 6. RIPFLOW-B 设置、持久化与 UI

### RIPFLOW-B-01 设置 DTO、校验和 CLI 映射

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** A-01

**内容：** 实现 Qt 无关设置模型及 manifest/CLI 映射；区分用户设置与 `--dll/-i/-o/-r` 派生路径。

**验收：** `autoAfterSlice=false`；intent 只允许 0..3；colorMode 首版只允许 0；白语义映射未知时
失败；ICC 路径有根约束；生成参数不经 shell。

**实际结果：** 新增 Qt 无关 `RipSettings`、绝对/contained 路径命令构建和无 shell argv；intent、
colorMode、ICC、grayBits 输出期望及固定 `rip` 均严格校验。`rip_integration_unit_tests` 在 MSVC
C++20 `/W4 /WX` 下 PASS，覆盖默认自动关闭、非法枚举、路径逃逸和参数完整性。

### RIPFLOW-B-02 独立设置持久化

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** B-01

**内容：** 新增独立 QSettings group/schema/store，保存 RIP 设置，不进入 `hostslicesettings`、
Profile hash 或 `HostWorkspaceState` v6。

**验收：** 保存/恢复/旧版本/未知版本/损坏值测试；失败恢复为自动关闭且配置无效；self-test 不污染
用户设置。

**实际结果：** 新增独立 `hostflow/rip` QSettings schema v1，不修改 workspace v6 或 Profile hash；
未知版本/损坏值恢复为默认并强制关闭自动 RIP。`ripflow_settings_unit_tests` 覆盖保存、恢复、未知
版本、自动关闭和 ICC 路径逃逸，PASS；self-test 路径禁用持久化，不污染用户设置。

### RIPFLOW-B-03 RIP 设置页与手动入口

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** B-02、A-03

**内容：** 增加独立 RIP 设置页，展示自动开关、intent、白语义、colorMode、ICC、失败策略、运行时
完整性、`layers -> rip` 路径、手动运行和禁用原因。

**验收：** 未校验 package、无运行时、已有作业或未知语义时按钮禁用；不可用选项可解释但不能提交；
布局和文字在目标分辨率无重叠；UI 测试覆盖交互和持久化。

**实际结果：** 右侧新增独立“RIP 设置”页，提供自动开关、intent、白语义、固定 colorMode、ICC、
失败继续、grayBits 输出校验、超时、只读 `layers -> rip` 路径、运行时/前置状态、运行/取消/打开。
无严格包、无模块、语义/DPI 不支持、已有输出或作业运行时按钮禁用；`ripflow_ui_self_test` 与既有
HostFlow 设置/作业/结果/workspace UI 定向回归 PASS。

## 7. RIPFLOW-C 执行、验证、发布与自动接线

### RIPFLOW-C-01 唯一 QProcess 执行控制器

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** A-03、B-03

**内容：** 实现绝对程序路径、分离参数、子进程环境、stdout/stderr、退出码、启动失败和单作业约束。

**验收：** 空格/中文路径通过；参数不可注入；exit 0/1/2、缺 EXE/DLL/TIFF 和资源错误映射稳定；
同一 package 不可并发；宿主不加载私有 DLL。

**实际结果：** `HostRipJobController` 以绝对 `setProgram` + 分离 `setArguments` 启动唯一 QProcess，
子进程 PATH 只追加模块根，捕获受限 stdout/stderr 和退出码。真实 CLI 20 层、中文/空格路径 PASS；
fake CLI 的 exit 1/2 均映射 `RIP_PROCESS_EXIT_FAILED`，无半成品。

### RIPFLOW-C-02 状态机、取消、超时与关闭收口

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** C-01

**内容：** 实现 `preflighting -> running -> validating -> publishing`、协作取消、超时强制终止、
窗口关闭和 UI 双状态。

**验收：** 取消/失败/崩溃无本次 staging 或子进程残留；RIP 失败不改变切片成功；重复取消幂等；
进度不回退，非法输出 fail-closed。

**实际结果：** 状态覆盖 starting/running/validating/publishing/cancelling；S1/S2 全层扫描在后台线程
执行，验证期保持 active 并通过原子 token 逐层/逐 scanline 响应取消。超时 terminate 后定时 kill，
析构确认进程停止并 join 验证线程后才请求清理；清理要求 canonical 同父目录、固定 staging 前缀且
全树无 junction/reparse point，不安全时拒绝删除并保留现场。生命周期 Gate 实测 cancel、timeout、
exit 1、exit 2 全部 PASS，未发布 `rip` 且无安全 staging 残留；RIP 终态与切片成功状态分开显示。

### RIPFLOW-C-03 S1 前置、真实 S2 验证与原子发布

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** C-02

**内容：** 校验 package/manifest/layers，运行到 `.rip.staging.<jobId>`，解析真实 7 通道 TIFF，
归一化为 `rip_%06d.tif`，生成 `rip_result.json` 并发布 `rip`。

**验收：** 层数、索引、宽高、DPI/Grid、8bit、samples、planar、stripped、W/S/V 范围全通过；
tiled、扩宽、缺层、坏层、超限全部拒绝；原 layers/manifest/hash 前后不变；已有未知 `rip` 不覆盖。

**实际结果：** 启动前复核 production Package、层清单、真实 unsigned 8bit/6ch/contiguous/stripped
TIFF、尺寸和可解码性，并冻结 manifest 与逐层 canonical path/size/SHA-256；输出逐层检查层数/索引、
unsigned 8bit/>=7ch/contiguous/stripped/尺寸/600 DPI/WSV 上限。发布前重新计算全部源身份，任一变化
以 `RIP_SOURCE_PACKAGE_CHANGED` fail-closed，再归一化 `rip_%06d.tif`、写结果合同并同父目录 rename
发布。core 与安全清理正负例 PASS；真实 20 层发布 PASS；signed sample、tiled、缺层、尺寸/DPI/范围、
输入篡改及已有输出均 fail-closed。

### RIPFLOW-C-04 切片完成后的自动 RIP

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** C-03

**内容：** 只在 Worker 成功且 `HostPackageReviewController::LoadAsync` strict PASS 后读取新设置快照，
按开关启动同一控制器；补手动/自动一致性和下一会话路径处理。

**验收：** 默认关闭时零进程、零目录；结果加载失败时不启动；开启后只触发一次；UI 分别显示切片和
RIP 终态；切片编辑状态、下一输出目录和结果查看无回归。

**实际结果：** 自动入口只位于 `HostPackageReviewController::LoadAsync` 成功回调之后；默认关闭时
仅显示 `not_requested`，不启动进程或创建目录。手动/自动共用 `StartRipForPackage` 和唯一控制器；
静态 wiring Gate 确认未直接挂 Worker 完成信号、未走 shell、`layers/rip` 同级；控制器单作业和
已有输出保护阻止重复触发，RIP 失败不撤销切片成功或结果查看。

## 8. RIPFLOW-D 矩阵、迁移与本地收口

### RIPFLOW-D-01 兼容、负例和回归矩阵

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** C-04

**内容：** 覆盖手动/自动、grayBits 1/2、opaque/transparent、ICC、PackBits 输入、中文路径、异常
运行时、取消、尺寸扩宽、W/S/V 超限、tiled 和已有输出等矩阵。

**验收：** 支持组合逐层真实校验 PASS；不支持组合稳定 fail-closed；自动关闭零影响；原生产
Package/RIP Reader/HostFlow Debug 与 Release 定向回归通过。

**实际结果：** 合同 3 正/4 负、Debug 与 Release 各 12/12 focused CTest（core、settings、safety、
module/UI、wiring 与 HostFlow 回归）全部 PASS；真实矩阵
验证 transparent+grayBits2 与中文/空格路径 PASS，follow_manifest 缺权威值、opaque grayBits2、
transparent grayBits1、635x600、tiled/坏布局稳定 fail-closed；cancel/timeout/exit1/exit2 均无残留。
当前支持面明确收窄为 600x600、stripped、explicit_transparent、colorMode0、grayBits2 本地候选。

### RIPFLOW-D-02 Runtime 与打印软件目录迁移验证

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** D-01、A-03

**内容：** 把 `modules/rip` 放入全新隔离 Runtime，按相对路径完成自检、手动和自动候选运行；输出
机器清单与迁移步骤。

**验收：** 不访问仓库、不依赖系统 PATH、不误加载宿主 tiff.dll；模块/资源 hash 可审计；许可证
未闭合时产物保留内部工程标记。

**实际结果：** 新增隔离迁移 Gate，把宿主、Qt 运行时和整个 `modules/rip` 复制到带中文/空格的全新
目录；应用按自身目录相对发现模块，逐 hash 自检并完成真实 20 层 RIP。根目录只保留宿主
`tiffd.dll`，私有 `tiff.dll` 只在 `modules/rip`；运行不依赖仓库工作目录，输入层 hash 不变。
`RIPFLOW_MIGRATION_PASS relativeModule=true privateTiff=true realJob=true`。该本机隔离结果不替代
外部干净机/许可证验收；VS 2026 Hostx64 的 `Release/slicesoft_runtime` 构建及 Release 12/12
定向 CTest 已 PASS，但不冒充目标打印软件环境验收。

### RIPFLOW-D-03 本地收口与使用说明

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** D-02

**内容：** 输出当前状态报告和用户/移植说明，更新本清单，汇总实际支持矩阵、失败码、性能、限制和
外部待办。

**验收：** 代码、合同、Runtime、报告和指南互相一致；状态只写
`SLICER_SIDE_COMPLETE / EXTERNAL_VALIDATION_DEFERRED`；无生产或外部分发误报。

**实际结果：** 已输出 `REPORT_RIPFLOW_外置RIP本地候选当前状态.md` 与
`SLICE_RIPFLOW_切片后RIP设置与迁移说明.md`，同步机器合同、支持面、执行/错误边界、实测矩阵、
迁移步骤和外部阻塞。最终本地状态为 `SLICER_SIDE_COMPLETE / EXTERNAL_VALIDATION_DEFERRED`；
E-01/E-02 保持 `BLOCKED_EXTERNAL`，未写生产或外部分发 PASS。

### RIPFLOW-D-04 非 4 对齐宽度归一化缺陷修复

**状态：COMPLETE / PASS（2026-08-17）**

**依赖：** D-03；运行时缺陷 `RIP_OUTPUT_DIMENSION_MISMATCH`

**内容：** 修复当前 RIP 将非 4 对齐输入宽度向上补齐后被 S2 尺寸 Gate 拒绝的问题，同时保持
最终 `rip_%06d.tif` 与 Package Grid 精确一致。

**验收：** 只接受高度不变且 `actualWidth == align_up(packageWidth, 4)` 的 1..3 像素右侧补齐；
先完成真实 TIFF/逻辑像素校验，再在 staging 中裁回 Package 原宽；其他尺寸差异、重写失败和取消
继续 fail-closed；输入 `layers` 不变。

**实际结果：** 用户样例实测 Package 为 `1842 x 623`，当前 RIP 输出为 `1844 x 623`，确认错误
来自 DLL 固定的 4 像素行宽对齐而非切片 Grid 损坏。适配层已增加受限重写，95 -> 96 合成 LZW
回归可归一化为 95，97 -> 95 等非对齐差异仍以 `RIP_OUTPUT_DIMENSION_MISMATCH` 拒绝；
`rip_integration_unit_tests` 与 Release 宿主构建 PASS；用户包前 30 层真实 RIP 由 1844 裁回
1842，30/30 层发布及结果合同 PASS。对完整 175 层数据继续实测后，尺寸 Gate 已通过，但第 30 层
出现 `W=255`，随后按冻结 C6 上限以 `RIP_OUTPUT_DROP_LIMIT_EXCEEDED`
拒绝且不发布 `rip`；该独立语义问题不得通过放宽上限或猜测极性绕过。

## 9. RIPFLOW-E 外部阻塞

### RIPFLOW-E-01 二进制、lcms2、ICC 与私有 LibTIFF 分发闭合

**状态：BLOCKED_EXTERNAL**

**依赖：** 权利方/供应方材料

**内容：** 取得 RipSlicer/CLI 的来源版本与再分发授权、lcms2 许可证、ICC Profile 授权、私有
LibTIFF 构建来源/SBOM，并纳入分发清单。

**验收：** 法务/权利方可追溯材料齐全；所有许可证随包；hash 与模块 manifest 对应；解除
`LOCAL_ENGINEERING_ONLY` 有书面依据。

**实际结果：** 当前目录材料不足，保持外部阻塞；本地工程开发不得把该状态改为 PASS。

### RIPFLOW-E-02 目标打印软件与生产 S2 验收

**状态：BLOCKED_EXTERNAL**

**依赖：** D-03、E-01、RIP/打印软件双边及真实设备

**内容：** 闭合 `transparent`/whiteSemantics、`colormode`、W/S/V 极性，执行目标
ChannelSplitter、干净机、并行/长稳和必要实物工艺验证。

**验收：** 目标环境真实 `rip_%06d.tif`、层数/尺寸/量化/白语义/极性证据和双方签字完整；失败
样例可重复；之后才允许 `EXTERNAL_ACCEPTED`。

**实际结果：** 目标 RIP、打印软件和实物证据未提供，保持 `BLOCKED_EXTERNAL`。

## 10. 专项完成 Gate

本地完成必须同时满足：A/B/C/D 全部 `COMPLETE / PASS`；自动默认关闭；手动与自动共用唯一严格
执行链；`modules/rip` 在隔离 Runtime 可迁移；真实 TIFF Gate 和负例矩阵通过；原 S1 package
逐文件身份无变化；报告明确外部延期。

E-01/E-02 未完成不阻止本地专项标记 `SLICER_SIDE_COMPLETE`，但阻止外部分发、生产默认开启和
`EXTERNAL_ACCEPTED`。自动开关在获得新的生产裁决前不得改为默认开启。

## 11. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-17 | v1.0 | 建立 RIPFLOW 权威任务清单；00-01..04 完成，A-01 READY；拆分 A-D 本地开发和 E 外部阻塞；固定每卡验收与实际结果栏 |
| 2026-08-17 | v1.1 | RIPFLOW-A-01 完成：冻结三份机器合同、默认设置和正负例门禁；A-02 与 B-01 转 READY |
| 2026-08-17 | v1.2 | RIPFLOW-A-02 完成：可迁移模块工程包、11 文件 hash、自检及许可证阻塞声明 PASS；A-03 转 READY |
| 2026-08-17 | v1.3 | A-03 与 B-01..03 完成：应用相对模块发现、自检、独立设置持久化和 RIP 设置页定向门禁 PASS |
| 2026-08-17 | v1.4 | C-01..04 完成：唯一 QProcess、生命周期、真实 S1/S2 Gate、原子发布及 strict-load 后自动接线 PASS |
| 2026-08-17 | v1.5 | D-01 完成：真实正例、中文路径、负例、取消/超时/退出码和 HostFlow 回归矩阵 PASS；D-02 转 READY |
| 2026-08-17 | v1.6 | D-02 完成：中文/空格隔离 Runtime 相对发现、模块私有 TIFF、真实 20 层迁移运行 PASS |
| 2026-08-17 | v1.7 | D-03 完成：状态报告与用户/迁移指南收口；本地专项完成，E-01/E-02 继续 BLOCKED_EXTERNAL |
| 2026-08-17 | v1.8 | P1 安全复核闭合：S1/S2 后台可取消验证、源 Package 运行时身份复验、junction/reparse 安全清理及 unsigned sample Gate；Debug/Release 各 12/12 定向 CTest、Release Runtime 与真实 local/lifecycle/migration Gate PASS |
| 2026-08-17 | v1.9 | D-04 完成：定位 1842 -> 1844 为 RIP 固定 4 像素对齐并在 staging 裁回 Package 原宽，用户包前 30 层真实发布 PASS；非确定性尺寸差异继续拒绝；完整包随后暴露的 W=255 仍按 S2 C6 fail-closed |
