# DEV_00A_P0Demo增强_预览导入与几何鲁棒性设计

> 文档版本：v0.1  
> 文档状态：Draft / P0+ 技术设计  
> 所属模块：切片软件 / Slicer  
> 输入依据：`REPORT_00_P0_Demo当前实现状态.md`

---

## 1. 当前实现基线

当前工程已经实现：

```text
config.h/cpp
json_value.h/cpp
model.h/cpp
slicer.h/cpp
tiff_io.h/cpp
rip_reader.h/cpp
slicer_cli
rip_reader_test
```

核心链路：

```text
配置读取
→ STL/OBJ 基础导入
→ 模型变换/autoOrient
→ 三角面 Z 截面
→ scanline fill
→ bottom projection support
→ RGBWSV compose
→ internal TIFF writer
→ manifest/reports
→ rip_reader_test
```

P0+ 不重写架构，只在现有架构上增强。

---

## 2. 设计目标

P0+ 技术目标：

```text
增强 preview
增强模型导入
增强几何采样诊断
增强 reports
补充测试样例和验证流程
```

非目标：

```text
不替换为 OpenVDB
不引入全彩纹理采样
不重构为 Qt UI
不引入 VTK/Unity
```

---

## 3. 模块级增强设计

### 3.1 PreviewExporter

当前 preview 为 PPM P6。

建议新增：

```text
preview_writer.h
preview_writer.cpp
```

接口草案：

```cpp
enum class PreviewFormat {
    Ppm,
    Png
};

struct PreviewRequest {
    int layerIndex;
    std::string channelName;
    PreviewFormat format;
    std::filesystem::path outputPath;
};

class PreviewWriter {
public:
    static void write_rgb_preview(const LayerBuffer& layer, const PreviewRequest& request);
    static void write_single_channel_preview(const LayerBuffer& layer,
                                             Channel channel,
                                             const PreviewRequest& request);
};
```

PNG 写入路线：

```text
优先：使用 OpenCV imwrite
备选：使用轻量 stb_image_write
```

当前 P0+ 建议：

```text
若不希望引入 OpenCV，可先引入 stb_image_write 单头文件。
```

### 3.2 PreviewReport

增强 `preview_report.json`：

```json
{
  "format": "png",
  "interval": 10,
  "generated": [
    {
      "layerIndex": 10,
      "channel": "support",
      "path": "preview/support_s_000010.png",
      "nonZeroPixels": 18233,
      "maxValue": 65535
    }
  ]
}
```

---

## 4. 模型导入增强设计

### 4.1 Binary STL

当前已支持 ASCII STL。P0+ 增加 Binary STL。

识别策略：

```text
读取文件前 80 bytes header
读取 uint32 triangleCount
判断文件大小是否等于 84 + triangleCount * 50
若匹配则按 binary STL 读取
否则尝试 ASCII STL
```

Binary STL triangle 结构：

```text
normal: float32[3]
v0: float32[3]
v1: float32[3]
v2: float32[3]
attributeByteCount: uint16
```

### 4.2 OBJ MTL 基础读取

当前 OBJ 支持 `v` 和 `f`。P0+ 增加：

```text
mtllib
usemtl
```

P0+ 不需要解析贴图，只需要：

```text
记录 material name
统计每个 material 下的 face 数
写入 model_report.json
```

### 4.3 ModelReport 增强

建议字段：

```json
{
  "format": "obj",
  "vertexCount": 0,
  "faceCount": 0,
  "triangleCount": 0,
  "materialCount": 0,
  "materials": [
    {
      "name": "default",
      "faceCount": 0,
      "triangleCount": 0
    }
  ],
  "bboxOriginal": {},
  "bboxOriented": {},
  "warnings": []
}
```

---

## 5. 几何采样鲁棒性设计

### 5.1 当前风险

当前采用：

```text
Triangle mesh
→ Z plane intersection
→ 2D segment list
→ scanline fill
→ model mask
```

该路线在以下场景可能不稳定：

```text
退化三角形
切片平面刚好经过顶点
多个轮廓
孔洞
非闭合模型
自交模型
极薄模型
```

### 5.2 P0+ 增强策略

不要求一次解决所有几何问题，但必须增加诊断能力。

新增 `contour_report.json`：

```json
{
  "layers": [
    {
      "layerIndex": 100,
      "zMm": 1.005,
      "segmentCount": 120,
      "openSegmentWarnings": 0,
      "fillWarnings": [],
      "modelNonZeroPixels": 12345,
      "supportNonZeroPixels": 6789
    }
  ]
}
```

### 5.3 退化三角形处理

在采样前过滤：

```text
面积小于 epsilon 的三角形
重复顶点三角形
z 范围不覆盖当前 layer 的三角形
```

建议配置：

```json
{
  "geometry": {
    "epsilonMm": 1e-6,
    "enableDegenerateTriangleFilter": true
  }
}
```

### 5.4 Z 平面顶点相交策略

切片平面穿过三角形顶点时，容易产生重复交点。

建议：

```text
使用 half-open interval 规则
zMin <= z < zMax
```

避免相邻三角形重复贡献边。

---

## 6. Reports 增强

当前 reports：

```text
model_report.json
slice_report.json
repair_report.json
support_report.json
preview_report.json
```

新增：

```text
contour_report.json
```

增强 `slice_report.json`：

```json
{
  "layerCount": 0,
  "widthPx": 0,
  "heightPx": 0,
  "layers": [
    {
      "layerIndex": 0,
      "zMm": 0.005,
      "modelNonZeroPixels": 0,
      "supportNonZeroPixels": 0,
      "whiteNonZeroPixels": 0,
      "varnishNonZeroPixels": 0
    }
  ]
}
```

---

## 7. 测试设计

### 7.1 样例模型

建议新增：

```text
samples/models/cube_ascii.stl
samples/models/cube_binary.stl
samples/models/sphere_ascii.stl
samples/models/thin_plate.stl
samples/models/hole_test.obj
samples/models/multi_material_stub.obj
```

### 7.2 测试用例

```text
ASCII STL 导入
Binary STL 导入
OBJ fan triangulation
OBJ usemtl 统计
bottom support 非零
RGBWSV checksum
缺失 layer 报错
错误 channelOrder 报错
preview PNG 生成
```

---

## 8. 不建议在 P0+ 做的改动

```text
不要重构为 OpenVDB
不要引入完整 Assimp 替换当前 loader，除非作为独立可选路径
不要改变 manifest schema 主版本
不要改变 RGBWSV 通道顺序
不要引入 Qt UI 作为阻塞项
```

---

## 9. 完成标准

P0+ 完成后，应达到：

```text
真实模型验证更容易
preview 更容易查看
模型导入更稳
几何错误更容易定位
reports 更适合交给 Codex/工程师继续迭代
```
