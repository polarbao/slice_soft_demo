# TASKS 12E-09A 诊断 UI 任务清单

> 状态：09A-01..05 COMPLETE / 09A-06 READY
> 日期：2026-07-29
> 性质：独立 diagnostic UI 支线

前置执行合同：

```text
docs/slice/PRD/PRD_12E_09A_SceneAware诊断UI.md；
docs/slice/DEV/DEV_12E_09A_SceneAware诊断UI设计.md；
docs/slice/DEMO/DEMO_12E_09A_SceneAware诊断UI验证方案.md；
docs/slice/DOC/DOC_PREP_12E_09A_02_SceneAwareEffectiveConfig准备.md；
docs/slice/DOC/DOC_PREP_13B_01_MultiModelScene与EffectiveConfig准备.md；
docs/slice/DOC/DOC_PREP_13C_01_TIFFLayerSource与Cache准备.md。
```

## 1. 固定边界

```text
09A 只处理当前模型的诊断配置、分析状态和同层语义预览；
09A 不开放 Global 生产写包；
09A 不替代 09B 的产品模式/Profile/准入和生产 package 绑定；
诊断失败、未评估和 blocked 不得显示为 production PASS；
OpenVDB 仍是可选后端能力，不是第三种产品模式。
```

## 2. 09A-01 只读 Diagnostic Facade 与 UI DTO

状态：COMPLETE

成果：

```text
只读消费 slicesoft.texture_fill_partition.12e.1；
支持 pending/unavailable/blocked/diagnostic；
使用 optional 保留未评估值；
不接收输出目录，不写 package/TIFF；
拒绝把 release matrix 冒充当前模型诊断结果。
```

状态报告：

```text
docs/slice/REPORT/REPORT_12E_09A_01_只读DiagnosticFacade与UIDTO当前状态.md
```

## 3. 09A-02 Diagnostic Effective Config

状态：COMPLETE（2026-07-27）

目标：

```text
按 Config Editor 事务保存 texture width、modelFill.material、来源 Profile 和派生阈值；
生成 output/ui_sessions/<session>/slice_config.diagnostic.effective.json；
不覆盖 samples/configs fixture；
requested、derived、effective 字段可审计。
subjectType 兼容 single_model/scene；
scene 模式绑定 sceneId/instanceId/sceneRevision/transformRevision。
```

验收：保存、回读、事务回退、stale、取消、完整性和负向配置单测 PASS。

状态报告：

```text
docs/slice/REPORT/REPORT_12E_09A_02_SceneAwareDiagnosticEffectiveConfig当前状态.md
```

## 4. 09A-03 中文参数控件与状态区

状态：COMPLETE（2026-07-29）

目标：

```text
纹理表面层宽度 QDoubleSpinBox + slider；
单位 mm，步长 0.01 mm，显示 2 位；
模型填充材料选择；
显示最小值、最大值、全纹理阈值、阻断原因和 backend 可用状态；
所有中文控件提供简短 tooltip。
```

验收：双向同步、最长中文、三窗口尺寸和不可用状态 smoke。

状态报告：

```text
docs/slice/REPORT/REPORT_12E_09A_03_中文参数控件与状态区当前状态.md
```

## 5. 09A-04 异步分析 Worker

状态：COMPLETE（2026-07-29）

原子准备与执行指令：

```text
docs/slice/DOC/DOC_PREP_12E_09A_04_异步分析Worker准备.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_04_异步分析Worker执行指令.md
```

目标：

```text
topology/distance/width sweep/texture transfer/raster mapping 不阻塞 UI；
支持取消、关闭窗口、重复运行和模型切换；
结果绑定 session/model/config identity；
不复用 stale result，不悬挂 QObject。
```

验收：取消、关闭、重入、失败和成功生命周期测试。

状态报告：

```text
docs/slice/REPORT/REPORT_12E_09A_04_异步分析Worker当前状态.md
```

## 6. 09A-05 同层语义 Preview

状态：COMPLETE（2026-07-29）

原子准备与执行指令：

```text
docs/slice/DOC/DOC_PREP_12E_09A_05_同层语义Preview准备.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_05_同层语义Preview执行指令.md
```

目标：

```text
Texture Surface、Model Fill、Partition、Support、Varnish 使用同一真实 layerIndex/zMm；
显示 width、coverage、allTexture；
fullClosureLinkage 缺失时显示未评估；
不得按 preview 文件序号跨层兜底。
```

验收：同层 identity、物理坐标映射、材料分区、空层、缺失证据和真实模型 smoke。

状态报告：

```text
docs/slice/REPORT/REPORT_12E_09A_05_同层语义Preview当前状态.md
```

## 7. 09A-06 阶段收口

状态：READY / 09A-05 COMPLETE

原子准备与执行指令：

```text
docs/slice/DOC/DOC_PREP_12E_09A_06_诊断UI阶段收口准备.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_06_诊断UI阶段收口执行指令.md
```

目标：

```text
Qt self-test；
1280x720、1440x900、1920x1080 smoke；
最长中文、取消、失败和重复运行；
默认 OpenVDB OFF regression；
用户手册、状态报告、索引和上下文更新。
```

## 8. 开发序列判断

09A 必须保留在正式开发序列中，但不是 09B-01..06 的字母顺序前置。单贡献者推荐顺序：

```text
12E-09B-01..06 COMPLETE
  -> 12E-09C COMPLETE
  -> 13A-01 + 13B-01
  -> scene-aware 12E-09A-02 COMPLETE
  -> 13A-02..05
  -> 13B-02..07
  -> 13C-01..03
  -> 12E-09A-03..06
  -> 12E-10A..D
```

原因：

```text
09B 先冻结产品模式、Profile 和生产 session 合同；
09C 再冻结最终 X/Y raster 尺寸与 Reader 合同；
09A 同层 preview 在最终生产模式和 DPI 合同上收口，可减少重复验证；
13B-01 先冻结 scene identity，09A-02 已据此兼容 single_model/scene；
13C-03 先建立 TIFF 原生底图，避免 09A-05 复制旧 preview PNG 合成路线；
12E-10A 明确依赖 09A-05 和 09B-05，因此 09A 不能从路线中删除。
```

若使用独立分支并确保文件所有权不冲突，09A-02..04 可与 09B 后半段并行；当前单工作树不建议交叉实施。

## 9. 每任务验证

```powershell
git branch --show-current
git status --short
git diff --check
```

C++/Qt 修改按 `.agents/docs/build-and-test.md` 运行定向单测、UI build 和 self-test。
