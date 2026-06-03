# DEV_00_单材料体素切片引擎与RGBWSV多通道TIFF输出设计

> 版本：v0.2  
> 状态：Draft / P0 Demo  
> 类型：DEV

## 1. 架构

```text
slice_config.json
→ ModelLoader
→ ModelNormalizer
→ MeshRepairLite
→ Voxelizer
→ BottomProjectionSupportGenerator
→ LayerSampler
→ LayerChannelComposer
→ PrivateTiffWriter
→ ManifestWriter
→ ReportWriter
```

## 2. Targets

```text
slicer_core
slicer_cli
rip_reader_test
slicer_qt_demo later
```

## 3. 通道规则

通道顺序：

```text
R G B W S V
```

模型像素：

```text
R/G/B = 配置文件指定的单材料 RGB
W = configured white strength
S = 0
V = configured varnish strength
```

支撑像素：

```text
R/G/B/W/V = 0
S = configured support strength
```

优先级：

```text
Model > Support
```

## 4. 第一版实现策略

第一版可以先使用 dense / column-based prototype，优先证明 TIFF/RIP 数据闭环。

后续再替换为 OpenVDB / SDF-based Level Set 正式体素内核。

## 5. P0 工程形态

```text
slicer_core      C++ 切片核心库
slicer_cli       命令行验证入口
rip_reader_test  RIP 输入验证工具
slicer_qt_demo   Qt 调试与预览壳，后续实现
```

## 6. 第三方依赖建议

P0 初始依赖：

```text
nlohmann-json
libtiff
assimp
```

后续评估：

```text
OpenVDB
CGAL
lib3mf
Qt 5.15
OpenCV
```
