# DOC_PREP_RIPFLOW 外置模块、设置与自动后处理准备

> 状态：**PREPARATION GATE PASS / IMPLEMENTATION READY**
> 版本：v1.2 ｜ 日期：2026-08-17
> 对应专项：`RIPFLOW`
> 首张开发卡：`RIPFLOW-A-01`
> 权威决策：`DOC_DECISION_RIPFLOW_切片后外置RIP模块与自动处理边界.md`
> 状态真源：`docs/codex_task/current/TASKS_RIPFLOW_切片后外置RIP集成专项任务清单.md`

---

## 1. 准备结论

专项需求、架构、目录所有权、配置、状态机、失败处置、部署边界和验收矩阵已经拆到原子卡，
准备 Gate 结论为：

```text
GO_LOCAL_CANDIDATE
SLICER_SIDE_IMPLEMENTATION_READY
NO_GO_EXTERNAL_DISTRIBUTION_LICENSE_PENDING
NO_GO_PRODUCTION_EXTERNAL_ACCEPTANCE_PENDING
```

允许从 `RIPFLOW-A-01` 开始开发。首版必须保持自动 RIP 默认关闭，所有未闭合语义只能
fail-closed 或保持 UI 禁用，不能用默认猜测绕过。

## 2. 冻结输入与现有接缝

### 2.1 切片侧输入

- 成功包由 `RgbwsvPackageWriter` 在私有 staging 中完整写出、strict 校验后原子发布；
- 生产层路径固定为 `layers/layer_%06d.tiff`，manifest 为层列表权威源；
- 参考宿主在切片成功后异步调用 `HostPackageReviewController::LoadAsync`；
- RIP 自动钩子必须位于 `LoadAsync` 成功回调之后，不能直接接在 Worker `succeeded` 上；
- 原 manifest、layers、reports 和已验证 package 身份不得被 RIP 修改。

### 2.2 RIP 侧输入

- CLI 与 DLL 路径均为 UTF-8，调用约定和二进制实现由外部模块持有；
- 批量模式只扫描输入目录直接子文件，按文件名不区分大小写排序；
- 当前确认只接受 8bit、6 通道、contiguous、stripped TIFF；
- 资源目录必须含 4 个 matrix、`linear.csv` 和所选 ICC；
- 输出原始名为 `slice.N.tiff`，适配层负责与 manifest 层索引建立确定性映射。

## 3. 依赖与分层

推荐实现分层：

```text
contracts/
  RIP module/settings/result 的机器合同

src/rip_integration/
  Qt 无关的设置校验、模块清单解析、路径所有权、结果 DTO 与真实输出规则
  不依赖 slicer_core，不修改 slicer_module/worker

apps/slicer_ui_host_sim/
  HostRipSettingsPanel / HostRipJobController / HostMainWindowRip
  QProcess、UI、持久化和切片成功后的编排

scripts/
  模块打包、完整性测试、本地迁移验证
```

依赖方向为 `host -> rip_integration -> contracts`。`rip_integration` 不得反向依赖参考宿主，
也不得引入对 Stage 14 SPI 的新导出。首版不新增 vcpkg 依赖：宿主使用现有 Qt 5.15 `QProcess`，
真实 TIFF 检查复用项目已批准的 LibTIFF 4.7.1；RIP 子进程继续私有加载 4.1.0。

## 4. 预计新增和修改文件

优先新增：

```text
contracts/slicesoft.rip.module.1.schema.json
contracts/slicesoft.rip.settings.1.schema.json
contracts/slicesoft.rip.result.1.schema.json
src/rip_integration/RipModuleManifest.*
src/rip_integration/RipSettings.*
src/rip_integration/RipOutputValidator.*
apps/slicer_ui_host_sim/HostRipSettingsStore.*
apps/slicer_ui_host_sim/HostRipSettingsPanel.*
apps/slicer_ui_host_sim/HostRipJobController.*
apps/slicer_ui_host_sim/HostMainWindowRip.cpp
scripts/PackageRipModule.ps1
scripts/TestRipModulePackage.ps1
tests/ripflow/*
```

预计修改：

```text
CMakeLists.txt
apps/slicer_ui_host_sim/CMakeLists.txt
apps/slicer_ui_host_sim/HostMainWindow.h
apps/slicer_ui_host_sim/HostMainWindow.cpp
apps/slicer_ui_host_sim/HostMainWindowResult.cpp
scripts/PrepareSliceSoftRuntime.ps1
contracts/third_party_distribution_manifest.json（仅许可证闭合后）
```

当前工作树已有与 HOSTFLOW/场景变换相关的用户改动，其中 `HostMainWindow.h` 和文档索引存在重叠
风险。实施必须先重新读取并基于现状合并；不得回退这些改动。文档索引不属于本次准备落地范围。

## 5. 配置和持久化准备

独立配置根：

```json
{
  "schema": "slicesoft.rip.settings.1",
  "autoAfterSlice": false,
  "renderIntent": 0,
  "transparentMode": "follow_manifest",
  "colorMode": 0,
  "inputIcc": "CmykFiles/CIERGB.icc",
  "outputIcc": "CmykFiles/CMYK.icc",
  "continueOnError": false,
  "deviceGrayBits": 2,
  "timeoutSeconds": 3600,
  "outputDirectoryName": "rip",
  "existingOutputPolicy": "fail_closed"
}
```

保存到独立 QSettings group，并带独立 schema/version；不提升 `HostWorkspaceState` v6，不进入切片
Profile hash。未知 schema、缺字段、非法枚举、越界值和资源路径逃逸全部恢复为“自动关闭 + 配置
无效”，不得静默套用生产默认。

UI 建议为独立“RIP 设置”页，至少提供：自动开关、手动运行、渲染意图、白语义映射状态、颜色
模式、输入/输出 ICC、失败继续、运行时完整性、输出目录只读预览、状态/进度/取消/日志。未获定义
的 `colorMode != 0` 和未闭合的白语义组合应显示但禁用，而不是伪装成可用选项。

## 6. 作业、路径与发布准备

### 6.1 输入解析

由已验证 package 路径派生：

```text
input  = <package>/layers
stage  = <package>/.rip.staging.<jobId>
final  = <package>/rip
```

所有路径规范化为绝对路径后验证：package 身份与 Review 一致；input 必须是 package 的直接子目录
`layers`；stage/final 必须仍位于同一 package；不得接受符号链接、`..`、根目录或用户任意外部路径。

### 6.2 前置检查

在启动进程前检查：manifest schema/通道/位深/极性、`whiteSemantics`、
独立 RIP `deviceGrayBits` 输出期望、层列表、每层存在、8bit/6ch/contiguous/stripped、Grid、模块
manifest/hash/资源/ICC 完整。tiled、缺层、冲突白语义或 grayBits 缺失立即失败。

当前 S1 层 TIFF 不写 DPI 标签，输入 DPI 读取 Package grid；当前外置二进制真实输出固定为
600 x 600 DPI，因此非 600 x 600 Package 在启动前拒绝。S2 外部合同未来仍须由打印侧权威
`profile.device.grayBits` 闭合；本地设置不得被误解为打印 Profile 已回签。

### 6.3 子进程和取消

- `QProcess::setProgram` 与 `setArguments` 分离，程序固定绝对路径；
- 工作目录固定模块根；环境只在子进程范围加入模块目录；
- 合并到 UI 前保留 stdout/stderr 原始日志，并过滤展示中的敏感绝对路径；
- 退出 `0` 仅代表进入输出验证，不直接代表成功；退出 `1/2` 和启动失败映射为稳定错误码；
- 取消先 `terminate` 并给固定宽限，超时后 `kill`；Windows 子进程树残留必须进入测试矩阵；
- 任何路径不得通过 shell 展开。

### 6.4 输出验证与发布

逐层从真实 TIFF 检查并生成描述符：

```text
层数与 manifest 一致；索引连续且一一对应；
8bit；samplesPerPixel >= 7；planar contiguous；stripped；非 tiled；
宽高与输入层一致；输出 DPI/物理网格符合准入规则；
W/S/V 实际最小/最大不越 grayBits 上限；
文件可完整逐行读取且无额外未知层。
```

验证后把 `slice.N.tiff` 确定性改名为 `rip_%06d.tif`，写入 `rip_result.json`，再原子发布。
已有 final 时首版拒绝；失败或取消不得发布半成品。发布前后再次核对所有 `layers` hash 不变。

## 7. 自动后处理准备

自动链路固定为：

```text
slice succeeded
  -> package LoadAsync strict PASS
  -> read fresh RIP settings snapshot
  -> autoAfterSlice ? preflight : not_requested
  -> run/validate/publish
```

RIP 运行时禁用重复 RIP 提交，但不篡改切片终态。切片后结果加载失败时不自动 RIP。RIP 失败时
继续允许查看原切片结果，并在 UI 中分别显示切片和 RIP 状态。手动 RIP 必须复用同一控制器、
前置检查和发布器，禁止存在第二条弱校验路径。

## 8. 验收矩阵

| 维度 | 正例 | 负例/边界 |
|---|---|---|
| 触发 | 手动、自动开启 | 自动关闭零进程/零目录、重复提交 |
| 路径 | ASCII、空格、中文 | `..`、符号链接、外部目录、已有未知 `rip` |
| 运行时 | 全文件/hash 正确 | EXE/DLL/TIFF/matrix/CSV/ICC 缺失或篡改 |
| S1 输入 | stripped、grayBits 1/2、合法白语义 | tiled、坏 TIFF、缺层、manifest/Profile 冲突 |
| 进程 | exit 0、完整日志 | 启动失败、exit 1/2、崩溃、超时、取消、关闭窗口 |
| S2 输出 | 7ch、连续、stripped、层数/范围正确；固定 4 像素补齐可裁回 Package 原宽 | `slice.N` 缺口、坏层、tiled、非确定性尺寸差异、W/S/V 超限 |
| 发布 | staging -> `rip` 原子完成 | 半成品、已有结果、取消残留、跨卷失败 |
| 回归 | `layers`/manifest/hash 不变 | 自动关闭仍启动、RIP 失败破坏切片结果 |
| 迁移 | 整个 `modules/rip` 在隔离运行时自检 | 依赖宿主 PATH、加载宿主 tiff.dll、仓库绝对路径 |

至少覆盖既有实测异常：tiled 输入失败、95 像素扩为 96、`transparent=0` 时 W 可到 9，以及
`colorMode` 非 0 无权威语义。95 -> 96 只允许按受控规则裁回 95；其他异常不能只记 warning 后发布。

## 9. 原子开发顺序与并行边界

1. `RIPFLOW-A-01` 冻结 module/settings/result 合同；
2. A-01 完成后，A-02 运行时打包与 B-01 设置模型可由不同 agent 并行；
3. B-02/B-03 完成设置持久化和 UI；
4. C-01/C-02 完成唯一 QProcess 控制器、取消和日志；
5. C-03 完成前置检查、真实 TIFF Gate、命名归一化和原子发布；
6. C-04 只做切片 strict 加载成功后的自动接线；
7. D 组完成矩阵、迁移包和收口；E 组等待外部许可证/生产证据。

并行 agent 不得修改同一文件；`HostMainWindow.h`、宿主 CMake 和 Runtime 打包脚本分别只允许一个
owner。每张卡完成后停止并更新任务真源，不自动占用下一卡文件。

## 10. Gate 判定与停止条件

| Gate | 结论 | 依据 |
|---|---|---|
| 需求映射 | PASS | 四项产品要求均有原子卡与验收 |
| 架构/依赖 | PASS | QProcess 隔离；不改 SPI/Worker；无新第三方依赖 |
| 目录所有权 | PASS | `layers` 冻结，`rip` 独立，staging 有路径约束 |
| 配置/UI | PASS | 独立 schema、默认关闭、未知值 fail-closed |
| 验证 | PASS | 真实 TIFF、异常实测、取消和迁移矩阵已定义 |
| 外部分发 | BLOCKED_EXTERNAL | DLL/lcms2/ICC/私有 LibTIFF 来源与许可不完整 |
| 生产验收 | BLOCKED_EXTERNAL | 极性、白语义映射、目标打印软件和实物证据未闭合 |

开发中若需要修改 S1/S2、Stage 14 ABI/能力/Worker、允许 tiled 输入、猜测 `colormode`、在宿主
进程加载私有 TIFF、覆盖未知 `rip` 或跳过真实输出扫描，必须停止并新建受控修订。

## 11. Revision History

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-17 | v1.0 | 完成分层、文件所有权、配置、状态机、路径、真实 TIFF 验证、自动接缝、矩阵、并行边界和停止条件准备；Gate PASS，A-01 可开发 |
| 2026-08-17 | v1.1 | 补齐 `deviceGrayBits`/超时字段及验证属性，明确 600 x 600 本地子集与 S1 DPI 权威来源 |
| 2026-08-17 | v1.2 | 增补 4 像素对齐宽度归一化 Gate：只裁掉 RIP 在右侧新增的 1..3 列，发布 TIFF 仍须精确匹配 Package Grid |
