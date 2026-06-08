# RIP_COMPATIBILITY_CHECKLIST_RGBWSV_TIFF

> 适用协议：`p0.rgbwsv.2`  
> 适用阶段：03C 后真实 RIP 对接前检查  
> 目标：确认目标 RIP 能正确读取当前 RGBWSV TIFF 包。

---

## 1. Manifest Schema

- [ ] RIP 接受 `schema = p0.rgbwsv.2`。
- [ ] 如需兼容历史包，RIP 接受 `schema = p0.rgbwsv.1` legacy tiled。
- [ ] RIP 不接受未知 schema，并能返回明确错误。

## 2. Channel Protocol

- [ ] `channelOrder = R G B W S V`。
- [ ] `channelCount = 6`。
- [ ] RGB 为模型颜色通道。
- [ ] W 为白墨通道。
- [ ] S 为支撑通道。
- [ ] V 为光油通道。
- [ ] 模型优先级保持 `Model > Support > Empty`。

## 3. Pixel Polarity

- [ ] `polarity = black_is_print`。
- [ ] `printValue = 0`。
- [ ] `emptyValue = 255`。
- [ ] 空白区域所有通道为 `255`。
- [ ] 支撑区域 S 通道为 `0`，其他通道默认 `255`。

## 4. TIFF Sample Format

- [ ] `BitsPerSample = 8,8,8,8,8,8`。
- [ ] `SampleFormat = unsigned integer`。
- [ ] `SamplesPerPixel = 6`。
- [ ] `PlanarConfig = contiguous`。
- [ ] 不使用 `TIFFReadRGBAImage` 进行生产读取。

## 5. TIFF Photometric And Compression

- [ ] `PhotometricInterpretation = RGB`。
- [ ] `Compression = 1`，当前为 uncompressed。
- [ ] Extra samples 存在时不改变 RGBWSV 通道顺序。

## 6. Stripped Storage

- [ ] `storageMode = stripped`。
- [ ] `tiff.tiled = false`。
- [ ] `RowsPerStrip = 64` 或目标 RIP 确认的可接受正整数。
- [ ] `StripOffsets` 存在。
- [ ] `StripByteCounts` 存在。
- [ ] 不要求 `TileOffsets / TileByteCounts`。

## 7. Tiled Storage

- [ ] `storageMode = tiled`。
- [ ] `tiff.tiled = true`。
- [ ] `tileSize = [256, 256]` 或目标 RIP 确认的兼容尺寸。
- [ ] `TileWidth` / `TileLength` 存在。
- [ ] `TileOffsets` / `TileByteCounts` 存在。
- [ ] tile padding 使用 `255`。

## 8. Layer List

- [ ] manifest 顶层 `layers` 与 `grid.layerCount` 一致。
- [ ] 每层 `path` 指向存在的 TIFF。
- [ ] 每层 `widthPx / heightPx` 与 `grid` 一致。
- [ ] 每层 TIFF 实际尺寸与 manifest 一致。

## 9. Reader Error Handling

- [ ] schema 错误返回 schema 类错误。
- [ ] channel order / channel count 错误可定位。
- [ ] bit depth / planar config 错误可定位。
- [ ] storageMode 非法可定位。
- [ ] manifest storageMode 与实际 TIFF tag 结构不一致可定位。
- [ ] rowsPerStrip / tileSize 非法可定位。

## 10. 当前 P0 限制

- [ ] 不支持 BigTIFF。
- [ ] 不支持压缩 TIFF。
- [ ] 不支持 planar separate。
- [ ] 不支持 CMYK / ICC / 半色调。
- [ ] 不支持多 IFD 生产语义。
