# REPORT_14B-01A Base/Engine 两库拆分当前状态

> 文档状态：COMPLETE / GATE PASS
> 日期：2026-08-05
> 对应任务：`14B-01A`
> 权威准备：`DOC_PREP_14B_核心Facade与BaseEngine分层实施准备.md`

## 1. 任务结论

`slicer_core` 的单体静态库已拆分为 `slicer_base` 与 `slicer_engine` 两个 Qt-free 静态库，
并保留同名 `slicer_core` 兼容 `INTERFACE` target。正式依赖方向固定为：

```text
slicer_engine -> slicer_base
slicer_core (compatibility interface) -> slicer_engine
```

`slicer_base` 不链接 `slicer_engine`，同一源文件不会重复编入两个静态库。当前 CMake
确定性分配 318 个核心源文件，其中 base 78 个、engine 240 个。

## 2. 窄接口抽取

- `ModelLoadConfig` 只携带模型路径、格式、输出缓存目录、变换与自动定向配置；
  `model.*`、OBJ/3MF importer 不再包含完整 `config.h`。
- `ModelLoadConfigAdapter.cpp` 位于 engine，负责把既有 `SliceConfig` 转换成窄配置，
  保持现有 CLI 调用兼容。
- `OutputResolution` 抽出 DPI 常量与像素尺寸一致性检查，`rip_reader.cpp` 不再依赖
  完整生产配置。
- 14B-00 登记的 4 条 base 候选到 engine include 边已降为 0。

## 3. 构建与依赖边界

- Windows 模型导入依赖 `windowscodecs`、`ole32` 归 `slicer_base`。
- `psapi`、生产 Writer、可选 LibTIFF 与可选 OpenVDB 归 `slicer_engine`。
- OpenVDB 仍默认关闭；LibTIFF 仍为可选后端；手写 TIFF Writer 仍为默认。
- `model_import_layering_probe` 直接链接 `slicer_base`，验证模型导入不需要 engine。
- `stage14b_layer_assignment.txt` 由 CMake 配置阶段生成，作为 CI 构建图证据。

## 4. 自动门禁

- `ValidateStage14BLayeringFeasibility.py` 校验窄接口、逐文件归属及零反向 include 边。
- `ValidateStage14BTargetGraph.py` 校验 source 无重复、base/engine 分配与 CMake target
  单向依赖。
- CTest 已登记 `slicer_stage14b_target_graph_test`，并继续运行模型导入探针、Facade
  头文件合同和源码行数门禁。

## 5. 验证结果

```text
Debug 全量构建：PASS
Release 全量构建：PASS
14B 定向 CTest：7/7 PASS
Debug 全量 CTest：107/109 PASS
Release 全量 CTest：107/109 PASS
```

全量 CTest 中 `grid_layout_policy_unit_tests` 与 `scene_layer_adapters_unit_tests` 仍失败；
两者不属于本卡文件所有权，本卡未修改排版间距或场景层平移实现。该现象作为现有回归
基线缺口记录，不通过修改 14B 分层合同绕过。

## 6. 保持不变的边界

- `PM_SPI_VERSION=1`、11 个 `pm_*` 导出、15 项能力不变。
- `p0.rgbwsv.2`、`R G B W S V`、uint8、`black_is_print` 不变。
- 生产切片算法、TIFF 像素、RIP、Qt UI 与 Worker 行为不变。
- 14B-02 才拆分只读 TIFF 能力并实现 Model/PackageQuery Facade；本卡不提前实现。

## 7. 后续任务

`14B-02`、`14B-03` 与 `14B-04` 已具备并行开发条件，必须分别提交。`14B-03A`
仍等待 `14B-02`、`14B-03` 与 14A-04-R1 合同前置，不得提前以灰模替代真实纹理。
