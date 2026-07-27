# DEMO_13A 模型俯视与实例变换验证方案

> 文档版本：v0.1
> 文档状态：Formal DEMO / PREPARED
> 生成日期：2026-07-24

## 1. 验证目标

验证模型在切片前可见、可选择、可精确变换，并且 UI 显示、effective config、几何准入和最终切片使用
同一实例变换。

## 2. 验证模型

优先使用当前 strict PASS 资产：

```text
model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj；
model/obj/yecan/3.obj；
samples/models/3mf/texture2d_checker_cube.3mf。
```

复杂浮雕 confirmed self-intersection 模型只用于 blocked 显示，不作为变换生产 PASS。

## 3. 用例

### Case 13A-01 默认俯视

```text
导入模型；
检查 XY 轴、网格、轮廓和包围盒；
比较 UI 尺寸与 SceneSummary；
确认 Z 不可编辑；
确认默认切片姿态未改变。
```

### Case 13A-02 移动与旋转

```text
设置 X=12.34 mm、Y=-5.67 mm、rotateZ=30.0；
确认画布即时更新；
保存并回读 session config；
运行切片；
比较 package 物理位置与 effective transform。
```

### Case 13A-03 缩放

```text
设置 80%、100%、120%；
确认 XY 尺寸按比例变化；
确认 SourceTransform 的 minZ 基准保持不变，不发生二次落台；
0、负数和超范围被拒绝。
```

### Case 13A-04 镜像

```text
分别执行 mirrorX、mirrorY 和双镜像；
检查 UI 形状；
检查 winding/normal/UV；
重新运行 strict preflight；
纹理模型确认 UV 没有丢失。
```

### Case 13A-05 选择隔离

使用两个 fixture 实例：

```text
选中 A 只修改 A；
锁定 B 后不可变换；
切换选择不串写；
重置 A 不影响 B。
```

### Case 13A-06 阻断

```text
导入 confirmed self-intersection 模型；
允许只读查看；
显示明确 blocked；
切片入口保持禁用；
不得显示 production PASS。
```

### Case 13A-07 UI 尺寸

```text
1280x720；
1440x900；
1920x1080；
最长中文、DPI 缩放和模型长文件名；
控件不遮挡，数值不截断。
```

## 4. 自动化验证

计划命令：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view-transform
.\scripts\run_ci_quick.ps1
git diff --check
```

命令在实现任务中建立后才能运行；本文不宣称当前已通过。

## 5. 通过标准

```text
显示坐标与切片坐标一致；
变换保存/回读/重置一致；
镜像后 strict 结果可信；
Z 落台不回归；
blocked 模型不能生产；
UI 无明显遮挡；
协议回归通过。
```
