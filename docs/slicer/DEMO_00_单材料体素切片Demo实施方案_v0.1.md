# DEMO_00_单材料体素切片Demo实施方案

## 1. 实施顺序

1. `slicer_cli` 空流程
2. 配置读取
3. STL/OBJ loader
4. 层采样
5. 下表面投影支撑
6. RGBWSV layer composer
7. TIFF writer
8. manifest writer
9. `rip_reader_test`
10. 可选 preview PNG
11. 可选 Qt demo

## 2. 验收

- `slicer_cli --config samples/configs/slice_config.json` 可以运行
- 生成 `SlicePackage/manifest.json`
- 生成 TIFF layers
- TIFF 通道顺序为 R G B W S V
- 支撑出现在 S 通道
- `rip_reader_test` 能验证数据包

## 3. P0 不做

```text
彩色纹理
3MF
glTF
复杂支撑
Unity
VTK
完整 Qt UI
```

## 4. 实施优先级

```text
CLI Core 优先
RIP 验证优先
Qt 调试其次
3D 展示最后
```
