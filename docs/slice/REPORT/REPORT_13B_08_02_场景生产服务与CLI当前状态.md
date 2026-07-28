# REPORT 13B-08-02 场景生产服务与 CLI 当前状态

> 状态：COMPLETE / GATE PASS
> 日期：2026-07-28
> 下一入口：13B-08-03 Qt 当前场景主切片动作

## 1. 任务结论

13B-08-02 已完成无 Qt 的多模型场景生产服务和显式 CLI 入口。服务只消费冻结后的
`scene_config.effective.json`，对场景身份、Profile、设备幅面、切片模式和模型资源执行
fail-closed 校验，然后复用既有 Legacy 单实例 Raster、场景合成器和单 Package writer，
生成一个可由严格 RIP Reader 读取的 RGBWSV Package。

本任务没有把 `multi_model_scene_matrix` 变成产品入口，也没有把 Qt 引入
`slicer_core`。Global 多模型生产仍未准入，不能静默回退到 Legacy。

## 2. 已实现能力

### 2.1 场景生产服务

新增 `MultiModelProductionService`：

```text
读取 scene effective config；
验证 sceneId / sceneRevision / sceneHash / effectiveConfigHash；
解析显式 Profile 配置路径和输出 Package 路径；
校验 buildVolume、DPI X/Y、层厚、Profile 身份和切片模式；
按可见实例加载 OBJ/3MF 及相邻资源并校验 source/resource hash；
对全部实例保留碰撞准入证据，隐藏实例不参与生产 Raster；
通过 LegacySceneLayerAdapter 为各可见实例生成内存 Raster；
通过 ComposeAdmittedSceneRasters 合成全局层；
通过 WriteMultiModelSceneProductionPackage 只发布一个 Package；
发布后执行严格 RGBWSV 协议和场景身份复核。
```

### 2.2 CLI

新增显式入口：

```powershell
.\build\Debug\slicer_cli.exe --scene-config <scene_config.effective.json>
```

`--scene-config` 不允许与单模型 `--config`、OpenVDB 实验入口或其他单模型诊断参数混用。
成功时输出 Package 路径、场景身份、可见实例数和层数；合同错误返回退出码 `2`。

### 2.3 稳定错误合同

```text
SCENE_EFFECTIVE_CONFIG_INVALID
SCENE_EFFECTIVE_CONFIG_STALE
SCENE_RESOURCE_UNRESOLVED
SCENE_PROFILE_MISMATCH
SCENE_BUILD_VOLUME_UNDEFINED
SCENE_PIPELINE_MODE_NOT_ADMITTED
SCENE_PRODUCTION_PACKAGE_INVALID
```

### 2.4 输入覆盖

`SliceRunOptions` 增加显式模型输入覆盖，使一个场景级 Profile 可用于多个模型资源，
无需为每个实例生成临时单模型配置文件。该覆盖只由场景适配器使用，既有单模型 CLI
行为保持不变。

## 3. 输出与固定协议

生产输出继续遵守：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
一个场景一个 Package
每个 layer 一个 TIFF
```

`manifest.json` 和 `reports/multimodel_scene_report.json` 均携带一致的场景身份。
隐藏实例保留在报告统计中，但不生成打印 Raster。

## 4. 测试覆盖

新增核心和 CLI 正负向验证：

```text
三个可见实例合成为一个严格 Package；
两个可见实例加一个隐藏实例，报告保持 2/1 计数；
Profile 路径缺失；
场景无可见实例；
buildVolume 未定义；
Profile 身份、DPI 或层厚不匹配；
Global 多模型模式拒绝且不回退；
CLI 正向 --scene-config；
CLI 路径缺失和参数混用负向。
```

本轮实测：

```text
Debug 全量构建：PASS
Debug CTest：80/80 PASS
UI --self-test：PASS
scripts/run_ci_quick.ps1：PASS
git diff --check：PASS
```

## 5. 当前边界

```text
仅 Legacy 场景生产已接通；
Global 多模型生产未准入；
UI 尚未生成完整 production effective config，也尚未调用 --scene-config；
设备 buildVolume/origin/axes 仍是生产输入开放项；
22 实例真实性能预算尚未关闭；
不支持 mixed-profile、自动 nesting、跨模型联合支撑；
当前结论是 functional PASS，不是设备 production GO。
```

## 6. 下一步

13B-08-03 可解除顺序等待。下一任务应在 Qt 层新增“切片当前场景”状态机，冻结
SceneDocument 快照并写出带显式 Profile、输出路径和设备幅面的 effective config，
调用本任务的 `--scene-config`，完成成功 Package 自动回载、取消、stale 和 no-fallback
UI Smoke。不得恢复旧单模型按钮来冒充场景切片。
