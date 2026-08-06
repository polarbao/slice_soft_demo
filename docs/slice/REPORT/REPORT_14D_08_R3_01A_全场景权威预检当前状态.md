# REPORT_14D-08-R3-01A 全场景权威预检当前状态

> 日期：2026-08-06
>
> 状态：`COMPLETE`
>
> 父任务：`14D-08-R3`
>
> 下一关键路径：`14D-08-R3-01B -> 14D-08-R2-02`

## 1. 完成结论

已实现 Qt-free、Worker-free 的全场景权威预检服务。服务以 committed scene 的 hash、revision、
绝对资源身份和显式 `targetMode` 为输入，对可见实例完成源模型与变换后几何审计，再统一执行构建体积、
越界和精确投影碰撞检查。

本任务只提供 engine 内部服务，不注册 Worker executor、不修改公开 SPI、不生成 Package/TIFF。
`geometry.preflight.full` 的能力 DTO 和 Worker 请求适配仍由 `14D-08-R3-01B` 完成。

## 2. 代码交付

| 文件/目标 | 内容 |
|---|---|
| `SceneFullPreflightService.*` | scene 身份、稳定排序、逐实例 topology/admission 和空间准入聚合 |
| `SceneFullPreflightResourceResolver.cpp` | 绝对 scope、源文件 SHA-256、相邻资源 hash 和不可变模型解析 |
| `SceneFullPreflightServiceTests.cpp` | 通过、隐藏、碰撞、越界、资源、stale、模式、预算和取消用例 |
| `stage14d08_r3_scene_preflight_tests` | Debug/Release 定向 CTest 入口，链接 `slicer_engine` |

服务保证同一模型在单次 scene 运行中只解析一次；同源多实例仍分别保留 transform revision/hash 和
变换后审计证据。隐藏实例只计入 skipped，不参与 topology、碰撞或生产阻断。

## 3. 准入语义

- `Legacy` 与 `GlobalSurfaceShell` 由请求显式选择，禁止从 Profile 名称猜测；
- topology warning 可在 Legacy 中继续，但 Global blocker 保持阻断；
- 碰撞或越界属于完整、权威的业务阻断：`authoritative=true`、`productionAdmitted=false`；
- 资源缺失/hash 变化、stale、预算未完成、取消属于证据不完整：两项均为 false；
- 输出实例、issue、碰撞对和越界实例均采用稳定排序。

## 4. 验证证据

实际运行并通过：

```text
cmake --build build-slicesoft/main --config Debug --target stage14d08_r3_scene_preflight_tests
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R ^stage14d08_r3_scene_preflight_tests$
cmake --build build-slicesoft/main --config Release --target stage14d08_r3_scene_preflight_tests
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R ^stage14d08_r3_scene_preflight_tests$
```

Debug/Release 均为 `1/1 PASS`。同时执行：

```text
ctest --test-dir build-slicesoft/main -C Debug -R ^(stage14d08_r3_scene_preflight_tests|transformed_model_preflight_unit_tests|scene_collision_admission_unit_tests)$ --output-on-failure
ctest --test-dir build-slicesoft/main -C Release -R ^(stage14d08_r3_scene_preflight_tests|transformed_model_preflight_unit_tests|scene_collision_admission_unit_tests)$ --output-on-failure
```

Debug/Release 相关回归均为 `3/3 PASS`。结构门禁：

```text
python scripts/ValidateSourceSizeGuard.py --base-ref HEAD
```

结果为 `PASS`；新增实现文件均低于 500 行限制。

## 5. 保持的边界

- SPI v1、11 个 `pm_*` 导出和 15 项 capability 不变；
- `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 不变；
- 不依赖 Qt、Worker runtime、TIFF Writer、UI 或打印 SDK；
- 不调用单模型生产入口循环伪装 scene-wide 预检；
- 不使用 OpenVDB 实验 admission 覆盖通用 scene admission；
- 不写生产 Package、manifest、TIFF 或 preview。

## 6. 后续状态

```text
14D_08_R3_01A_STATUS=COMPLETE
14D_08_R3_01B_STATUS=PREPARATION_REQUIRED
14D_08_R2_02_STATUS=BLOCKED_BY_R3_01B
PRODUCTION_WORKER_EXECUTOR_REGISTERED=false
PRODUCTION_PACKAGE_WRITTEN=false
```
