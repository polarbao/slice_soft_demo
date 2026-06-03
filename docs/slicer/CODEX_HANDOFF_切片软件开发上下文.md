# CODEX_HANDOFF_切片软件开发上下文

## 1. 当前项目目标

本仓库是 UV 3D 打印切片软件的 P0 Demo 仓库。当前目标是验证从单材料模型到 RIP 输入数据包的最小闭环。

P0 最小闭环：

```text
STL / OBJ 单模型输入
→ C++ 模型导入
→ 配置化模型变换
→ 体素/层采样
→ 普通模型下表面投影支撑
→ RGBWSV 六通道 TIFF
→ manifest.json
→ rip_reader_test 验证
```

## 2. 和现有打印软件的关系

现有 UV 打印软件已经具备部分 RIP 后能力，例如：

```text
通道化处理
板卡控制
运动控制
打印任务执行
设备诊断
维护服务
```

本仓库只负责上游切片 Demo，不直接处理板卡、运动控制和打印执行。

## 3. P0 冻结事项

```text
开发语言：C++20
构建系统：CMake
包管理：vcpkg
默认 DPI：600
默认层厚：0.01mm
TIFF 通道顺序：R G B W S V
TIFF 位深：uint16
TIFF 存储：tiled
TIFF planarConfig：contiguous
P0 支撑策略：bottom_projection
正式验收对象：C++ slicer_cli + rip_reader_test
```

## 4. P0 非目标

P0 不做：

```text
全彩纹理采样
3MF
glTF / GLB
多材料切片
复杂支撑树
上表面牺牲层
侧壁辅助支撑
Unity 集成
VTK 集成
完整 Qt UI
GPU 加速
```

## 5. 推荐实现顺序

不要先做 UI。

推荐顺序：

```text
1. slicer_core
2. slicer_cli
3. rip_reader_test
4. 可选 Qt layer/channel preview
5. 可选 QOpenGLWidget mesh preview
```

## 6. P0 可用简化算法

P0 可以先使用 column-based sampler，而不是直接实现完整 OpenVDB SDF。

建议第一版算法：

```text
for each XY pixel:
  沿 Z 方向采样或投射
  得到模型占据区间
  填充模型 layer mask
  找到 zMin
  从 build plate 到 zMin 生成下表面投影支撑
```

后续可以把 `Voxelizer` 替换为 OpenVDB / SDF-based Level Set。

## 7. RGBWSV 语义

模型像素：

```text
R = 配置 RGB R
G = 配置 RGB G
B = 配置 RGB B
W = 配置白墨强度
S = 0
V = 配置光油强度
```

支撑像素：

```text
R = 0
G = 0
B = 0
W = 0
S = 配置支撑强度
V = 0
```

优先级：

```text
Model > Support
```

## 8. rip_reader_test 必须验证

```text
manifest 存在
channelOrder == ["R", "G", "B", "W", "S", "V"]
bitDepth == 16
planarConfig == contiguous
所有 layer 文件存在
所有 layer 尺寸一致
可以打印每个通道 checksum
```
