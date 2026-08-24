# REPORT_RIPFLOW 外置 RIP 本地候选当前状态

> 日期：2026-08-18
> 结论：**SLICER_SIDE_COMPLETE / EXTERNAL_VALIDATION_DEFERRED**
> 禁止外推：不是 `EXTERNAL_ACCEPTED`，不是 `PRODUCTION_READY`

## 1. 当前实现

- `src/rip_integration` 提供无 Qt 设置/命令、真实 S1/S2 TIFF、命名和原子发布规则；
- 参考宿主增加独立 RIP 设置页、QSettings v1、手动入口、自动开关、取消、超时和双终态；
- 产品执行使用 `QProcess::setProgram/setArguments`，不经过 BAT、cmd 或 PowerShell；
- 应用相对发现 `modules/rip`，私有 `tiff.dll` 与宿主 LibTIFF 4.7.1 进程隔离；
- `layers` 不变；输出先进入 `.rip.staging.<id>`，真实校验后发布同级 `rip`；
- 输出验证默认 `strict_s2`；显式诊断模式只发布不可打印的同级 `rip_diagnostic`，不占用 `rip`；
- RIP 固定的 4 像素右侧补齐只在确定匹配时于 staging 裁回 Package 原宽，最终 Grid 不变；
- DPI 不参与 RIP 前置或输出发布 Gate；宿主不再把外部二进制写出的 600 x 600 标签解释为输入限制；
- S1/S2 解码与逐文件身份检查在后台线程执行，验证期可取消；发布前强制复验 manifest 与全部输入层；
- staging 清理要求同父目录、固定前缀且全树无 junction/reparse point，拒绝不安全递归删除并保留现场；
- 自动开关默认关闭，且只接在 `HostPackageReviewController::LoadAsync` 成功回调之后。

## 2. 本地证据

2026-08-17 实际通过：

| Gate | 实际结果 |
|---|---|
| 四份 JSON 合同 | `RIPFLOW_CONTRACTS_PASS positive=4 negative=5`，新增 `slicesoft.rip.diagnostic.1` |
| C++/Qt 定向测试 | Debug 与 Release 各 12/12 CTest PASS：core、settings、安全清理、模块/UI、自检、wiring 与 HostFlow 回归 |
| 真实 RIP 正例 | 20/20 层，600 x 600、透明、grayBits=2；真实 7 通道 TIFF 校验并发布 PASS |
| 中文/空格路径 | 同一 20 层矩阵 PASS |
| 生命周期 | cancel、timeout、exit 1、exit 2 均无 `rip`/staging 残留 PASS |
| fail-closed | follow_manifest 缺权威字段、opaque grayBits=2 W 超限、grayBits=1 超限、tiled/坏布局均拒绝 |
| 输入身份 | 控制器发布前重验 manifest 与 20 个 `layers` 的 canonical path/size/SHA-256；正例不变 |
| 宽度归一化修复 | 用户样例确认 `1842 x 623 -> 1844 x 623`；前 30 层真实 RIP 全部裁回 1842 并发布 PASS；95 -> 96 LZW 回归 PASS，97 -> 95 仍拒绝；Release Runtime 已部署 |
| DPI 限制剔除 | 原始 `rip_project` 处理 635x600 Package 成功；非 600 与缺失 DPI 标签单测 PASS；Release Hostx64 构建、RIP 6/6 CTest 及真实 635x600 Package 取消作业 PASS |
| 175 层诊断保存 | 真实 RIP `exitCode=0`，175/175 层在 42,955 ms 保存到 `rip_diagnostic`；`rip` 未生成；W/S/V 均为 `0..255`，各 10,875,980 个样本超过当前 grayBits=2 参考上限 |
| TIFF 运行库复核 | 下载目录与 `rip_project/modules/rip` 的私有 `tiff.dll` 均为 LibTIFF 4.1.0 且 SHA-256 相同；宿主 LibTIFF 4.7.1 保持进程隔离，未以替换 DLL 绕过合同 |

最终收口复验的合同门禁、Release Hostx64 构建和 RIP 定向 CTest 6/6 均 PASS。默认 Runtime 的
再次部署被部署脚本拒绝，因为用户当前仍在运行该目录中的 `slicer_ui_host_sim.exe`（PID 8244）；
未强制终止该进程。磁盘中的已部署版本包含诊断功能，关闭进程后仍需同步最新自测断言构建。

定向构建使用 VS 2026 x64 编译器、Qt 5.15.2、Debug Ninja、LibTIFF 4.7.1，目标
`rip_integration_unit_tests`、`ripflow_settings_unit_tests`、`ripflow_safety_unit_tests`、
`slicer_ui_host_sim` 和测试用 fake CLI
均构建通过。另在带中文/空格的全新目录复制宿主、Qt 和整个 `modules/rip`，相对发现、自检与
真实 20 层作业 PASS，且私有 `tiff.dll` 未进入宿主根。该证据是本机隔离迁移，不是外部干净机或
正式外部 Runtime 验收。随后使用 VS 2026 Hostx64 完成正式 `Release/slicesoft_runtime` 构建，
并完成 Release 12/12 定向 CTest；该本机构建证据仍不替代外部干净机和目标打印软件验收。

## 3. 支持边界

当前可发布的只是 `explicit_transparent + colorMode=0 + deviceGrayBits=2 + stripped`
本地候选。当前 CLI 没有 grayBits 参数，grayBits 设置只做输出准入。DPI 不是 CLI 或 DLL 的
输入参数；二进制写入的 600 x 600 TIFF 标签保留为外部元数据，不再限制 Package DPI。
`follow_manifest` 等待切片包权威 `whiteSemantics`，opaque/grayBits=1 在现有实测中因 W/S/V 上限
失败。这些失败是严格 `rip` 的预期保护，不得绕过检查发布。操作员可显式选择
`diagnostic_unvalidated` 保存结构合法的结果，但该目录固定为 `rip_diagnostic` 且不可打印。

非 4 对齐 Package 宽度可使用受控适配：只接受 RIP 输出宽度等于向上 4 像素对齐值、高度不变，
并在发布前裁回 Package 原宽。用户本次 Package 的前 30 层已完成 30/30 真实发布，输出均为
`1842 x 623`；完整 175 层继续扫描发现第 30 层 `W=255`，违反 grayBits=2 的 W<=6 冻结上限，因此仍不会发布
`rip`。诊断模式已经证明 RIP 本身可正常结束并产生 175/175 层：W/S/V 三个通道均出现 255，且
各有 10,875,980 个样本超过当前参考上限。该证据把问题定位到 S2 对供应方输出的量化/极性解释，
但仍不能证明 255 应映射成何种设备滴数，不能由切片侧静默截断、反相或解释为 0 滴。

## 4. 外部阻塞

1. RipSlicer/CLI、lcms2、ICC、私有 LibTIFF 的来源、许可证、SBOM 与再分发授权；
2. `transparent`/`whiteSemantics`、`colormode`、W/S/V 极性和设备 grayBits 的打印侧书面合同；
3. 目标打印软件、ChannelSplitter、干净机、实物打印、长稳和错误恢复证据。

在以上输入闭合前，自动 RIP 保持默认关闭，模块包保持 `LOCAL_ENGINEERING_ONLY`。
