# REPORT_14E-04c 带纹理 3D 视角与相机操作当前状态

> 状态：COMPLETE
> 日期：2026-08-07
> 适用分支：`feature/14-slicer-capability-package`

## 1. 任务目标

14E-04c 要求 Qt 参考宿主在不新增模块 ABI、相机交互不跨 DLL 的前提下，消费
`scene.get_viewdata(viewMode=three_d)` 的真实 mesh、UV、submesh、material 和 texture，
并经后端中立的 `IRenderBackend` 显示带纹理 3D 场景。宿主还需提供 orbit、pan、
光标中心 zoom、七向预设、正交/透视切换、构建体积、网格、坐标轴和越界高亮。

## 2. 实现结果

- 新增 `IRenderBackend` 内部参考接口，渲染层不暴露 Qt 或具体图形 API 类型；首个
  `cpu_raster` 后端满足本任务允许的最低后端范围。
- 新增 `SceneRenderPolicy`，仅在 Refresh 阶段通过公开 SPI 获取权威 snapshot 与
  `three_d` ViewData；Render 阶段只使用宿主本地 Frame，不调用 DLL。
- 新增 `AppearanceCache`，按 `meshIdentity`、`textureIdentity`、
  `appearanceIdentity + materialId` 幂等上传。相同 ViewData 重读以及实例 worldMatrix
  变化不会重复读取 mesh/texture blob。
- 新增 `CameraController`，提供 orbit、pan、光标中心 zoom、Top/Bottom/Front/Back/
  Left/Right/Isometric 七向预设及正交/透视投影。
- CPU 参考后端实现真实 UV 纹理采样、submesh 材质分组、深度缓存、透明遮罩、选中态、
  越界红色高亮，以及构建体积底面、1 mm/10 mm 网格和 XYZ 坐标轴。
- 10 万三角面门禁中将顶点投影按实例缓存，避免每个索引三角形重复变换顶点；该优化
  只作用于宿主显示，不影响切片几何、TIFF 或 Package。

## 3. 合同闭合

| 合同项 | 结果 |
|---|---|
| `three_d` mesh/UV/index/submesh | PASS |
| appearance/material/texture identity 闭合 | PASS |
| 真实纹理颜色可见 | PASS |
| 缺失/无效 blob、UV、材质引用 fail-closed | PASS |
| local mesh + worldMatrix 实例显示 | PASS |
| 同身份 mesh/texture 缓存复用 | PASS |
| 构建体积、网格、坐标轴、越界高亮 | PASS |
| 七向相机、orbit/pan/zoom、正交/透视 | PASS |
| UI-M7 相机操作跨 DLL 调用数 | `0`，PASS |
| UI-M8 10 万三角面连续 orbit P5 | `51.4168 FPS`，PASS |

Release UI-M8 使用 100352 个三角面、320 x 320 视口连续运行 30 秒；门槛为
P5 不低于 30 FPS。该数字是本机当前负载下的实际结果，不代表其他设备的承诺值。

## 4. 验证证据

已实际执行：

```text
cmake --build --preset slicesoft-debug --target slicer_ui_host_sim \
  stage14e03_interaction_tests stage14e04_top_view_tests \
  stage14e04c_three_d_tests stage14e04_render_benchmark
ctest --test-dir build-slicesoft/main -C Debug \
  -R "slicer_stage14e0(2|3|4)" --output-on-failure
结果：8/8 PASS

cmake --build --preset slicesoft-release --target slicer_ui_host_sim \
  stage14e03_interaction_tests stage14e04_top_view_tests \
  stage14e04c_three_d_tests stage14e04_render_benchmark
ctest --test-dir build-slicesoft/main -C Release \
  -R "slicer_stage14e0(2|3|4)" --output-on-failure
结果：8/8 PASS

stage14e04c_three_d_tests.exe --module slicer_module.dll
结果：PASS；meshUploads=1，textureUploads=1，blobReads=2，cameraCalls=0，
      p5Fps=51.4168

python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
python tests/stage14e_02/ValidateQtHostBoundary.py --repo-root . --binary <Release host>
python scripts/ValidateSourceSizeGuard.py --base-ref HEAD
结果：全部 PASS；源码行数门禁仅报告既有白名单警告
```

## 5. 边界与下一步

本任务建立的是打印软件参考宿主的 3D 渲染内核和门禁，尚未把 top/three_d 切换控件、
默认视图持久化和白/近白纹理对比辅助接入可见 UI；这些属于 14E-04d。

本任务未修改 `PM_SPI_VERSION=1`、11 个 `pm_*` 导出、15 项能力、
`p0.rgbwsv.2`、RGBWSV 通道顺序、8-bit 位深、`black_is_print` 极性、生产 TIFF 或
现有主干 `slicer_debug_ui`。`cpu_raster` 是可替换的参考后端，不构成 Qt 6/QRhi 迁移，
也不承诺作为最终生产 GPU 后端。
