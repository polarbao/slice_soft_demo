# DEMO_13B 多模型规则排版与联合切片验证方案

> 文档版本：v0.1
> 文档状态：Formal DEMO / PREPARED
> 生成日期：2026-07-24

## 1. 验证目标

验证多个实例能够按 11x2 规则排版、通过逐实例准入、映射到同一全局 raster，并生成一个严格可读的
RGBWSV package。

## 2. Fixture

正向模型优先使用：

```text
model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj；
model/obj/yecan/3.obj；
samples/models/3mf/texture2d_checker_cube.3mf。
```

需增加小型确定性 fixture：

```text
closed_box_a.obj；
closed_box_b.obj；
同名但不同内容的 texture/resource scope；
越界 scene；
重叠 scene；
23-instance scene；
blocked geometry scene。
```

## 3. 排版矩阵

| Case | 实例数 | 预期 |
|---|---:|---|
| 13B-L01 | 1 | 第一行第一列 |
| 13B-L02 | 11 | 第一行填满 |
| 13B-L03 | 12 | 第 12 个进入第二行 |
| 13B-L04 | 22 | 两行填满 |
| 13B-L05 | 23 | `INSTANCE_LIMIT_EXCEEDED` |
| 13B-L06 | 4，间距 20/30 | 边到边净距精确 |
| 13B-L07 | 4，自定义间距 | 保存、回读和重排一致 |

## 4. 场景交互

```text
添加不同模型；
确认全部 visible 模型及其贴图同时出现在统一 +Z 俯视画布；
确认追加导入自动按当前规则分开；
复制同一模型；
删除实例；
隐藏和锁定；
自动排列；
手工移动一个实例；
mirrorX/mirrorY；
重新排列前确认 override 提示；
保存和重新加载 scene。
```

## 5. 负向 Gate

```text
模型重叠；
模型超出 buildVolume；
buildVolume 缺失；
资源 scope 缺失；
一个实例 confirmed self-intersection；
不同实例 DPI/layerHeight 冲突；
写包中途失败；
用户取消。
```

所有负向用例不得留下可被加载为成功的最终 package。

## 6. 联合切片验证

```text
确认所有实例共享一个 layerIndex/zMm 序列；
确认每层只有一个 TIFF；
确认 TIFF 尺寸等于全场景 raster；
确认每个实例出现在正确 XY 区域；
确认实例间净距区域六通道均为 255；
确认各实例 RGB/W/S/V 语义不串；
确认 scene report per-instance 统计与全局统计可对账；
确认 RIP strict 通过。
```

## 7. 性能矩阵

在 Release 下记录：

```text
1/11/12/22 实例；
导入；
资源缓存；
排版；
preflight；
core slice；
scene compose；
TIFF write；
report write；
peak memory。
```

同一 modelId 的多实例必须单独记录资源复用率。预览 PNG 写盘不计入 core slice。

## 8. UI Smoke

计划增加：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-grid-layout
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-blocked-overlap
```

## 9. 回归命令

计划命令：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
.\build\Debug\rip_reader_test.exe --package <scene-package> --summary
git diff --check
```

本文只定义未来验证，不宣称当前命令已经具备对应 case 或已通过。

## 10. 通过标准

```text
11x2 规则、间距、镜像和实例变换全部可审计；
碰撞/越界/blocked fail-closed；
一个场景只发布一个 package；
RGBWSV 协议不变；
RIP strict PASS；
单模型生产入口回归 PASS。
```
