# TASKS_00_P0切片Demo任务清单

## Milestone 0：工程骨架

- [ ] 创建根目录 `CMakeLists.txt`
- [ ] 创建 `vcpkg.json`
- [ ] 创建 targets：
  - [ ] `slicer_core`
  - [ ] `slicer_cli`
  - [ ] `rip_reader_test`
- [ ] 添加 README
- [ ] 添加样例配置文件

## Milestone 1：配置系统

- [ ] 实现 `SliceConfig`
- [ ] 实现 JSON 配置读取
- [ ] 校验必要字段
- [ ] 添加默认值
- [ ] 添加 `samples/configs/slice_config.json`

## Milestone 2：模型导入

- [ ] 实现 STL 读取
- [ ] 实现 OBJ 读取，或使用 Assimp
- [ ] 计算包围盒
- [ ] 应用单位、缩放、旋转、平移
- [ ] 输出模型加载报告

## Milestone 3：层采样原型

- [ ] 根据 DPI 和层厚建立 GridSpec
- [ ] 实现简单 occupancy / layer sampler
- [ ] 生成每层 model mask
- [ ] 支持 600 DPI 和 0.01mm 层高

## Milestone 4：下表面投影支撑

- [ ] 找到每个 XY 像素的最低模型层
- [ ] 从构建平台到模型底部生成 support mask
- [ ] 保证 `Model > Support`
- [ ] 写入 `support_report.json`

## Milestone 5：RGBWSV TIFF Writer

- [ ] 创建六通道 uint16 layer buffer
- [ ] 通道顺序固定为 `R G B W S V`
- [ ] 写入 tiled TIFF
- [ ] 使用 contiguous planar config
- [ ] 每层一个 TIFF

## Milestone 6：Manifest 和 Reports

- [ ] 写入 `manifest.json`
- [ ] 写入 `slice_report.json`
- [ ] 写入 `repair_report.json`
- [ ] 写入 `support_report.json`

## Milestone 7：RIP Reader Test

- [ ] 读取 manifest
- [ ] 读取 TIFF layers
- [ ] 校验 channel count / channel order / bit depth
- [ ] 校验 layer 尺寸
- [ ] 打印通道 checksum

## Milestone 8：可选 Preview

- [ ] 输出 RGB preview PNG
- [ ] 输出 support preview PNG
- [ ] 支持 interval-based preview config

## Milestone 9：可选 Qt Demo

- [ ] 创建 `slicer_qt_demo`
- [ ] 加载配置
- [ ] 启动 `slicer_cli` 或调用 `slicer_core`
- [ ] 显示日志
- [ ] 显示 layer/channel preview

## Milestone 10：可选 Mesh Preview

- [ ] 添加 `QOpenGLWidget` mesh viewer
- [ ] 绘制 mesh wireframe
- [ ] 绘制 bbox
- [ ] 绘制 build plate
- [ ] 绘制当前 slice plane
