# REPORT_14E-04 带纹理俯视渲染与移动优化当前状态

> 状态：COMPLETE
> 日期：2026-08-07
> 前置：14E-03、14B-03A COMPLETE
> 下一任务：14E-04b 能力覆盖达标

## 1. 任务目标

Qt 参考宿主仅通过冻结的公开 ABI 读取 `scene.get_viewdata` top 数据与
`surfacePreview` blob，在宿主本地完成真实纹理俯视渲染、移动预览和缓存复用，
同时关闭 UI-M2 Commit 延迟与 UI-M3 帧率门禁。

## 2. 实现内容

| 模块 | 交付物 | 责任 |
|---|---|---|
| top 数据读取 | `TopViewRenderPolicyData.cpp` | 校验 ViewData/appearance/preview 身份闭合；经既有 `read_blob` 子操作读取 RGBA8；纹理缺失或 DTO 不完整时 fail-closed |
| 本地纹理渲染 | `TopViewRenderPolicy.cpp` | 使用 `localBoundsMm + worldMatrix` 绘制 +Z 正交纹理；提供 caller-owned `RenderInto` 高频路径 |
| 缓存 | `TopViewRenderPolicy` | `previewIdentity` 缓存像素，`viewDataIdentity + localRevision + canvasSize` 缓存布局；实例移动不重新读取纹理 blob |
| 移动优化 | `MoveOptimizationPolicy` | 本地矩阵预览、提交采纳与回滚；对象不持有 `ModuleClient`，移动帧结构上保持零 DLL 调用 |
| Commit 身份 | `SceneInteractionController` | 正常 Commit 直接采纳响应中的 `viewdataIdentity`，不追加 snapshot |
| 自动门禁 | `Stage14E04TopViewTests.cpp`、`Stage14E04RenderBenchmark.cpp` | 真实 DLL 纹理/缓存/移动/Commit 测试；与主干 `ModelTopViewWidget` 同模型同画布帧率比较 |

`TopViewRenderPolicy` 的 DTO/blob 解码与渲染分别拆文件，参考宿主所有源文件继续满足
单文件不超过 500 行的 Stage 14B-06 门禁。

## 3. 冻结边界

- top 视图固定为 +Z 正交，并要求真实 `surfacePreview`，不允许灰模成功兜底。
- 纹理像素按 `previewIdentity` 缓存；worldMatrix 与纹理身份分离。
- 移动、重绘、画布缓存均为宿主本地行为，不调用 DLL。
- 正常 Commit 直接使用 `scene.apply_operation` 响应，不尾随 snapshot。
- 未修改 SPI v1、11 个 `pm_*` 导出、15 项能力、生产 TIFF 或 RGBWSV Package。
- 主干 `slicer_debug_ui` 仅作为独立性能基准参与测试，没有产品代码改动。

## 4. 验证结果

### 4.1 Debug / Release 回归

```text
cmake --build build --config Debug --target
  stage14e04_render_benchmark stage14e04_top_view_tests slicer_ui_host_sim
ctest --test-dir build -C Debug -R "slicer_stage14e0(2|3|4)_|slicer_stage14e04_render_benchmark_smoke"
6/6 PASS

cmake --build build --config Release --target
  stage14e04_render_benchmark stage14e04_top_view_tests slicer_ui_host_sim
ctest --test-dir build -C Release -R "slicer_stage14e0(2|3|4)_|slicer_stage14e04_render_benchmark_smoke"
6/6 PASS
```

### 4.2 UI-M2 / 本地移动

Release 真实模块结果：

```text
colors=2
blobReads=1
commits=60
p95Ms=0.3819
localFrames=300
localRenderMs=178
```

- `UI-M2`：60 次 Commit 的 P95 为 0.3819 ms，满足不高于 150 ms。
- 纹理缓存：重复刷新只读取 1 次 blob；颜色变化证明确实显示纹理而非纯色占位。
- 本地移动：300 帧渲染不增加 ABI 调用，且不使 preview/texture cache 失效。
- 正常 Commit：测试断言未追加 snapshot，并采纳新的 ViewData identity。

### 4.3 UI-M3 30 秒性能门禁

Release、Qt offscreen、800 x 400 同画布、同一纹理模型；主干与参考宿主各累计
30 秒渲染时间，按小批次交替采样以避免前后运行的系统负载偏差：

```text
mainFps=1076.08
hostFps=1434.45
ratio=1.33303
```

参考宿主为主干帧率的 133.303%，满足 `UI-M3 >= 90%`。

### 4.4 合同与结构门禁

```text
ValidateCapabilityDtos.py          PASS（15 capabilities）
ValidateThreeLaneContract.py      PASS
ValidateQtHostBoundary.py         PASS
slicer_source_size_guard_self_test PASS
```

## 5. 后续

14E-04 已完整关闭 top 纹理、缓存、本地移动、UI-M2 与 UI-M3。下一张原子卡为
14E-04b：按 P0/P1/P2 能力清单补齐参考宿主演示，并验证取消不超过 2 秒、无
`.staging` 残留以及 DLL 缺失优雅失败。
