# DEV_03B_v0.2_TIFFStorageModeWriterReader设计

> 文档版本：v0.2  
> 文档状态：Draft / DEV  
> 适用阶段：REPORT_05 之后 / 03B  
> 所属模块：TIFF Writer / RIP Reader / Manifest  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

新增 TIFF StorageMode 抽象：

```text
stripped
tiled
```

并让 Writer / Reader / Manifest / Regression 全部识别该模式。

03B 不改变 `MaterialPolicy` 的 RGB/W/V 逻辑，只改变 layer TIFF 的物理存储方式。

---

## 2. 配置结构

建议扩展 OutputConfig：

```cpp
enum class TiffStorageMode {
    Stripped,
    Tiled
};

struct OutputConfig {
    TiffStorageMode storage_mode{TiffStorageMode::Stripped};
    int rows_per_strip{64};
    std::array<int, 2> tile_size{256, 256};
};
```

JSON：

```json
{
  "output": {
    "storageMode": "stripped",
    "rowsPerStrip": 64
  }
}
```

兼容旧配置：

```json
{
  "output": {
    "tiled": true,
    "tileSize": [256, 256]
  }
}
```

解析优先级：

```text
1. 如果 storageMode 存在，以 storageMode 为准；
2. 如果 storageMode 不存在，但 tiled=true，则 storageMode=tiled；
3. 如果两者都不存在，则 storageMode=stripped。
```

---

## 3. Manifest Writer

03B 默认：

```text
schema = p0.rgbwsv.2
```

stripped：

```json
{
  "schema": "p0.rgbwsv.2",
  "tiff": {
    "storageMode": "stripped",
    "tiled": false,
    "rowsPerStrip": 64
  }
}
```

tiled：

```json
{
  "schema": "p0.rgbwsv.2",
  "tiff": {
    "storageMode": "tiled",
    "tiled": true,
    "tileSize": [256, 256]
  }
}
```

Reader 仍要接受：

```text
schema = p0.rgbwsv.1
```

作为 legacy tiled。

---

## 4. TIFF Writer

### 4.1 Writer 分流

```cpp
void write_layer_tiff(...) {
    if (config.output.storage_mode == TiffStorageMode::Stripped) {
        write_stripped_tiff(...);
    } else {
        write_tiled_tiff(...);
    }
}
```

### 4.2 Stripped Writer

核心 tag：

```cpp
TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 6);
TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rows_per_strip);
```

写入：

```cpp
const std::size_t row_bytes = width * 6;

for (std::uint32_t y = 0; y < height; ++y) {
    const auto* row = layer.data() + y * row_bytes;
    TIFFWriteScanline(tif, const_cast<std::uint8_t*>(row), y, 0);
}
```

### 4.3 Tiled Writer

保留当前 tiled writer。

必须继续保证：

```text
tile padding = 255
```

---

## 5. RIP Reader

### 5.1 Reader 分流

```cpp
if (TIFFIsTiled(tif)) {
    return read_tiled_tiff(...);
}
return read_scanline_tiff(...);
```

注意：

```text
不要根据 manifest.storageMode 盲读。
应以 TIFFIsTiled(tif) 为实际文件结构依据。
manifest.storageMode 用于一致性校验。
```

### 5.2 Scanline Reader

```cpp
for y in 0..height-1:
    TIFFReadScanline(tif, row_buffer.data(), y, 0)
    copy row_buffer to layer buffer
```

### 5.3 Tiled Reader

使用：

```cpp
TIFFReadEncodedTile
```

边缘裁剪：

```text
copyWidth = min(tileWidth, width - tileX)
copyHeight = min(tileHeight, height - tileY)
```

---

## 6. Reader 校验

新增校验：

```text
manifest.tiff.storageMode 是否为 stripped / tiled
manifest.tiff.tiled 是否与 storageMode 一致
实际 TIFFIsTiled 是否与 manifest 一致
rowsPerStrip > 0
tileSize > 0
```

兼容：

```text
p0.rgbwsv.1 无 storageMode 时，按 legacy tiled 处理。
```

---

## 7. 错误码建议

新增：

```cpp
TiffStorageModeInvalid
TiffStorageMismatch
RowsPerStripInvalid
TileSizeInvalid
```

错误字符串建议：

```text
E_TIFF_STORAGE_MISMATCH: manifest.tiff.storageMode expected stripped, actual TIFFIsTiled=true
```

---

## 8. MaterialPolicy Regression

03B 修改后，`run_regression.ps1` 必须继续校验：

```text
RGB only: RGB > 0, W = 0, V = 0
RGB + W: RGB > 0, W > 0, V = 0
RGB + V: RGB > 0, W = 0, V > 0
RGB + W + V: RGB/W/V > 0
V only: RGB = 0, W = 0, V > 0
W only: RGB = 0, W > 0, V = 0
```

并额外校验：

```text
stripped 默认 package 可读
tiled compatibility package 可读
```

---

## 9. 实施顺序

```text
1. OutputConfig 增加 storageMode / rowsPerStrip
2. 配置解析兼容 tiled 旧字段
3. TIFFWriter 增加 stripped writer
4. Manifest 写 p0.rgbwsv.2 + storageMode
5. RIP Reader 增加 stripped/tiled 双读取
6. Reader 增加 storage mismatch 校验
7. 新增 good_stripped / good_tiled 样例
8. 新增 bad storage package
9. run_regression.ps1 更新 MaterialPolicy + storageMode 覆盖
10. REPORT_03B
```

---

## 10. 不做内容

03B 不做：

```text
MaterialPolicy 逻辑变更
Texture sampling 逻辑变更
RIP 半色调
OpenVDB
Qt UI
3MF
```

---

## 11. 结论

03B 的代码核心是：

```text
StorageMode 配置化
Writer 双路径
Reader 双路径
Manifest 显式记录
Regression 双模式覆盖
MaterialPolicy 语义不变
```
