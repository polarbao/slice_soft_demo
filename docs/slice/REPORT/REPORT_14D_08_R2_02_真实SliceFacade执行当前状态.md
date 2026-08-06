# REPORT_14D-08-R2-02 真实 SliceFacade 执行当前状态

> 日期：2026-08-06
>
> 状态：`COMPLETE / CONTROLLED DEVELOPMENT EVIDENCE`

## 1. 交付结果

本任务打通了文件合同到现有生产切片入口的真实执行链：

```text
request.json
  -> job 内 scene/Profile/effective config 物化
  -> Worker 内权威 full preflight
  -> CreateProductionSliceFacade()
  -> MultiModelProductionService
  -> RGBWSV Package（受控开发证据）
```

- Worker 不复制 CLI、模型加载、栅格化或 TIFF Writer；
- full preflight 必须为 `authoritative=true`、`complete=true`、`admitted=true`；
- committed scene 还必须携带可见实例的已提交准入，Worker 不在 hash 后静默改写 scene；
- 取消、stale admission、Profile/scene 身份错误和 Facade 失败均不返回成功；
- 进度使用保留的 `SLICE_PROGRESS`，耗时使用 `SLICE_TIMING`；
- 成功结果保留 package、manifest、grid、Profile echo、engine 与 elapsed 基础证据。

## 2. 实现修正

真实 production scene 首次贯穿 Writer 时暴露了既有准入字符串不一致：生产服务传入
`scene_production_admitted`，而冻结 RGBWSV Writer 合同要求 `admitted`。本任务修正为 Writer
权威值，并保留 fixture 的 `functional_fixture_admitted`，没有修改 TIFF 协议或材料数据。

## 3. 主要文件

```text
apps/slicer_worker/slice/WorkerSliceExecutor.*
apps/slicer_worker/slice/WorkerSliceRequestMaterializer.*
src/slicer_core/pipeline/MultiModelProductionService.cpp
tests/stage14d_08_r2/WorkerSliceExecutorTests.cpp
CMakeLists.txt
```

## 4. 已执行验证

```text
Debug  stage14d08_r2_slice_materializer_tests        PASS
Debug  stage14d08_r2_slice_executor_tests            PASS
Debug  stage14d08_r3_worker_preflight_tests          PASS
Release stage14d08_r2_slice_materializer_tests       PASS
Release stage14d08_r2_slice_executor_tests           PASS
Release stage14d08_r3_worker_preflight_tests         PASS
ValidateStage14BTargetGraph.py                        PASS（base=129 / engine=244）
ValidateCapabilityDtos.py                             PASS（15 capabilities）
ValidateFileContract.py                               PASS
ValidateThreeLaneContract.py                          PASS
git diff --check                                      PASS
```

正例使用真实 production build volume、真实 OBJ importer、权威 full preflight 和唯一生产 Facade
生成 RGBWSV Package；负例覆盖预取消和未提交 admission，均未发布 Package。

## 5. 边界与下一步

- 本任务未注册 production `slice.rgbwsv` executor；
- 未宣称 staging、租约、原子 publish、崩溃恢复或模块二次清理完成；
- 未补齐完整 Worker 成功 DTO 的 per-instance/strict package 证据；
- 未进入 14E UI 宿主开发。

下一步先实施 `14D-08-R3-02B` repair Writer/Facade/Worker，再复核并完成 `14D-05`。只有安全发布
闭合后，`14D-08-R2-03` 才能注册真实 slice executor 并完成独立入口正负例。
