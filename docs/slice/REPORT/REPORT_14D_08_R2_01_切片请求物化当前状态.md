# REPORT_14D-08-R2-01 切片请求物化当前状态

> 日期：2026-08-06
>
> 状态：`COMPLETE`
>
> 父任务：`14D-08-R2`
>
> 下一关键路径：`14D-08-R3-01A/01B -> 14D-08-R2-02`

## 1. 完成结论

已实现 Worker 私有的 `slice.rgbwsv` 请求物化层。它把 R1 保留的内嵌 scene、Profile 和 output
转换成 job 目录内的受信文件，并在任何算法调用前完成 production scene、双 hash、资源路径、
Profile 身份、DPI/层高/pipeline 和 packageDir 一致性校验。

本任务不注册 production executor、不调用切片算法、不创建 package/TIFF，也不绕过尚缺失的
scene-wide authoritative full preflight。因此 R2-01 完成不等于 R2、14D-08 或 M-MVP 完成。

## 2. 代码交付

| 文件/目标 | 内容 |
|---|---|
| `apps/slicer_worker/slice/WorkerSliceRequestMaterializer.*` | scene/Profile hash、资源与输出校验；原子物化；取消清理；稳定错误码 |
| `slicer_worker_slice_runtime` | engine-linked Worker 私有切片层，不污染 base-only R1 runtime |
| `tests/stage14d_08_r2/WorkerSliceRequestMaterializerTests.cpp` | 正向物化、scene/Profile hash 负例、relative resource 和取消清理 |
| `stage14d08_r2_slice_materializer_tests` | Debug/Release 定向 CTest 入口 |

物化产物固定为：

```text
<job>/scene.snapshot.json
<job>/profile.effective.json
<job>/scene_config.effective.json
```

`profileHash` 使用移除自身后的完整 Profile canonical JSON 计算；外部 sceneHash 使用
`sha256:<digest>`，内部 effective config 保持现有不带前缀摘要。输出继续精确要求
`p0.rgbwsv.2`。

## 3. 验证证据

实际运行并通过：

```text
cmake --build build-slicesoft/main --config Debug --target stage14d08_r2_slice_materializer_tests
ctest --test-dir build-slicesoft/main -C Debug -R ^stage14d08_r2_slice_materializer_tests$ --output-on-failure
cmake --build build-slicesoft/main --config Release --target stage14d08_r2_slice_materializer_tests
ctest --test-dir build-slicesoft/main -C Release -R ^stage14d08_r2_slice_materializer_tests$ --output-on-failure
```

Debug/Release 均为 `1/1 PASS`。同时执行 R1+R2 定向回归：

```text
ctest --test-dir build-slicesoft/main -C Debug -R ^(stage14d08_r1_worker_runtime_tests|stage14d08_r1_result_writer_tests|stage14d08_r1_dispatcher_tests|stage14d08_r2_slice_materializer_tests)$ --output-on-failure
ctest --test-dir build-slicesoft/main -C Release -R ^(stage14d08_r1_worker_runtime_tests|stage14d08_r1_result_writer_tests|stage14d08_r1_dispatcher_tests|stage14d08_r2_slice_materializer_tests)$ --output-on-failure
```

Debug/Release 均为 `4/4 PASS`。结构验证：

```text
python tests/stage14d_08_r1/ValidateWorkerRuntimeBoundaries.py --repo-root .
python tests/contracts/ValidateStage14BTargetGraph.py --assignment build-slicesoft/main/stage14b_layer_assignment.txt
```

两项均 PASS，target graph 为 `base=127, engine=239, duplicateSources=0`。

## 4. 保持的边界

- SPI v1、11 个 `pm_*` 导出、15 项 capability 未修改；
- `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 未修改；
- production registry 未安装 slice fake/placeholder executor；
- 未创建 package、manifest、TIFF 或伪成功 result；
- 未修改 Qt UI、RIP、TIFF Writer、材料策略和 OpenVDB 默认状态；
- 14D-05 staging/backup/lease/recovery 仍由后续任务独占。

## 5. 剩余阻塞

| 项 | 状态 | 原因 |
|---|---|---|
| `14D-08-R2-02` | BLOCKED | 缺 `R3-01A/01B` scene-wide authoritative full preflight |
| `14D-08-R2-03` | BLOCKED | 需 R2-02 和 14D-05 安全发布 |
| `14D-08-R3-01A` | PREPARATION REQUIRED | 需冻结 scene service DTO、聚合和 admission 调用关系 |
| `14D-08-R3-02A` | PREPARATION REQUIRED | 需冻结 repair input/output/evidence 与 minor 兼容策略 |
| `14D-07-R1` | READY | E-01..08 已由 R0 冻结，可建设参数化测试外壳 |

## 6. 状态声明

```text
14D_08_R2_01_STATUS=COMPLETE
14D_08_R2_STATUS=IN_PROGRESS
14D_08_R2_02_STATUS=BLOCKED_BY_R3_01
14D_08_PARENT_STATUS=BLOCKED
PRODUCTION_SLICE_EXECUTOR_REGISTERED=false
PRODUCTION_PACKAGE_WRITTEN=false
```
