# DEMO 12E-09B Qt 双模式生产入口验证方案

> 状态：READY
> 日期：2026-07-23

## 1. 验证目标

证明 Qt UI 可以显式、可审计地运行 Legacy 或已准入 Global Profile，并保持统一 TIFF/package/RIP 合同。

## 2. 固定用例

正向模型：

```text
model/obj/xiao_ma_wu_yu_new 中当前 R4/08D 固定 baseline；
model/obj/yecan/3.obj；
samples/models/3mf/texture2d_checker_cube.3mf 仅作 3MF Texture2D 正向控制。
```

阻断模型：

```text
aishen_fudiao；
meigui_fudiao；
titian_fudiao。
```

阻断模型用于验证 strict fail-closed，不计入 Global production PASS。

## 3. 用例矩阵

| Case | 模式/Profile | 预期 |
|---|---|---|
| Legacy default | legacy | 一键切片成功；历史能力保留 |
| Global restricted | restricted candidate | RGB+W package、RIP strict PASS；S/V 控件锁定 |
| Global parity | material parity candidate | RGB+W+lower/internal-void S+surface/outer V PASS |
| Global topology blocked | 任一阻断模型 | writer 不启动；无 package 伪成功；无 fallback |
| 非法 mode/Profile | 负向配置 | 稳定错误；不启动 |
| 切换模式后重跑 | Legacy -> Global -> Legacy | 每次 session 独立；requested/effective 匹配 |
| 进程失败后重试 | 先失败后成功 | 不加载旧 package；新结果身份正确 |

## 4. 数据断言

每个生产成功用例：

```text
manifest schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
requestedPipelineMode=effectivePipelineMode；
productionOutputWritten=true；
fallbackApplied=false；
TIFF layer list 连续；
RIP Reader strict PASS；
preview/report 与真实 layerIndex/zMm 一致。
```

## 5. UI 断言

```text
默认显示“传统切片”；
Global 必须显式选择；
普通页面不出现 OpenVDB backend 选择器；
不支持项禁用且 tooltip 说明原因；
Global 显示资源开销提示；
实际耗时来自当前运行，不使用固定倍数冒充；
1280x720、1440x900、1920x1080 无遮挡和文本截断。
```

## 6. 建议验证命令

原子任务按实际 target 选择定向命令，最终至少运行：

```powershell
cmake --build build --config Debug --target slicer_debug_ui slicer_cli rip_reader_test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
cmake --build build --config Release --target slicer_debug_ui slicer_cli rip_reader_test
.\scripts\run_12e_08d_06_release_matrix.ps1 -BuildDir build -Config Release -SkipBuild
```

若 UI executable 的实际输出目录不同，以当前 CMake/runtime 文档为准，不虚构已运行结果。

## 7. 失败判定

以下任一项为 09B NO-GO：

```text
Global 自动 fallback 到 Legacy；
Global 成功但缺 TIFF 或 RIP strict 失败；
requested/effective mode 不一致；
不支持控件值进入 Effective Config；
阻断模型仍启动 writer；
失败后加载旧 package；
Legacy 默认行为回归；
UI 把 OpenVDB 当成第三种产品模式。
```
