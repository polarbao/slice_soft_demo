# DOC_EXEC_12E-08D-01 SlicePipelineMode Router 结果

> 文档状态：COMPLETE
> 日期：2026-07-23
> 下一原子任务：12E-08D-02 Global Production Layer Adapter

## 1. 完成范围

08D-01 只建立端到端模式配置、路由和生产入口守门，不提前实现 Global production adapter：

```text
slicePipeline.mode = legacy | global_surface_shell；
省略 slicePipeline 时保持 legacy；
CLI 默认生产入口改为 RunSlicePipeline；
legacy 继续调用既有 RunSlicePipelineLegacy；
global 先执行共享 ModelPreflightGate；
global blocked/unavailable 时使用稳定错误码并禁止 silent fallback；
08D-02 接入前 global 不写 production package/TIFF。
```

## 2. 代码结果

```text
src/slicer_core/config/SlicePipelineConfig.*
src/slicer_core/pipeline/SlicePipelineRouter.*
src/slicer_core/config.*
src/slicer_core/pipeline/SlicePipeline.*
apps/slicer_cli/main.cpp
tests/unit/slice_pipeline_router/Main.cpp
CMakeLists.txt
```

稳定错误码与 `DOC_SCHEMA_12E_DualSlicePipelineConfig.md` 一致：

```text
E_12E_PIPELINE_MODE_UNSUPPORTED
E_12E_PIPELINE_MODE_CONFIG_MISMATCH
E_12E_PIPELINE_GLOBAL_NOT_ADMITTED
E_12E_PIPELINE_GLOBAL_TOPOLOGY_BLOCKED
E_12E_PIPELINE_PRODUCTION_TIFF_REQUIRED
E_12E_PIPELINE_SILENT_FALLBACK_FORBIDDEN
```

## 3. 兼容与负向行为

| Case | 实际行为 |
|---|---|
| old config omitted | 默认 legacy，保持原生产入口 |
| explicit legacy | 直接执行 legacy，无 fallback 标记 |
| unknown mode | `E_12E_PIPELINE_MODE_UNSUPPORTED` |
| legacy + global-only material semantics | `E_12E_PIPELINE_MODE_CONFIG_MISMATCH` |
| global + legacy material semantics | `E_12E_PIPELINE_MODE_CONFIG_MISMATCH` |
| global topology blocker | `E_12E_PIPELINE_GLOBAL_TOPOLOGY_BLOCKED`，不写 package |
| clean global before 08D-02 | `E_12E_PIPELINE_GLOBAL_NOT_ADMITTED`，不写 package |

## 4. 验证

实际运行并通过：

```text
cmake --build build --config Debug --target slice_pipeline_router_unit_tests slicer_cli
build/Debug/slice_pipeline_router_unit_tests.exe
ctest --test-dir build -C Debug -R "slice_pipeline_router|model_preflight_pipeline_gate|experimental_config" --output-on-failure
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
```

结果：

```text
router tests：7/7 PASS；
定向 CTest：3/3 PASS；
全量 CTest：46/46 PASS；
Quick CI：PASS，耗时约 441.8 s；
UI self-test 与 overlay-load-real：PASS。
```

首次 Quick CI 在链接两个既有 Debug target 时发现上次 E 盘 I/O 故障遗留的损坏 `main.obj/vc145.pdb`。
只把对应生成文件移动到 `build/_corrupt_backup_12e08d_20260723`，重新构建两个 target 后，完整 Quick CI
通过。该处理未修改源码或生产数据。

## 5. 固定边界

```text
p0.rgbwsv.2 未修改；
channelOrder = R G B W S V 未修改；
bitDepth = 8 未修改；
polarity = black_is_print 未修改；
OpenVDB 仍 optional/OFF；
legacy 仍是默认生产模式；
08D-01 没有接入 Global production TIFF；
Global 不可用或被阻断时不回退 legacy。
```

## 6. 下一步 Gate

08D-02 只能新增 Global classification/full-closure 到现有生产层 DTO 的 adapter。不得新增第二个 TIFF
writer，不得在 adapter 未完整表达 RGB/W/S/V 和 closure 语义时把
`global_production_available` 改为 true。
