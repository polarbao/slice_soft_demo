# DOC PREP 13B-08-02 场景生产服务与 CLI 准备

> 文档状态：PREPARED / SEQUENCE WAIT 13B-08-01
> 版本：v1.0
> 日期：2026-07-28
> 对应任务：13B-08-02

## 1. 目标

建立无 Qt 的多模型场景生产服务和显式 CLI 路由，使冻结的
`scene_config.effective.json` 可以产生一个可严格读取的 RGBWSV Package。

## 2. 前置与输入合同

执行前必须满足：

```text
13B-08-01 批量导入和 SceneDocument 主入口 PASS；
13B-05..07 的场景编排、层合成和 Package writer 保持通过；
输入 schema/hash/revision 可回读验证；
每个 visible instance 可解析模型资源和 scene-wide Profile；
buildVolume、DPI、layerHeight、pipeline mode 均为显式值。
```

生产入口固定为：

```powershell
slicer_cli --scene-config <scene_config.effective.json>
```

不得通过 `--config` 猜测 schema，不得调用 `multi_model_scene_matrix` 充当产品服务。

## 3. 服务边界

新增 `MultiModelProductionService`，只依赖 core DTO 和 pipeline：

```text
Read/Validate Effective Config
-> Resolve visible instances and resources
-> Validate profile/build volume/pipeline mode
-> Generate per-instance local rasters
-> Compose admitted scene rasters
-> Write one production Package
-> Validate manifest/report identity
-> Return stable result/error.
```

Qt、QProcess、QImage 和界面字符串不得进入该服务。

## 4. 模式与失败策略

```text
Legacy：复用已准入的 adapter/orchestrator；
Global：只有多模型 production Gate 明确通过时才执行；
Global 未准入：SCENE_PIPELINE_MODE_NOT_ADMITTED；
任何资源、revision、碰撞、越界或 writer 错误：fail-closed；
不得自动删除已有输出目录之外的数据；
不得静默回退 Legacy。
```

输出错误至少覆盖：

```text
SCENE_EFFECTIVE_CONFIG_INVALID；
SCENE_EFFECTIVE_CONFIG_STALE；
SCENE_RESOURCE_UNRESOLVED；
SCENE_PROFILE_MISMATCH；
SCENE_BUILD_VOLUME_UNDEFINED；
SCENE_PIPELINE_MODE_NOT_ADMITTED；
SCENE_PRODUCTION_PACKAGE_INVALID。
```

## 5. 文件所有权

计划新增或修改：

```text
src/slicer_core/pipeline/MultiModelProductionService.h/.cpp；
apps/slicer_cli/main.cpp；
tests/slicer_core 或独立 service/CLI test；
根 CMakeLists.txt；
阶段报告。
```

不得修改 Qt 布局、生产 TIFF 协议或 OpenVDB 默认值。

## 6. TDD 与验收

先增加失败测试，再实现最小生产路径：

```text
有效三实例 scene config -> 一个 Package；
坏 schema/hash/revision -> 稳定失败；
无 visible instance -> 稳定失败；
缺 buildVolume/profile/resource -> 稳定失败；
Global 未准入 -> 阻断且无 Legacy 输出；
每层一个 TIFF、scene extension 和 reports identity 一致；
RIP strict PASS。
```

计划验证：

```powershell
cmake --build build --config Debug --target slicer_cli multi_model_production_service_test
ctest --test-dir build -C Debug -R "multi_model_production_service|scene_slice_route" --output-on-failure
.\build\Debug\rip_reader_test.exe --summary <package>
.\scripts\run_ci_quick.ps1
git diff --check
```

命令路径以实际 CMake target 输出为准，未运行不得记为 PASS。

## 7. 停止条件

若有效 scene config 不能确定 per-instance 生产 Profile、资源身份或输出目录，不得用隐式默认值猜测；
应先补齐 scene effective schema 的显式引用并增加迁移/负向测试。
