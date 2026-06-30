# PRD_00_单材料体素切片与RIP前置数据生成

> 版本：v0.2  
> 状态：Draft / P0 Demo  
> 类型：PRD

## 1. 目标

验证单材料 UV 切片 P0 数据闭环：

```text
STL/OBJ 单模型
→ C++ slicer_cli
→ Occupancy/SDF-like 层采样
→ 下表面投影支撑
→ RGBWSV uint16 tiled TIFF
→ manifest.json
→ RIP Reader 验证
```

## 2. P0 范围

- 支持 STL
- 支持 OBJ
- 单模型
- 单材料 RGB 配置色
- 下表面投影支撑
- 每层一个 TIFF
- 通道顺序：R G B W S V
- uint16
- tiled TIFF
- contiguous
- 600 DPI
- 默认层厚 0.01mm

## 3. 非目标

- 彩色纹理切片
- 3MF
- glTF/GLB
- 复杂支撑
- Unity
- VTK
- 完整 Qt UI

## 4. 验收标准

- `slicer_cli --config samples/configs/slice_config.json` 可以运行
- 生成 `SlicePackage/manifest.json`
- 生成 TIFF layers
- TIFF 通道顺序为 R G B W S V
- 支撑出现在 S 通道
- `rip_reader_test` 能验证数据包

## 5. Demo 形态

P0 正式验收以 C++ CLI 为准：

```text
slicer_core
slicer_cli
rip_reader_test
```

Qt UI、Preview PNG、QOpenGLWidget 只作为后续调试辅助。
