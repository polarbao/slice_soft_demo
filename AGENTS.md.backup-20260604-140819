# AGENTS.md

## 项目背景

本仓库用于开发 UV 3D 打印切片软件的 P0 Demo。

当前不是做完整全彩切片器，也不是做 FDM 路径规划器，而是验证 UV 打印软件上游切片数据链路。

## 技术栈

- 开发语言：C++20
- 后续 UI：Qt 5.15
- 构建系统：CMake
- 包管理：vcpkg
- IDE：VS Code
- 主要平台：Windows / MSVC

## P0 目标

实现 C++ CLI 切片原型：

```text
STL/OBJ 单模型
→ 模型标准化
→ 轻量检查/修复
→ Occupancy/SDF-like 层采样
→ 下表面投影支撑
→ RGBWSV 六通道 uint16 tiled TIFF
→ manifest.json
→ RIP Reader 验证
```

## 固定协议

TIFF 通道顺序固定为：

```text
R G B W S V
```

通道语义：

- R/G/B：配置文件中指定的单材料 RGB 常量色
- W：白墨语义通道
- S：支撑语义通道
- V：光油语义通道

默认参数：

```text
DPI = 600
layerThicknessMm = 0.01
```

## P0 规则

- P0 不实现全彩纹理切片。
- P0 不引入 Unity。
- P0 不引入 VTK。
- 不要先做 Qt UI，必须先跑通 CLI 数据闭环。
- 优先实现 `slicer_cli`。
- 然后实现 `rip_reader_test`。
- Qt 只作为后续调试、配置和预览壳。
- Python 只能用于算法草模，不作为 P0 验收对象。
- `slicer_core` 必须独立于 CLI 和 UI。
- 未经确认，不要引入重依赖。

## 推荐 Targets

```text
slicer_core
slicer_cli
rip_reader_test
slicer_qt_demo later
```

## 初始依赖建议

```text
nlohmann-json
libtiff
assimp
```

## 编码要求

- 使用清晰的 C++20。
- 模块要小而可测试。
- CLI 不依赖 Qt。
- core 不依赖 UI。
- 每个切片包必须包含 manifest 和 reports。
