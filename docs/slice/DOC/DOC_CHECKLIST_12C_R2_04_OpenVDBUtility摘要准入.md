# DOC_CHECKLIST_12C-R2-04 OpenVDB Utility 摘要准入

> 文档状态：Readiness Complete / Stage 12C-R2-04
> 日期：2026-07-15
> 目标：冻结 OpenVDB SDF Utility 报告在 Qt 调试工作台中的只读加载、校验和安全展示契约

## 1. 准入结论

```text
12B-R2 utility report schema 与生成器：AVAILABLE；
OpenVDB ON/OFF 真实报告证据：AVAILABLE；
12C-R2-03 DiagnosticsDock：COMPLETE；
R2-04 展示位置、输入入口、中文语义和负向校验：FROZEN；
R2-04：READY TO IMPLEMENT。
```

R2-04 只扩展 `apps/slicer_debug_ui` 的报告读取与展示能力，不修改 OpenVDB 计算、Legacy 生产切片、RGBWSV TIFF、package schema 或 12D 材料闭环判断。

## 2. 当前代码与报告事实

| 证据 | 当前事实 | R2-04 用法 |
|---|---|---|
| `OpenVdbSdfUtilityReport.cpp` | 生成 `slicesoft.openvdb_sdf_utility.12b_r2.1` | 作为字段和枚举真源 |
| `DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md` | 固定 outputPolicy、utilities、decision 和 validation 约束 | 作为 UI 严格校验依据 |
| `output/benchmarks/12b_r2_openvdb_sdf_utility_off.json` | `useOpenVdb=false`、四项 utility unavailable | 作为本机真实 OFF 证据，不直接纳入 Git fixture |
| `output/benchmarks/12b_r2_openvdb_sdf_utility_on.json` | OpenVDB 12.0.1 可用，壳层/拓扑 utility pass | 作为本机真实 ON 证据，不等价于生产 PASS |
| `ReportPanel` / `ReportLoader` | 已支持 package/reports JSON 选择、摘要和原文 | 复用，不新增平行报告页 |
| `DiagnosticsDock` | 已承载唯一 ReportPanel | R2-04 摘要继续显示在“报告”页 |

`output/benchmarks` 是运行产物，不是稳定仓库 fixture。自动化测试必须在临时目录构造与当前 schema 对齐的 ON、OFF 和非法报告，避免依赖某台机器的 OpenVDB 安装状态。

## 3. 冻结范围

### 3.1 必须实现

```text
识别 slicesoft.openvdb_sdf_utility.12b_r2.1；
校验 schema、安全 outputPolicy、四项 utility 和 productionReplacementAllowed；
以中文显示 build 可用性、utility 状态、推进建议、blockers/issues 和 Legacy 保护状态；
无论 utility 是否 pass，都明确显示 productionReplacementAllowed=false；
支持 package/reports 自动发现和用户显式加载独立 JSON 报告；
对 OFF、ON、错误 schema 和非法生产替代标志提供确定性显示；
新增 openvdb-utility-summary smoke。
```

### 3.2 明确不做

```text
不运行 OpenVDB probe；
不把 benchmark JSON 复制进 production package；
不生成或修改 RGBWSV TIFF/preview；
不改变一键切片和 Legacy 默认生产路径；
不把 utility pass 显示为模型可打印或生产验收通过；
不实现 clearanceDistance 或 materialClosureAssist；
不提前实现 12D 材料闭环算法。
```

## 4. 输入与承载契约

输入入口固定为两种：

```text
1. PackageLoader 自动发现 <package>/reports/*.json；
2. ReportPanel 提供“加载诊断报告”入口，显式选择任意目录中的 JSON。
```

显式加载只把文件加入当前 ReportPanel 会话，不复制、不改写文件，也不修改当前 package。对话框取消时保持当前报告和摘要不变。

承载位置固定为：

```text
DiagnosticsDock -> 报告 -> ReportPanel
```

不新增第四个诊断页签，不在中央工作区恢复“报告”顶级页签。报告解释逻辑放在 `services`，`ReportPanel` 只负责文件选择和文本展示，`DiagnosticsDock` 继续只负责布局。

## 5. 严格校验契约

识别到 OpenVDB utility report family 后，至少校验：

```text
schema == slicesoft.openvdb_sdf_utility.12b_r2.1；
build、outputPolicy、utilities、decision、validation、issues 类型正确；
outputPolicy.writesProductionPackage == false；
outputPolicy.writesProductionTiff == false；
outputPolicy.modifiesLegacyOutput == false；
outputPolicy.protocolSchemaTouched == false；
decision.productionReplacementAllowed == false；
utilities 包含 outerVarnishShell、clearanceDistance、topologyDiagnostic、materialClosureAssist；
status 属于 pass/fail/unavailable/blocked/skipped/not_evaluated；
promoteDecision 属于 promote/keep_experimental/reject/not_evaluated；
available=false 时 executed 不能为 true；
build.openVdbAvailable=false 时 utility 不能为 pass。
```

错误报告必须显示“报告无效，禁止作为生产证据”，并列出字段路径。不得为了展示而宽松修复非法值。

错误 schema 分两类：

```text
schema 以 slicesoft.openvdb_sdf_utility 开头但版本不匹配：显示不支持的 Utility schema；
其他 schema：继续由 ReportLoader 使用对应报告摘要或通用摘要，不误判为 OpenVDB utility。
```

## 6. 中文展示语义

### 6.1 顶部固定安全摘要

每份有效 Utility 报告必须显示：

```text
报告角色：OpenVDB SDF 辅助工具/候选；
生产替代许可：否（productionReplacementAllowed=false）；
生产结论：仅 Utility 诊断，不代表生产切片通过；
默认生产路径：Legacy；
Legacy 保护：根据 outputPolicy 和 validation.legacyGuard 显示“未修改”及是否实际运行 guard。
```

不得把 `validation.legacyGuard.ran=false` 翻译成“已完成 Legacy 回退”。它只表示本报告没有运行 guard；Legacy 仍为默认生产路径来自 12C 产品冻结决策。

### 6.2 状态映射

| 原值 | 中文显示 |
|---|---|
| `pass` | Utility 验证通过（非生产） |
| `fail` | Utility 执行失败 |
| `unavailable` | 当前构建不可用 |
| `blocked` | 输入或准入阻断 |
| `skipped` | 已按任务边界跳过 |
| `not_evaluated` | 尚未评估 |
| `promote` | 建议推进为辅助 Utility |
| `keep_experimental` | 保持实验能力 |
| `reject` | 不建议继续推进 |

`available=false` 不是生产失败，而是当前构建或该 utility 不具备执行条件。`blockers` 按 utility 分组显示，`issues[].code` 作为报告级问题显示。旧 candidate/experimental report 的 `productionAdmission.blockerCodes` 继续使用现有兼容摘要，不与新 schema 字段混写。

## 7. 实现边界

建议新增 `OpenVdbUtilityReportInterpreter` 服务，负责：

```text
schema family 识别；
结构和安全字段校验；
中文状态映射；
形成摘要文本和错误列表。
```

`ReportLoader::summarize` 仅在识别到对应 family 时调用该服务。`ReportPanel` 新增显式加载入口和可测试的 `LoadReportPath`，文件对话框槽函数使用 `On` 前缀。新公共接口遵循 Doxygen、PascalCase、m_xxx 和 Allman 风格；不顺带重命名历史接口。

## 8. Smoke 契约

新增单一 case：

```text
openvdb-utility-summary
```

Smoke 在 `QTemporaryDir` 中生成四份 fixture：

| Fixture | 关键值 | 必须断言 |
|---|---|---|
| OFF valid | `openVdbAvailable=false`，四项 unavailable | 显示当前构建不可用、replacement=false、Legacy 默认路径 |
| ON valid | 壳层和拓扑 pass，另外两项 not_evaluated | pass 文案必须带“非生产”，显示 blockers/推进建议 |
| bad schema | utility family 的未知版本 | 显示不支持 schema，不落入有效摘要 |
| bad replacement | `productionReplacementAllowed=true` | 显示报告无效和要求值 false |

同时断言：

```text
ReportPanel 可通过 package/reports 自动发现有效报告；
ReportPanel::LoadReportPath 可加载独立 JSON；
摘要始终包含 productionReplacementAllowed=false 的安全要求；
不得出现不带“非生产/不形成生产验收结论”限定的生产成功结论；
加载报告不改变 PreviewWorkspace 当前 layerIndex。
```

## 9. 预计影响文件

```text
apps/slicer_debug_ui/services/OpenVdbUtilityReportInterpreter.h/.cpp
apps/slicer_debug_ui/services/ReportLoader.cpp
apps/slicer_debug_ui/widgets/ReportPanel.h/.cpp
apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp
apps/slicer_debug_ui/CMakeLists.txt
docs/slice/DEV/DEV_12C_Qt_UI配置预览工作台设计.md
docs/slice/DEMO/DEMO_12C_Qt_UI配置预览验证方案.md
docs/user_guides/QT_DEBUG_UI_操作手册.md
```

## 10. 实施验证命令

```powershell
cmake --build build-12c-ui --config Debug --target slicer_debug_ui
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case openvdb-utility-summary
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case diagnostics-collapse --package output\UiSmokeLayerPreview
ctest --test-dir build-12c-ui -C Debug --output-on-failure
git diff --check
```

R2-04 UI smoke 使用临时报告，不要求 OpenVDB ON 构建。真实 OpenVDB ON/OFF probe 仍由 `scripts/run_12b_r2_openvdb_sdf_utility.ps1` 独立验证。

## 11. 最终判断

R2-04 的真实字段、独立文件入口、展示位置、安全语义、负向行为和 smoke 已冻结，具备进入代码实施的条件。实现时必须保持 `productionReplacementAllowed=false` 是安全门禁，而不是普通展示字段。
