# DOC_DECISION_RIPFLOW 切片后外置 RIP 模块与自动处理边界

> 文档状态：**ACCEPTED / USER AUTHORIZED**
> 版本：v1.2 ｜ 日期：2026-08-17
> 定位：独立补充专项 `RIPFLOW` 的权威边界；不占 Stage 编号，不改写 Stage 14 历史结论
> 任务真源：`docs/codex_task/current/TASKS_RIPFLOW_切片后外置RIP集成专项任务清单.md`
> 上游合同：`DOC_DECISION_14_S2_RIP接口合同定案.md`、`DOC_DECISION_14F_外部验证延期与接口冻结.md`
> 证据等级：A=当前代码/文件/可重复实测，B=本决策目标，C=供应方 README 自述，P=工程判断

---

## 1. 用户授权与专项目标

用户于 2026-08-17 授权把“切片完成后接入 RIP”作为独立专项处理，并授权以下顺序：

```text
计划与任务拆分完整
  -> 准备阶段
  -> PREPARATION GATE PASS
  -> 开发阶段
```

本专项交付目标：

1. 在当前切片软件建立可独立迁移的外置 RIP 模块目录；
2. 提供 RIP 设置、手动执行和“切片后自动执行”开关；
3. 保留切片目录名 `layers`，在同一 `package` 根下生成同级目录 `rip`；
4. 以真实 TIFF 检查和 fail-closed 方式约束当前二进制，不把本地接入冒充外部生产验收；
5. 输出模块清单、配置快照、运行结果和移植说明，供打印软件按目录整体接入。

任务状态只允许在 `TASKS_RIPFLOW` 中更新。本文只冻结边界，不代替任务完成证据。

## 2. 当前事实与合同冲突

### 2.1 当前可用 RIP 资产（A/C）

`rip_project` 当前包含：

```text
rip_cli.exe
RipSlicer.dll
tiff.dll
rip_cli.c
rip_slicer.h
README.md
CmykFiles/0.matrix .. 3.matrix
CmykFiles/linear.csv
CmykFiles/CIERGB.icc
CmykFiles/CMYK.icc
CmykFiles/JapanColor2001Coated.icc
```

公开参数为 `--dll`、`--input/-i`、`--file/-f`、`--output/-o`、
`--resource/-r`、`--number/-n`、`--rgb-icc`、`--cmyk-icc`、`--intent 0..3`、
`--transparent 0|1`、`--colormode`、`--keep-going/-k`、`--quiet/-q`。

README 所述完整实现源文件、CMake/qmake 工程、导入库、示例和许可证并未全部随目录提供；
因此构建与一致性说明只作 C 级自述，不能替代当前二进制实测或重编证据。

### 2.2 已确认的适配缺口（A）

| 项 | 当前 RIP | 冻结 S1/S2 或项目现状 | 本专项处置 |
|---|---|---|---|
| 进程依赖 | 私有 `tiff.dll` 4.1.0 | 当前应用使用 LibTIFF 4.7.1，文件同名 | 必须进程隔离，不在宿主 `LoadLibrary` |
| 输入存储 | scanline/stripped 可用，tiled 实测失败 | S1 允许 stripped 或 tiled | 首版只声明 stripped 子集，tiled fail-closed |
| 输出命名 | `slice.N.tiff` | S2 要求 `rip_%06d.tif` | 校验后仅重命名，不改写像素 |
| 输出宽度 | 非 4 对齐宽度会向上扩到 4 像素边界 | 发布结果必须与输入 Grid 身份一致 | 只允许该确定性右侧补齐，完整校验后裁掉 1..3 列；其他尺寸差异 fail-closed |
| W 上限 | `transparent=0` 实测可出现 W=9 | grayBits=2 时 W 必须 `<=6` | 真实像素扫描，不合格不发布 |
| 白语义 | CLI 只有 `transparent 0|1` | manifest `whiteSemantics` 为权威 | 映射未定前只允许显式候选并 fail-closed |
| 颜色模式 | `colormode` 默认 0 | 未提供枚举和值域语义 | 首版只允许 0；其他值外部阻塞 |
| 输出合同 | 固定 7 通道、LZW、600 dpi 自述 | S2 还要求 stripped、层数、量化和外部极性 | 从真实 TIFF 提取证据，不信任描述符 |

Stage 14 的 `RipOutputValidator.ps1` 当前验证的是机器描述符，不是目标 RIP 真实 TIFF。
本专项必须补真实输出检查，不能把既有 C1-C7 描述符门禁直接记作目标二进制 PASS。

## 3. 架构决策

### 3.1 采用进程外适配

采用 Qt `QProcess` 直接启动绝对路径 `rip_cli.exe`：

```text
slicer_ui_host_sim
  -> HostRipJobController (QProcess，无 shell)
  -> modules/rip/rip_cli.exe
  -> modules/rip/RipSlicer.dll
  -> modules/rip/tiff.dll 4.1.0 + CmykFiles
```

约束：

- 可执行文件与每个参数分别传入，禁止拼接命令交给 `cmd.exe`、BAT 或 shell；
- `--dll`、`--resource`、`--input`、`--output` 均由适配层解析为绝对路径；
- 捕获 stdout/stderr、退出码、耗时和取消结果；
- 子进程私有依赖不复制到宿主根目录，避免覆盖当前 LibTIFF 4.7.1；
- 不修改 `PM_SPI_VERSION`、11 个导出、15 项能力、Worker 文件合同或 slicer module ABI；
- RIP 设置不进入切片 Profile hash，不改变切片结果或 `p0.rgbwsv.2`。

非 4 对齐宽度适配属于进程外边界归一化，不修改输入 Package：仅当输出高度不变且
`actualWidth == align_up(packageWidth, 4)` 时，适配层在 staging 中重写每行前
`packageWidth` 个像素，并以 Package 原宽发布。重写前仍须通过层数、索引、布局和逻辑
宽度内 W/S/V 扫描；其他扩宽、缩窄或高度变化继续 fail-closed。该规则不改变 S2 最终尺寸合同。

### 3.2 未采用方案

| 方案 | 结论 | 原因 |
|---|---|---|
| 宿主进程内 `LoadLibrary(RipSlicer.dll)` | 不采用 | 同名 TIFF 版本冲突、MinGW/MSVC 二进制边界和崩溃域风险 |
| 通过 BAT 作为产品入口 | 不采用 | 参数转义、中文路径、日志、取消和进程树管理不可形成稳定宿主合同 |
| 取得源码后统一重编 | 后续候选 | 长期更理想，但当前缺完整源码、构建输入和来源/许可证证据 |

BAT/PowerShell 仍可作为诊断与验收入口，但不得成为 Qt 产品链的唯一执行机制。

## 4. 目录与所有权决策

### 4.1 可迁移运行时模块

打印软件迁移单位固定为整个目录：

```text
modules/rip/
  rip_module.json
  runtime_dependencies.json
  rip_cli.exe
  RipSlicer.dll
  tiff.dll
  CmykFiles/
    0.matrix .. 3.matrix
    linear.csv
    CIERGB.icc
    CMYK.icc
    JapanColor2001Coated.icc
  licenses/
```

`rip_module.json` 至少记录 schema、模块版本、入口、DLL、资源目录、支持的输入子集、默认配置、
文件 SHA-256、目标架构和外部验收状态。运行时发现和完整性校验只依赖模块根，不依赖仓库路径。

### 4.2 作业输出目录

冻结结构：

```text
<session>/package/
  manifest.json
  layers/
  reports/
  rip/
    rip_result.json
    rip_000000.tif
    rip_000001.tif
    ...
```

裁决：

- `layers` 保持原名；本专项不得顺手重命名、迁移或改变其 manifest 相对路径；
- RIP 最终目录名固定为 `rip`，与 `layers` 同级；
- `layers`、manifest 和原报告归切片 Writer 所有；`rip` 归 RIPFLOW 所有；
- `rip` 是下游派生产物，不加入或改写冻结 S1 manifest；原有层文件 SHA-256 必须前后不变；
- 原始 CLI 输出先写到受控 `.rip.staging.<jobId>`，通过真实检查后改名并原子发布为 `rip`；
- 首版遇到已有 `rip`、无所有权标记或任一输出不合格时 fail-closed，不删除未知目录；
- 取消/失败只能清理经路径约束证明属于本次作业的 staging，不能影响有效切片包。

严格 Reader、结果重开和恢复流程必须验证其能够容忍该下游命名空间；若事实证明冻结合同拒绝
额外目录，停止开发并建立 Stage 14 受控修订，禁止偷偷修改 Reader 或 manifest。

## 5. RIP 设置合同

RIP 设置使用独立 `slicesoft.rip.settings.1`，不并入 `HostSliceSettings` 或 workspace schema v6：

| 字段 | 默认值 | UI/执行约束 |
|---|---|---|
| `autoAfterSlice` | `false` | 操作员显式开启后才自动运行 |
| `renderIntent` | `0` | 只允许 `0..3` |
| `transparentMode` | `follow_manifest` | 映射未闭合时不猜测；直接 0/1 仅候选诊断 |
| `colorMode` | `0` | 首版只允许 0，未知值 fail-closed |
| `inputIcc` | `CmykFiles/CIERGB.icc` | 必须位于已校验模块资源或显式准入路径 |
| `outputIcc` | `CmykFiles/CMYK.icc` | 可选择已准入 ICC；文件 hash 入结果 |
| `continueOnError` | `false` | 自动流程默认失败即停 |
| `deviceGrayBits` | `2` | 只约束真实输出 W/S/V 上限；当前 CLI 无 grayBits 参数，不得宣称它会改变 RIP 算法 |
| `timeoutSeconds` | `3600` | 只允许 `1..86400`，超时后 terminate/kill 并清理本次 staging |
| `outputDirectoryName` | `rip` | 首版只读固定值 |
| `existingOutputPolicy` | `fail_closed` | 不覆盖未知或既有结果 |

`--dll`、`--resource`、`--input`、`--output` 是适配层派生路径，不允许普通 UI 任意拼接；
`--file/--number` 不属于首版批量生产流程；`--quiet` 固定关闭以保留审计日志。

UI 必须展示运行时完整性、配置有效性、自动开关、手动运行、进度、取消、日志摘要和最终状态。
未知设置、非法枚举、缺失资源、hash 不一致或路径越界均 fail-closed。

## 6. 作业状态机与切片接缝

RIP 作业状态机：

```text
idle -> preflighting -> starting -> running -> validating -> publishing -> succeeded
  \          \            \          \            \-> failed
   \----------\------------\-----------> cancelling -> cancelled | failed
```

接缝规则：

1. 切片 Worker 必须先 `succeeded`；
2. `HostPackageReviewController` 必须完成 strict 加载，不能在未验证 package 上启动 RIP；
3. `autoAfterSlice=false` 时不创建进程、不创建目录，状态为 `not_requested`；
4. 自动开启后先做 S1/manifest/layers/grayBits/whiteSemantics/stripped 前置检查；
5. RIP 成功须通过真实输出 Gate 后才发布 `rip`；
6. RIP 失败不改写“切片成功”，UI 必须分别显示“切片成功 / RIP 失败”；
7. 关闭窗口、操作员取消或超时必须等待子进程收口，并验证无本次 staging 残留；
8. 同一 package 同时最多一个 RIP 作业，禁止重复提交。

`rip_result.json` 记录设置快照、输入 manifest/profile hash、运行时与资源 hash、命令参数的安全化
表达、退出码、层数、时长、真实 TIFF 摘要、发布状态和 `EXTERNAL_VALIDATION_DEFERRED`。

## 7. S1/S2 与外部边界

本专项始终保持：

```text
p0.rgbwsv.2；R,G,B,W,S,V；uint8；black_is_print；
manifest whiteSemantics 权威；profile.device.grayBits 缺失即失败；
S2 输出每层一个 >=7 samples 的 stripped 交错 TIFF；
grayBits=2: W<=6、S<=9、V<=9；grayBits=1: W<=2、S<=3、V<=3。
```

当前二进制固定写出 600 x 600 DPI，故本地候选在进程启动前只接收 600 x 600 DPI Package；
635 x 600 等其他 Grid 直接 fail-closed。当前 S1 层 TIFF 不含 DPI 标签，输入 DPI 以 Package grid
为权威；输出 DPI 仍从真实 RIP TIFF 标签读取并校验。`deviceGrayBits` 是输出准入期望，不是 CLI
控制项：本地实测只证明 `explicit_transparent + grayBits=2` 子集，其他组合按真实结果决定是否发布。

以下状态不得因本地开发而升级：

- `colormode` 语义与有效范围未获权威说明；
- `transparent` 与 manifest `whiteSemantics` 的映射未书面闭合；
- W/S/V 极性仍由 RIP 与打印软件双边确认；
- 目标打印软件 ChannelSplitter、干净机、实物打印和长稳未验收；
- 当前二进制是否能覆盖全部 grayBits/白语义矩阵尚未证明。

因此本专项可达到 `SLICER_SIDE_COMPLETE / EXTERNAL_VALIDATION_DEFERRED`，但不能写成
`EXTERNAL_ACCEPTED`、`PRODUCTION_READY` 或“目标打印链已 PASS”。

## 8. 许可证与分发边界

当前目录没有足以闭合以下分发责任的材料：

1. `RipSlicer.dll`/算法和 `rip_cli.exe` 的所有权、版本来源与再分发授权；
2. 静态链接 lcms2 的版本、来源和许可证随附；
3. 三份 ICC Profile 的来源与再分发许可；
4. 私有 `tiff.dll` 4.1.0 的确切构建来源、补丁和 SBOM 绑定。

可以生成本机工程验证包和目录迁移机制，但在材料闭合前，外部分发卡保持
`BLOCKED_EXTERNAL`，交付物不得标记为可对外发布。现有仓库 LibTIFF 许可证文件不能自动证明
其他二进制、lcms2 或 ICC 的授权。

## 9. 计划、准备与开发 Gate

计划完整 Gate：需求有任务映射、冻结边界明确、方案比较完成、路径与所有权确定、风险及验证
矩阵完整。该 Gate 已于 2026-08-17 通过。

准备 Gate：文件所有权、配置 schema、状态机、依赖方向、失败清理、真实 TIFF Gate、构建测试入口
和外部停止条件全部明确。该 Gate 已于 2026-08-17 通过，允许首张开发卡进入 `READY`。

开发阶段仍按原子卡推进。每张卡完成后必须更新 `TASKS_RIPFLOW` 的状态、日期、实际验证结果和
修订记录；不得因为用户已授权整个专项而跳过单卡 Gate、验证或外部状态约束。

## 10. 非目标与停止条件

本专项不重写 RIP 算法、不反向工程 DLL、不更改 S1/S2 冻结语义、不把 RIP 放入 slicer Worker、
不改变 Legacy 默认切片路径、不默认开启自动 RIP、不删除原始 `layers`。

出现以下任一情况必须停止并建立受控修订：需要修改 Stage 14 冻结接口；需要修改
`p0.rgbwsv.2`、通道、位深或极性；严格包读取拒绝 `rip` 命名空间；真实输出无法满足尺寸、层数或
W/S/V 上限；需要把私有 TIFF DLL 装入宿主进程；需要在许可证未闭合时声明外部分发可用。

## 11. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-17 | v1.0 | 建立 RIPFLOW 专项；冻结进程外适配、`modules/rip`、`layers`/`rip` 同级、独立设置、自动默认关闭、真实输出 fail-closed、S1/S2 与外部分发边界；计划与准备 Gate 通过 |
| 2026-08-17 | v1.1 | 明确 `deviceGrayBits` 仅为输出校验期望、当前二进制固定 600 x 600 DPI、S1 TIFF 的 DPI 由 Package grid 持有；本地支持范围不得外推 |
| 2026-08-17 | v1.2 | 受控接纳 RIP 固定的 4 像素右侧补齐：只在高度一致且宽度等于 `align_up(packageWidth,4)` 时裁回 Package 原宽；其余尺寸差异继续 fail-closed，不改变 S2 最终 Grid |
