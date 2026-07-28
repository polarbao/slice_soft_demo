# REPORT 13B-07 真实模型矩阵与阶段收口当前状态

> 文档状态：FUNCTIONAL MATRIX COMPLETE / PRODUCTION INPUT OPEN  
> 日期：2026-07-28  
> 前置：13B-06 FIXTURE COMPLETE  
> 下一任务：13C-01 TIFF Layer Source 与 LRU Cache

## 1. 阶段结论

13B-07 已完成真实 OBJ、Texture2D 3MF、1/11/12/22 实例、规则排版、逐实例准入、
局部切片层复用、联合 RGBWSV 合成、单 Package 发布和 RIP strict 的功能矩阵。

本阶段结论为：

```text
13B 功能矩阵：PASS；
M13-3 功能候选：PASS；
正式设备 production GO：INPUT_OPEN。
```

功能通过不替代设备输入。正式生产准入仍等待设备 buildVolume、原点/X/Y 轴向和 22 实例性能预算。

## 2. 实现范围

### 2.1 真实模型矩阵执行器

新增 `multi_model_scene_matrix`，覆盖：

```text
13B-M01：1 个 xiao_ma OBJ；
13B-M11：11 个 xiao_ma OBJ；
13B-M12：11 个 xiao_ma OBJ + 1 个 yecan OBJ；
13B-M22：11 个 xiao_ma OBJ + 11 个 yecan OBJ；
13B-M3F：1 个 xiao_ma OBJ + 1 个 Texture2D 3MF。
```

执行器使用固定功能 Profile：

```text
Legacy；
127 x 127 DPI；
0.20 mm 层厚；
11 列 x 2 行；
列/行净距 20/30 mm；
modelFill=white/all_model；
preview.enabled=false；
storageMode=stripped；
fixture buildVolume 显式标记。
```

该 Profile 用于低成本、可重复功能验证，不覆盖用户生产配置。

### 2.2 平移实例资源复用

新增纯 XY 平移局部 Raster 复用：

```text
同一 model/source；
旋转、缩放和镜像一致；
仅平移发生变化；
scene/instance/revision/hash 重新绑定；
局部 Grid 原点随目标实例平移；
偏移量必须满足半像素量化容差。
```

旋转、缩放、镜像、过期 identity 或超过量化容差时 fail-closed，不复用旧 Raster。

矩阵结果：

```text
11 实例：生产器调用 1 次，复用 10 次；
12 实例：生产器调用 2 次，复用 10 次；
22 实例：生产器调用 2 次，复用 20 次。
```

### 2.3 材料闭环

真实贴图可能包含 `RGB=(255,255,255)`。在 `black_is_print` 协议下，该 RGB 值本身表示三个
RGB 通道均不打印。矩阵 Profile 因此固定 `modelFill=white/all_model`，使纯白纹理模型像素仍由
W 通道承载实体材料，避免被误判为 Empty。

SceneLayerComposer 的闭环错误信息已补充：

```text
pixelIndex；
R/G/B/W/S/V 六通道值；
model/modelVarnish/outerVarnish/support ownership。
```

该诊断不改变协议或材料优先级。

## 3. 输出合同

每个正向 Case 只生成一个场景 Package：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
sampleFormat = uint
polarity = black_is_print
printValue = 0
emptyValue = 255
```

矩阵报告：

```text
schema = slicesoft.multimodel_scene_matrix.13b.1
real_model_matrix.json
real_model_matrix.md
```

报告记录模型格式、实例数、生产器/复用次数、Grid、Package、RIP、分阶段耗时、峰值内存、
正负向状态和 production blockers。

## 4. Release 功能矩阵

2026-07-28 在 MSVC Release 下实测：

| Case | 实例 | 生产器 | 复用 | Grid | 总耗时 | Package | 结果 |
|---|---:|---:|---:|---|---:|---:|---|
| 13B-M01 | 1 | 1 | 0 | 61x117x28 | 0.68 s | 1.25 MB | PASS |
| 13B-M11 | 11 | 1 | 10 | 1661x117x28 | 5.34 s | 32.71 MB | PASS |
| 13B-M12 | 12 | 2 | 10 | 1661x416x29 | 12.06 s | 120.30 MB | PASS |
| 13B-M22 | 22 | 2 | 20 | 1661x416x29 | 14.61 s | 120.31 MB | PASS |
| 13B-M3F | 2 | 2 | 0 | 176x117x28 | 1.06 s | 3.51 MB | PASS |

22 实例 Release 分段耗时：

```text
import：227.01 ms；
layout：0.01 ms；
preflight/admission：7045.16 ms；
slice：343.80 ms；
compose：387.50 ms；
TIFF + report write：3655.26 ms；
RIP validation：2880.23 ms；
total：14611.34 ms；
peak working set：243830784 bytes。
```

这些数据说明重复实例局部切片复用有效；当前主要耗时已转移到逐实例几何预检、TIFF/报告写入和
RIP 校验。该功能 Profile 不是设备生产 SLA，不能据此直接确定 22 实例生产预算。

## 5. 负向矩阵

以下输入均按预期阻断：

| Case | 预期错误码 | 结果 |
|---|---|---|
| 23 实例 | `LAYOUT_INSTANCE_CAPACITY_EXCEEDED` | PASS |
| 投影重叠 | `SCENE_INSTANCE_OVERLAP_BLOCKED` | PASS |
| 幅面越界 | `SCENE_INSTANCE_OUT_OF_RANGE` | PASS |
| 缺失 buildVolume | `SCENE_BUILD_VOLUME_UNDEFINED` | PASS |
| 过期 scene revision | `SCENE_REVISION_STALE` | PASS |

负向 Case 不生成伪成功 Package。

## 6. 验证证据

实际执行：

```powershell
cmake --build build --config Debug --target multi_model_scene_matrix rip_reader_test
ctest --test-dir build -C Debug -R "^(multimodel_scene_contract_unit_tests|multi_model_scene_matrix_report_unit_tests|translated_scene_raster_reuse_unit_tests|grid_layout_policy_unit_tests|scene_collision_admission_unit_tests|scene_layer_adapters_unit_tests|multi_model_layer_composer_unit_tests|multi_model_package_writer_unit_tests)$" --output-on-failure

cmake --build build --config Release --target multi_model_scene_matrix rip_reader_test
powershell -ExecutionPolicy Bypass -File scripts/run_13b_07_real_model_matrix.ps1 -BuildDir build -Config Release -OutputDir output/benchmarks/13b_07/release -SkipBuild

powershell -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1 -BuildDir build -Config Debug
ctest --test-dir build -C Debug --output-on-failure
```

结果：

```text
定向 CTest：8/8 PASS；
Debug 全量 CTest：72/72 PASS；
Debug 真实模型正向矩阵：5/5 PASS；
Debug 负向矩阵：5/5 按预期 BLOCK；
Debug 五个正向 Package：RIP strict PASS；
Release 正向矩阵：5/5 PASS；
Release 负向矩阵：5/5 按预期 BLOCK；
Release 五个正向 Package：RIP strict PASS；
Quick CI：PASS，包含全量 Debug build、quick regression、golden、UI self-test 和真实 Overlay smoke。
```

## 7. 与俯视排版界面的关系

13B-04A 已完成全部 visible 实例的统一 +Z 俯视显示、OBJ/MTL/贴图采样、追加模型自动排版和
场景选择框。13B-05 至 13B-07 继续使用同一 scene/model/instance/transform identity：

```text
俯视排版所见实例；
逐实例 admission；
局部 Raster；
全局联合层；
单一 Package；
scene report。
```

因此，不再使用“只显示一个模型但后台切多个模型”的双重状态。Qt 一键联合切片接线仍不属于
13B-07，本轮 Runner 证明的是核心功能链和真实模型矩阵。

## 8. 未完成与风险

仍未关闭：

```text
真实设备 buildVolume；
设备原点和 X/Y 轴方向；
22 实例生产性能预算；
Qt 一键多模型联合切片接线；
跨实例联合支撑；
mixed-profile / mixed Legacy-Global 场景；
Global/OpenVDB 多模型矩阵；
aishen/meigui/titian 复杂浮雕 strict 资产覆盖仍为 0/3；
13C TIFF 原生统一预览。
```

## 9. 阶段判断与下一步

13B-07 功能开发和验证已经收口，Stage 13B 可标记：

```text
FUNCTIONAL MATRIX COMPLETE；
M13-3 FUNCTIONAL CANDIDATE PASS；
PRODUCTION INPUT OPEN。
```

固定执行顺序进入 `13C-01`，实现 TIFF Layer Source 与 LRU Cache。设备输入和 22 实例预算
到位后，再独立评审 Stage 13B production GO，不回写或伪造本轮功能 fixture 结论。
