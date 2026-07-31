# DOC_PREP_12E-09D 生产纹理厚度与单材料材质收口准备

> 文档状态：PREPARATION COMPLETE / WAIT 03D PRIORITY
> 日期：2026-07-31

## 1. 依赖

| 依赖 | 状态 | 说明 |
|---|---|---|
| 12E-09A | COMPLETE | 诊断控件和异步分析可复用但不可改成生产入口 |
| 12E-09B | COMPLETE | Production Effective Config 和一键切片入口可扩展 |
| 12E-09C | COMPLETE | X/Y DPI、物理宽度和 Preview 比例已收口 |
| 13C | COMPLETE | TIFF 原生生产预览可用于验收 |
| 13D/13E | COMPLETE | 右侧检查器和 UI 信息架构已存在 |
| 03D-LIBTIFF | P0 NEXT | 非技术硬依赖，但按用户优先级必须先执行 |
| 12G-TCWS | FROZEN | 不纳入 09D |

## 2. 已确认代码证据

```text
诊断宽度只写 m_diagnosticTextureSurfaceWidthMm；
状态提示明确“不修改生产 Profile”；
Legacy 使用 texture.topSurfaceLayers；
Global 使用 texture.surfaceShell.widthMm；
单材料白墨/光油样例真实通道分别为 W/V；
通用 modelFill.material 不能独立完成 W/V 切换。
```

## 3. 原子任务顺序

```text
09D-01：冻结 DTO、错误码和配置映射；
09D-02：ProductionTextureSettingsModel；
09D-03：SingleMaterialReliefResolver 与核心校验；
09D-04：Qt 条件化控件、stale 和保存/回读；
09D-05：一键切片、报告、UI Smoke；
09D-06：Legacy/Global/W/V Release 矩阵与阶段收口。
```

## 4. 文件所有权

预计涉及：

```text
apps/slicer_debug_ui/services；
apps/slicer_debug_ui/widgets/ContextInspector/SliceSettings；
EffectiveConfigGenerator/ConfigValidator；
src/slicer_core/config；
material process/profile resolver；
reports；
tests/unit；
scripts；
09D 状态文档和用户指南。
```

不得修改：

```text
tiff_io Writer（归 03D）；
12G resolver/composer；
OpenVDB admission；
support/baseProjection；
p0.rgbwsv.2。
```

## 5. 启动 Gate

```text
文档准备：PASS；
需求/设计/验证：PASS；
任务清单/Prompt：PASS；
03D 第一优先级：WAIT；
09D 代码授权：NOT YET REQUESTED。
```
