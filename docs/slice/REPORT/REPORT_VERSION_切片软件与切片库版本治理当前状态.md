# REPORT_VERSION 切片软件与切片库版本治理当前状态

> 日期：2026-08-17
> 状态：`FUNCTION_COMPLETE / REGRESSION_PARTIAL`
> 任务清单：`docs/codex_task/current/TASKS_VERSION_切片软件与切片库版本治理任务清单.md`

## 1. 当前结论

版本治理功能已完成：软件和切片库以根 `version-manifest.json` 为唯一实现版本事实源，
当前均为 `0.2.0-dev`，并已贯通 CMake、生成头、模块自述、Worker、Qt/CLI 查询、Windows
VERSIONINFO、模块包和 Runtime 清单。

当前工作区不是稳定发布输入。已验证的 Release 完整构建标识为：

```text
0.2.0-dev+24731b11292c.dirty.release.msvc-x64-md.x64-windows.tiff-libtiff.openvdb-off
```

其中 `dirty` 是真实源码状态，不得将该制品描述成 `0.2.0` 稳定版。稳定发布仍必须满足 clean
source、annotated `vX.Y.Z` tag 与 revision 一致；打包和 Runtime 脚本已对该条件 fail-closed。

## 2. 已实现范围

- 建立 `slicesoft-app` 与 `slicer` 双组件、当前 lockstep 发布的 SemVer 事实源；
- 严格校验组件 ID、release status、SemVer、lockstep、兼容合同和 vcpkg 版本；
- 按 Debug/Release 隔离生成 source state、完整构建标识、版本头和 build manifest；
- `pm_module_info.version`、`module.json.version` 和 Worker discovery 统一派生，未新增 SPI 导出；
- Qt 标题和常驻状态显示软件/切片库/SPI，诊断显示完整构建标识；缺模块显示不可用；
- UI/CLI `--version` 在初始化、创建窗口和切片前短路；
- UI、Module、Worker、CLI 写入一致的 Windows VERSIONINFO；
- 模块包、Runtime 清单、校验和、二进制属性与运行时查询执行交叉一致性校验；
- Visual Studio 生成器显式使用 x64 host 工具，关闭不稳定的文件跟踪并 clean-first，匹配封装版
  Release 调试配置。

冻结边界保持不变：`PM_SPI_VERSION=1`、精确 11 导出、`file_contract_v1`、
`p0.rgbwsv.2`、RGBWSV 顺序/位深/极性和 Module Info/Manifest `.1` 结构均未修改。

## 3. 构建与验证

| 验证项 | 结果 |
|---|---|
| Ninja Debug `slicesoft_runtime` 构建 | PASS |
| Ninja Release `slicesoft_runtime` 构建 | PASS |
| Visual Studio Release 当前 HEAD clean build + Runtime deploy | PASS |
| 最新源码 MSVC/Ninja Release staging Runtime | PASS |
| Debug VERSION/Stage 14 定向功能集 | 10/10 PASS |
| Release VERSION/Stage 14 定向功能集 | 10/10 PASS |
| Debug/Release 完整定向集 | 各 10/11；同一既存边界测试失败 |
| Module Package Gate | `STAGE14F01_PACKAGE_PASS` |
| Module Package Validation Gate | `STAGE14F01_PACKAGE_VALIDATION_PASS` |
| Runtime staging 四制品 VERSIONINFO、Worker/Module 内部版本 Gate | PASS |
| PowerShell 脚本解析、Python `py_compile` | PASS |

定向功能集覆盖 Module Info/Manifest、Worker contract、精确 11 导出、版本正负例 fixture、
Qt 默认模块、缺模块、UI smoke 与 Hostflow 导入 UI smoke。Debug 和 Release 均再次执行并取得
10/10 PASS。

与引用图片红框环境对应的实际验证命令为：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/PrepareSliceSoftRuntime.ps1 `
  -BuildDir build-slicesoft/main `
  -RuntimeDir output/version-validation/redbox-runtime-current `
  -Config Release -BuildSystem VisualStudio -Jobs 2
```

该命令完成 clean build 和全部 Runtime staging 自检；Windows 对 staging 目录的一次瞬时占用令
最终原子重命名失败，等待 5 秒后对同一 build-tree 执行 `-DeployOnly` 成功完成部署。当前可运行
制品位于：

```text
output/version-validation/redbox-runtime-current-2/Release/slicer_ui_host_sim.exe
output/version-validation/staging-check-runtime-4/Release/slicer_ui_host_sim.exe
```

第一处为引用图片红框对应的 Visual Studio Release 生成物，第二处为独立 MSVC/Ninja 对照。
两处均返回本报告上方的 `24731b11292c.dirty`；短版本和 VERSIONINFO 均为受控的
`0.2.0-dev`。

中途一次 Visual Studio clean build 恰逢并发 RIPFLOW 提交，部署脚本按输入指纹 fail-closed，
拒绝发布可能混合的 build-tree。并发提交完成后已重新 clean build、部署和自检 PASS；该过程
没有覆盖并发修改。

## 4. 未收口项

`slicer_stage14e02_qt_host_boundary_test` 当前失败：已在现有 HEAD 中的
`apps/slicer_ui_host_sim/HostModelImportWorkflow.cpp` 为 516 行，超过历史门槛 500 行。该文件
不是 VERSION 专项修改文件，其他 10 个 VERSION/Stage 14 定向测试均通过，因此不通过放宽
门槛或夹带宿主重构掩盖此问题。VERSION-06 保持
`PARTIAL / BLOCKED_BY_EXISTING_14E02_BOUNDARY`，待该既存边界单独修复后复跑收口。

本专项未执行打印侧、目标 RIP、洁净机或物理打印验收，不将其记为 PASS。

## 5. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-17 | v1.0 | 记录 VERSION-00..05 完成、VERSION-06 定向回归结果、封装版 Release 构建证据和既存阻塞。 |
