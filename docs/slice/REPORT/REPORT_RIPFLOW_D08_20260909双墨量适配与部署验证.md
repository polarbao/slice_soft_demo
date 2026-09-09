# RIPFLOW-D-08 双墨量适配与部署验证

日期：2026-09-09。状态：切片侧 COMPLETE / PASS，外部验收继续延期。
权威状态：`docs/codex_task/current/TASKS_RIPFLOW_切片后外置RIP集成专项任务清单.md`。

## 1. 实施边界

- 本次显式固定 `rip_module/source.json` 到 `rip_project/RIPDLL_20260909`，不扫描并自动启用最新日期目录。
- 整套 11 文件更新，包含 EXE、RipSlicer.dll、私有 tiff.dll、ICC、矩阵和线性化表。此次 CMYK.icc、linear.csv 也有变化，不允许只替换 EXE/DLL。
- 模块版本为 1.2.0，CLI 能力检查必须同时包含 transparent 0..4 与 ripmode 0|1；旧模块拒绝混用。
- 新设置默认正常 RIP（0）；已有 v1/v2 设置按用户授权迁移到第二档 3 倍墨量 RIP（1）。旧 follow_manifest 仍迁移颜色模式 0 并关闭自动运行。
- settings/result/diagnostic 新增 v3，记录实际 ripMode；旧 schema 原文件保留。核心命令、Package 作业、手动目录作业均显式传参。
- 单色/彩色仅替换原严格/诊断的 UI 名称。结构、层数、尺寸、源身份与 S2 墨滴门不变；彩色仍隔离至 rip_diagnostic，不能因更名获取 S2 发布资格。
- 供应方说明 0 为直通流程，不进行墨量控制、补光油、肤色特殊处理和白墨挂网；1 为完整流程。产品名称“3 倍墨量”不代表每个通道值简单乘 3。
- 本次不修改几何采样、p0.rgbwsv.2、通道顺序、DPI 判定、生产切片算法，不开展后续编译环境优化。

## 2. 实际验证

| 验证 | 结果 |
|---|---|
| Release 核心、宿主、RIP 单测与 runtime 目标构建 | PASS，最终构建退出码 0 |
| RIP 定向 CTest | 8/8 PASS，包含 UI、设置迁移、核心参数、合同和模块包检查 |
| 合同固定样例 | positive=8 / negative=13 PASS |
| 新版 CLI 两档墨量 x 五档颜色 | 10/10 PASS，每次 3/3 层 |
| 宿主 Package 彩色模式两档墨量 x 五档颜色 | 10/10 PASS；实际参数快照一致，源层哈希不变，无严格 rip 输出 |
| 已部署 Runtime 手动目录两档墨量 | 2/2 PASS，diagnostic_unvalidated |
| 已部署 Runtime 单色严格 Package 两档墨量 | 2/2 PASS，rip_result v3 |
| 真实结果 JSON Schema | 14/14 PASS，使用本地 schema registry 解析引用 |
| 取消 / 超时 / CLI exit1 / CLI exit2 | 4/4 PASS，无部分发布或遗留 staging |
| 默认 Runtime 部署复验 | 模块 11/11 文件与源哈希一致，宿主与本次构建产物一致 |

构建入口：`cmake --preset slicesoft-main`；成功后构建 `build-slicesoft/main` Release 的
`rip_integration_unit_tests ripflow_settings_unit_tests ripflow_safety_unit_tests ripflow_fake_cli slicesoft_runtime`。
定向回归：`ctest --test-dir build-slicesoft/main -C Release --output-on-failure --timeout 120 -R "^(rip_integration_unit_tests|ripflow_)"`。
部署：`scripts/PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly`。

测试输入为已有 3 层、16x16 的 RGB 测试 Package（不是完整生产模型），不足以覆盖复杂模型、特殊墨工艺或物理三倍墨量。
未运行全仓回归、目标打印软件验收、干净机或实物打印。

## 3. 本地证据

以下路径均相对仓库根目录：

- 输入：`output/ripflow/host_mode4_gate.2d424e74c6f6401c81ace3ed23c246e6/package`
- 原生 CLI：`output/ripflow/ink_mode_gate.1f9b890656b2410d9396063c609e051f`
- 宿主 10 档矩阵：`output/ripflow/ink_mode_matrix/1f7952f6c8e04536bf8580ff6bdc6eb1/matrix.json`
- 手动两档：`output/ripflow/manual_modes.d941c23bdad4411b9b4b10b51acd1be7`
- 严格正常：`output/ripflow/local_gate/case.6578a99446b04bdea61ffcc6170096ad/package/rip/rip_result.json`
- 严格三倍：`output/ripflow/local_gate/case.8c56ca87785b4be183ad571210dcab4e/package/rip/rip_result.json`
- 生命周期：`output/ripflow/lifecycle_gate/run.3a0ccae364f343c389e4324f695e0cd4`
- 部署：`runtime/slicesoft/Release`；模块内 `source_provenance.json` 与 `rip_module.json` 记录来源和哈希。

## 4. 验证过程异常与剩余风险

- 首次构建使用旧生成的 POST_BUILD 命令，仍从 rip_project 根目录打包，故失败。修正 CMake 日期源后重新 configure/generate，最终目标构建通过；未使用失败构建的测试结论。
- 新增模块包 CTest 首次因子进程中 Get-FileHash 不可用失败；脚本改用与打包脚本一致的 .NET 流式 SHA-256，完整复跑 8/8 通过。未放宽哈希校验。
- 默认 Runtime 只部署 qwindows.dll，首次误用 offscreen 测试环境阻塞启动；只终止本次自测进程，改为 windows 平台后通过。第一次手动用例因未创建输出父目录被正确拒绝，创建独立父目录后复验通过。
- 部署时目录整体重命名受占用影响，既有脚本采用保留 output 的原位 payload 更新，返回成功；随后对部署的全部 RIP 文件和宿主做哈希复验，并在部署目录实际运行两档手动和严格作业。未强制终止用户进程。
- 真实 RIP 仍有 Photometric/ExtraSamples 警告，不影响本次输出结构验证，但不能宣称供应方 TIFF 元数据警告已修复。
- 历史构建目录、跟踪设置与编译耗时问题留待用户要求的后续编译专项；外部分发和生产打印验收状态不升级。
