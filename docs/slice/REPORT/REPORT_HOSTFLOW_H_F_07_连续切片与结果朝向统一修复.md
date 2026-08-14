# REPORT HOSTFLOW H-F-07 连续切片与结果朝向统一修复

> 状态：COMPLETE
> 日期：2026-08-14

## 1. 问题与根因

### 1.1 完成一次后不能再次切片

作业控制器在终结前已经释放 Worker 句柄，本身可以复用；缺口在宿主完成态：

- 默认输出 Profile 仍指向刚发布的生产包目录，没有生成下一次会话身份；
- 结果后台加载期间切片按钮会被临时禁用，但完成后只刷新旧的有效 Profile；
- 完成摘要会保留，因此此前的就绪失败原因也不再显示，容易表现为“按钮永久置灰”。

### 1.2 工作区和结果页朝向不同

工作区顶视图按 `lower_left` 构建体积显示，`+X` 向右、`+Y` 向上。生产 TIFF
的栅格索引则以行 0 表示最小 Y；原结果 PNG 直接把 TIFF 行 0 写到图像顶部，
视觉上等价于 Y 轴镜像。H-F-06 的新旧 TIFF 比对只证明两代生产栅格方向一致，
没有覆盖工作区与 PNG 显示行原点的差别。

## 2. 修复

1. 自动管理的默认输出在成功作业后轮换到新的
   `<应用根>/output/h<yyMMddHHmmsszzz>/package`。其中 `output` 是稳定根目录，
   明确不得缩写为 `out`；只缩短内部临时会话名。
2. 轮换后以仍在工作区的模型、权威场景身份和当前工艺重建有效 Profile；结果包仍按
   已完成作业返回的旧目录加载，两者互不覆盖。用户自定义输出目录不自动改写。
3. 结果预览在 TIFF 通道合成后、缩放与 PNG/BMP/PPM 编码前垂直翻转显示行，使最大 Y
   位于图像顶部；缓存语义版本从 `1` 提升为 `2`。
4. 不修改生产 TIFF 字节、`p0.rgbwsv.2`、RGBWSV 顺序、`black_is_print` 极性、
   场景变换或工作区相机。

## 3. 回归覆盖

- H-B-05：自动输出轮换后路径不同、仍位于 `output` 根、有效 Profile 仍就绪；
  自定义输出不被轮换。
- H-B-06：同一 `HostSliceJobController`、同一场景连续提交两次，两个独立生产包
  均发布成功，终结后句柄均释放。
- H-B-06 UI smoke：完成态调用 `SetReady(true)` 后“开始切片”重新启用。
- 材料预览单元测试：原始最小 Y 行移动到底部、最大 Y 行移动到顶部，中间行不变。
- H-D-01：工作区顶视图的 `+Y` 向上坐标合同继续通过。

## 4. 验证

```text
Release build:
  material_preview_composer_unit_tests
  hostflow_hb05_slice_settings_tests
  hostflow_hb06_slice_job_tests
  hostflow_hd01_top_canvas_tests
  slicer_ui_host_sim
  slicer_module

Release CTest: 8/8 PASS
  material_preview_composer_unit_tests
  hostflow_hb05_slice_settings
  hostflow_hb05_settings_ui_smoke
  hostflow_hb06_slice_job
  hostflow_hb06_job_ui_smoke
  hostflow_hb07_package_review
  hostflow_hb07_result_ui_smoke
  hostflow_hd01_top_canvas

Core preview regression CTest: 7/7 PASS
  package_query_facade_14b02_unit_tests
  package_query_facade_14b02_base_link_test
  preview_output_policy_unit_tests
  tiff_layer_source_unit_tests
  tiff_layer_cache_unit_tests
  texture_fill_partition_semantic_preview_unit_tests
  material_preview_composer_unit_tests

Additional gate:
  slicer_source_size_guard_self_test PASS
```

历史 `slicer_stage14e02_qt_host_boundary_test` 仍会在扫描到
`apps/slicer_ui_host_sim/CMakeLists.txt` 中测试目标对 `slicer_base` 的链接时失败；该行和
`HostSliceSettingsPanel.cpp` 超过旧 500 行阈值均已存在于当前 `HEAD`，不是 H-F-07
引入。本卡没有通过改名隐藏该问题，也没有扩大到测试架构重构。

`runtime/slicesoft/Release` 已部署；构建产物与运行时的宿主 EXE、模块 DLL
SHA-256 分别一致。发布目录验证通过：

```text
STAGE14E02_SELF_TEST_PASS spi=1 calls=6
HOSTFLOW_HB06_UI_PASS
HOSTFLOW_HB07_UI_PASS
```
