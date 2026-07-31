# DEMO_12E-09D 生产纹理厚度与单材料材质验证方案

> 文档状态：VALIDATION PLAN READY
> 日期：2026-07-31

## 1. Legacy 矩阵

对同一闭合 fixture：

```text
topSurfaceLayers=1；
topSurfaceLayers=3；
topSurfaceLayers=10。
```

验证：

```text
session config 值正确；
effective thickness = layers * layerThicknessMm；
RGB 生效层数单调增加；
ModelFill/S 语义不被意外修改；
RIP strict PASS。
```

## 2. Global 矩阵

使用已准入 xiao_ma/yecan：

```text
minimum width；
middle width；
all_texture。
```

验证：

```text
requested/effective/backend；
TextureSurface coverage 单调；
ModelFill coverage 反向单调；
all_texture ModelFill=0；
full closure PASS；
RIP strict PASS。
```

不使用 aishen/meigui/titian 冒充 strict Global PASS。

## 3. 单材料矩阵

使用 `samples/models/relief/relief_nail_arched.obj`：

| Case | 材料 | 期望 |
|---|---|---|
| 09D-W | 白墨 | W 打印、V 空、S 按支撑 |
| 09D-V | 光油 | V 打印、W 空、S 按支撑 |

比较：

```text
model occupancy；
layer count；
model silhouette；
support mask；
W/V channel stats；
manifest/报告；
RIP strict。
```

## 4. UI 矩阵

```text
诊断滑块变化 -> production config hash 不变；
生产控件变化 -> 当前 package stale；
保存/回读 -> 值一致；
一键切片 -> 使用新 effective config；
不支持控件 -> 锁定并说明原因；
切换 Profile -> 不把上一个 Profile 值泄漏到当前 Profile。
```

## 5. 目标命令

实现后目标入口：

```powershell
cmake --build build-slicesoft/main --config Debug --target `
  production_texture_settings_model_unit_tests `
  single_material_relief_resolver_unit_tests `
  production_effective_config_unit_tests `
  slicer_debug_ui rip_reader_test

ctest --test-dir build-slicesoft/main -C Debug `
  -R "(production_texture|single_material_relief|production_effective_config)" `
  --output-on-failure

.\scripts\run_12e_09d_production_texture_material_matrix.ps1 `
  -BuildDir build-slicesoft/main -Config Release
```

当前准备阶段不得宣称这些新增入口已经存在或运行。

## 6. 完成标准

```text
代码、单测、UI Smoke、真实/fixture package 和 RIP 全部通过；
09A 诊断回归不变；
09D REPORT 给出 requested/effective 和实际通道证据；
12E-10A 的前置 Gate 更新为 09D COMPLETE。
```
