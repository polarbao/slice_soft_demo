# REPORT_14E-04b 能力覆盖达标当前状态

> 状态：COMPLETE
> 日期：2026-08-07
> 适用分支：`feature/14-slicer-capability-package`

## 1. 任务目标

14E-04b 要求 Qt 参考宿主仅经冻结的公开 C SPI 覆盖 15 项能力：P0 五项和
P1 五项必须端到端成功，P2 六项必须至少完成一次终态调用并记录结果；同时关闭
UI-M5 协作取消和 UI-M6 缺失 DLL 两项门禁。

本任务不修改 `PM_SPI_VERSION=1`、11 个 `pm_*` 导出、15 项能力清单，也不改变
`p0.rgbwsv.2`、RGBWSV 通道顺序、8-bit 位深或 `black_is_print` 极性。

## 2. 实现结果

- 新增 `CapabilityCoverageRunner`，通过 `ModuleClient` 串联模型导入、场景提交、
  ViewData、几何检查、Worker 切片、Package 查询、修复和模型释放。
- 新增宿主侧请求构造器，复用纯 C 宿主的 Scene/Profile 生成逻辑，不链接
  `slicer_core`、`slicer_base`、`slicer_engine` 或 `slicer_module.lib`。
- 新增 `--capability-self-test` 命令行入口及 CTest，证据写入构建目录下独立的
  `capability_coverage.json`，不会进入生产 Package。
- UI-M5 使用活动 `slice.rgbwsv` Worker 作业验证取消；要求 2 秒内进入
  `cancelled`、返回 `PM-SLICER-CANCELLED-0070`，且无 `.staging`、`.backup`、
  `.lease` 残留。
- UI-M6 继续由 `slicer_stage14e02_qt_host_missing_module_test` 验证，缺失 DLL
  时参考宿主输出稳定诊断并退出，不发生崩溃。
- `ModuleClient` 的三态缓冲读取允许终态 JSON 在探测与读取之间发生长度变化：
  增长时重新探测，缩短时按实际写入长度接收，避免 Worker 结果长度抖动造成误报。

## 3. 能力覆盖

| 级别 | 能力 | 结果 |
|---|---|---|
| P0 | `model.import` | PASS |
| P0 | `scene.apply_operation` | PASS |
| P0 | `scene.get_snapshot` | PASS |
| P0 | `slice.rgbwsv` | PASS |
| P0 | `package.verify` | PASS |
| P1 | `scene.get_viewdata`（含 blob） | PASS |
| P1 | `geometry.collision` | PASS |
| P1 | `geometry.preflight(fast)` | PASS |
| P1 | `package.get_layer_descriptor` | PASS |
| P1 | `package.render_layer_preview` | PASS |
| P2 | `model.get_metadata` | RETURN RECORDED |
| P2 | `geometry.preflight(full)` | RETURN RECORDED |
| P2 | `geometry.repair` | RETURN RECORDED |
| P2 | `package.get_summary` | RETURN RECORDED |
| P2 | `package.read_report` | RETURN RECORDED |
| P2 | `model.release` | RETURN RECORDED |

最新 Debug 证据汇总为 `P0=5/5`、`P1=5/5`、`P2=6/6`。其中
`package.get_summary` 在当前最小包上返回 `PM-SLICER-CONTRACT-0060`，符合 P2
“调用并记录”的本卡要求；该返回没有被伪装为成功，完整代码保留在机器证据中。

## 4. 验证证据

已实际执行：

```text
cmake --build build --config Debug --target slicer_ui_host_sim --parallel
ctest --test-dir build -C Debug -R "slicer_stage14e0(2|3|4)" --output-on-failure
结果：7/7 PASS

cmake --build build --config Release --target slicer_ui_host_sim \
  stage14e03_interaction_tests stage14e04_top_view_tests --parallel
ctest --test-dir build -C Release -R "slicer_stage14e0(2|3|4)" --output-on-failure
结果：7/7 PASS

python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
python tests/stage14e_02/ValidateQtHostBoundary.py \
  --repo-root . --binary build/apps/slicer_ui_host_sim/Release/slicer_ui_host_sim.exe
python scripts/ValidateSourceSizeGuard.py --base-ref HEAD
结果：全部 PASS；源码行数门禁仅报告既有白名单警告
```

UI-M5 实测取消延迟为 103 ms，无 owned 临时产物残留。实际数值会随机器负载变化，
自动门禁始终以不超过 2000 ms 为准。

## 5. 边界与下一步

14E-04b 已闭合能力覆盖，不等于 3D 视图完成。下一任务为 14E-04c：在
`IRenderBackend` 边界下实现带纹理 `three_d` 视图、相机操作和本地零 DLL 调用；
随后由 14E-04d 完成 top/three_d 双入口、网格与设置持久化。
