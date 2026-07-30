# DEMO_13G 支撑投影铺底与层间连续性验证方案

> 文档状态：DEMO / PASS
> 版本：v1.1
> 日期：2026-07-30

## 1. 验证目标

```text
证明 Reality 模型异常来自 Z 正反面而非层序；
证明修正后甲片外表面朝 +Z、尖端朝 +Y、minZ=0；
证明最大支撑投影只作用于前 N 层；
证明铺底、内部支撑和普通 lower 支撑来源可区分；
证明生产 TIFF/RIP 协议不变。
```

## 2. Fixture

```text
生成式 face-up/face-down arched nail；
生成式分层支撑 footprint；
model/obj/reality/segment_105 作为首个真实 Release 样例；
Reality 其余四个模型只读方向矩阵；
12A internal void fixture；
单材料与纹理配对 fixture。
```

## 3. 核心用例

| Case | 配置 | 预期 |
|---|---|---|
| G13-01 | face-down synthetic nail | 自动翻转，frontUp=true |
| G13-02 | face-up synthetic nail | 姿态保持，frontUp=true |
| G13-03 | autoOrient=false | 不翻转，报告 explicit_disabled |
| G13-04 | base disabled | 与既有 TIFF 一致 |
| G13-05 | base enabled, N=30 | 0..29 有铺底，第 30 层无仅铺底 S |
| G13-06 | model 与铺底冲突 | Model 胜出 |
| G13-07 | Reality segment_105 | 层序低到高、姿态正确、RIP PASS |
| G13-08 | 单材料/纹理配对 | 支撑 mask 一致 |

## 4. Reality 复测观察点

复测前后都记录：

```text
selectedOrientation / rotationDeg；
frontOrientation / frontOrientationAdjusted；
orientedBbox minZ/maxZ；
layer 0/10/20/21/29/30 的 model、S、internalVoid、projectionBase；
支撑总像素；
TIFF 保存和核心切片耗时；
RIP Reader summary。
```

不能只凭 UI 伪彩截图判定通过。

## 5. 负向用例

```text
baseProjection.layerCount < 0；
baseProjection.layerCount > 1000；
未知 source；
近球体/立方体 frontOrientation=indeterminate；
取消切片时不得留下可加载的完整 Package；
配置启用铺底但 support.enabled=false 时 effectiveEnabled=false。
```

## 6. 验收命令

当前冻结的最低验证集：

```powershell
cmake --build build-slicesoft/main --config Debug --target auto_orient_unit_tests
ctest --test-dir build-slicesoft/main -C Debug -R "auto_orient|support" --output-on-failure
.\scripts\run_ci_quick.ps1
.\scripts\run_13g_support_base_projection_tests.ps1 `
  -BuildDir build-slicesoft/main -Configuration Debug
.\runtime\slicesoft\Release\rip_reader_test.exe --package <segment_105-package> --summary
```

## 7. PASS 条件

```text
方向、铺底边界、材料优先级和来源统计全部通过；
Reality 单模型无反向层序错觉；
30 层定义无 off-by-one；
旧 fixture 兼容；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 不变。
```
