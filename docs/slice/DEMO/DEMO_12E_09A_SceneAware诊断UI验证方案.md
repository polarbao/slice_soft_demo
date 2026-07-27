# DEMO 12E-09A Scene-aware 诊断 UI 验证方案

> 文档版本：v1.0
> 文档状态：Formal DEMO / 09A-02 READY
> 日期：2026-07-27

## 1. 验证目标

验证 09A 诊断链路能在不改变生产切片语义的前提下，同时绑定单模型和场景当前实例，并在配置、
异步分析和同层预览中拒绝 stale、跨实例和跨层结果。

## 2. 09A-02 fixture

```text
single_model 正向配置；
13B-01 fixture_two_model_scene.json；
scene instance_a / instance_b 两个 current-instance 请求；
bad subjectType；
缺 sceneId/instanceId；
不存在的 instance reference；
stale sceneRevision/transformRevision；
Profile mismatch；
configHash tamper；
取消与写失败。
```

## 3. 09A-02 验证矩阵

| Case | 输入 | 预期 |
|---|---|---|
| 09A-E01 | single_model | 生成、保存、回读一致 |
| 09A-E02 | scene + instance_a | scene/instance/revision 完整 |
| 09A-E03 | scene 切换 instance_b | A 配置 stale |
| 09A-E04 | sceneRevision 改变 | 旧配置 stale |
| 09A-E05 | transformRevision 改变 | 旧配置 stale |
| 09A-E06 | Profile mismatch | fail-closed |
| 09A-E07 | hash tamper | integrity failed |
| 09A-E08 | cancel | 无 staging/final 新文件 |
| 09A-E09 | 源与输出同路径 | write failed |
| 09A-E10 | 旧 production config | 不被覆盖，回归通过 |

## 4. 09A-03 UI Smoke

```text
中文宽度和材料控件；
0.01 mm 步长、两位小数；
single_model/scene/current instance 身份摘要；
pending/unavailable/blocked/diagnostic；
最长中文在 1280x720、1440x900、1920x1080 无遮挡；
tooltip 与 disabled reason 可见。
```

## 5. 09A-04 Worker

```text
成功、失败、取消、重复运行；
关闭窗口和切换模型/实例；
旧 revision/configHash 结果丢弃；
UI 线程不执行 topology/distance/texture/raster 重任务；
无 QObject 悬挂和重复完成回调。
```

## 6. 09A-05 同层预览

前置：13C-03 COMPLETE。

```text
生产 RGBWSV TIFF 提供底图；
Texture/Fill/Partition 语义使用同一 layerIndex/zMm；
Support/Varnish 由生产通道显示；
缺少诊断 mask 显示未提供；
不按 preview 文件序号跨层兜底；
不写新的生产 TIFF；
RGBWSV 像素探针与叠加图一致。
```

## 7. 计划验证命令

09A-02：

```powershell
cmake --build build --config Debug --target diagnostic_effective_config_unit_tests production_effective_config_unit_tests multimodel_scene_contract_unit_tests
ctest --test-dir build -C Debug -R "^(diagnostic_effective_config_unit_tests|production_effective_config_unit_tests|multimodel_scene_contract_unit_tests)$" --output-on-failure
git diff --check
```

09A-03..06 另加 Qt build、`--self-test`、任务专用 `--ui-smoke-test` 和 Quick CI。本文是计划，
不得在任务执行前把这些命令写成已通过。

## 8. 通过标准

```text
single_model/scene 身份不混淆；
current instance 和 revision 可审计；
stale/cancel/失败不污染成功文件；
UI/Worker 不阻塞且不跨身份复用；
同层预览不跨 layer；
生产 Profile、package 和 RGBWSV 协议保持不变。
```
