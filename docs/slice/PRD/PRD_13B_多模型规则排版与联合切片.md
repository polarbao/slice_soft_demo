# PRD_13B 多模型规则排版与联合切片

> 文档版本：v0.1
> 文档状态：Formal PRD / PREPARED
> 生成日期：2026-07-24

## 1. 背景

当前生产配置只接收一个 `input.modelPath`。用户需要一次导入多个模型，在同一打印版面完成规则排布，
并输出一个可供后续 RIP 使用的 RGBWSV package。

## 2. 产品目标

```text
一次导入多个 OBJ/STL/3MF；
全部 visible 实例在同一 +Z 俯视画布中显示，并保留可用贴图颜色；
追加导入后默认执行当前规则排版，避免实例在同一原点完全重叠；
模型列表可选择、复制、删除、隐藏和锁定；
最多 11 列、2 行、22 个实例；
默认列间净距 10.00 mm；
默认行间净距 10.00 mm；
间距可在 UI 配置；
排版后可对单个实例移动、rotateZ、uniformScale、mirrorX/mirrorY；
统一切片并输出一个 p0.rgbwsv.2 package；
报告可定位每个 modelId/instanceId。
```

## 3. 范围定义

### 3.1 多模型

多模型既支持不同源文件，也支持同一模型复制多个实例：

```text
一个源文件对应一个 modelId；
一次摆放对应一个 instanceId；
多个 instanceId 可引用相同 modelId；
资源缓存不得因文件同名跨 modelId 污染。
```

### 3.2 规则排版

P0 排版是确定性规则网格，不是自由形状 nesting：

```text
row_major；
每行最多 11 个；
最多 2 行；
默认先填满第一行；
列间距和行间距按变换后 XY 包围盒边缘计算；
用户可在自动排版后继续手工调整。
```

### 3.3 联合切片

联合切片必须输出：

```text
一个 session；
一个 effective scene config；
一个 package；
每个 layerIndex 一个 RGBWSV TIFF；
一个 manifest；
scene report、slice report、support/material reports；
RIP strict 可读取。
```

不得把“逐个模型分别生成 package”显示为联合切片成功。

## 4. UI 需求

### 4.1 模型列表

每项显示：

```text
名称；
modelId/instanceId 简短标识；
格式；
尺寸；
位置；
旋转；
缩放；
镜像；
可见/锁定；
几何准入；
资源状态。
```

### 4.2 排版设置

```text
最大列数：1..11，默认 11；
最大行数：1..2，默认 2；
列间净距：默认 10.00 mm，步长 0.01 mm；
行间净距：默认 10.00 mm，步长 0.01 mm；
排列按钮；
居中到打印幅面；
恢复排版前位置；
碰撞/越界状态。
```

### 4.3 模型操作

```text
添加；
复制；
删除；
选择；
锁定；
隐藏；
镜像 X/Y；
移动 X/Y；
绕 Z 旋转；
统一缩放；
重置。
```

## 5. 支撑和材料

```text
每个实例使用明确的材料/Profile 绑定；
P0 默认要求场景内 layerHeight、dpiX、dpiY 一致；
在多 Profile 产品规则确认前，P0 使用 scene_profile_only，所有实例共享同一 MaterialProcessProfile；
请求不同 Profile 时 fail-closed，不做隐式材料合并；
每个实例独立生成 lower/internal-void/upper 支撑语义；
P0 不做跨实例联合支撑树或跨模型支撑优化；
场景合成保持 Model > OuterVarnishShell > Support > Empty；
模型重叠直接阻断，不进入材料冲突合成。
```

## 6. 镜像

镜像属于实例变换，必须：

```text
记录 mirrorX/mirrorY；
在画布即时更新；
重新执行 transformed preflight；
保持纹理资源和 UV 可追溯；
保存到 scene effective config；
进入联合切片实际几何。
```

## 7. 构建幅面与越界

正式 Profile 必须提供打印幅面。若幅面未知：

```text
允许编辑和预览；
允许保存 scene draft；
禁止标记 production ready；
禁止生成可被误认成正式成功的 package。
```

## 8. 非目标

```text
不实现任意形状最优 nesting；
不实现跨模型联合支撑优化；
不实现模型布尔合并；
不允许模型重叠打印；
不允许不同实例使用不同 DPI 或 layerHeight；
不改变 RGBWSV 通道和 TIFF 格式；
不实现 RIP、半色调和设备通信。
```

## 9. 验收标准

```text
1、11、12、22 个实例均可确定性排版；
第 12 个实例进入第二行；
第 23 个实例被明确阻断；
10/10 mm 默认净距在 effective scene config 和画布中一致；
修改间距后排版结果可重复；
镜像、移动、旋转、缩放进入实际切片；
越界和碰撞阻断生产；
不同 modelId 的同名 MTL/texture 不串资源；
联合 package 每层只有一个 TIFF；
scene report 能按 instanceId 统计；
RIP strict 通过；
单模型配置和生产路径保持回归。
```

## 10. 待设备确认

```text
正式 buildVolume.widthMm/heightMm；
场景原点位置；
机器 X/Y 正方向；
允许的最大模型数是否长期固定 22；
不同模型是否允许不同 MaterialProcessProfile；
正式多模型性能预算。
```
