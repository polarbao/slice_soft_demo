# ROADMAP_后续PRD_DEV文档生成计划

## 1. 文档体系目标

本仓库当前只包含 P0 Demo 的基础 PRD、DEV、DEMO 文档。后续需要逐步生成更完整的切片软件文档体系，方便 Codex 继续生成代码和设计资料。

文档编号从 00 开始。

## 2. 当前已有文档

```text
PRD_00_单材料体素切片与RIP前置数据生成_v0.2.md
DEV_00_单材料体素切片引擎与RGBWSV多通道TIFF输出设计_v0.2.md
DEMO_00_单材料体素切片Demo实施方案_v0.1.md
```

## 3. 后续文档计划

### 3.1 PRD_01 / DEV_01：彩色纹理模型切片

目标：

```text
OBJ + MTL + Texture
UV 采样
ColorShellVolume
真实 RGB 输出
侧壁颜色基础处理
```

非目标：

```text
不在 P1 直接做完整多材料
不在 P1 直接做 3MF Volumetric
```

建议文档：

```text
PRD_01_彩色纹理模型切片.md
DEV_01_彩色壳体体素与纹理采样设计.md
```

### 3.2 PRD_02 / DEV_02：支撑生成与孤岛检测

目标：

```text
unsupported-only support
layer-to-layer overlap
island detection
support report
SupportType 扩展
```

建议文档：

```text
PRD_02_支撑生成与孤岛检测.md
DEV_02_高级支撑体素生成设计.md
```

### 3.3 PRD_03 / DEV_03：RGBWSV TIFF 协议与 RIP 输入规范

目标：

```text
PrivateMultiChannelTiffSpec
manifest schema
RIP reader contract
错误码
协议升级策略
```

建议文档：

```text
PRD_03_RGBWSV多通道TIFF协议.md
DEV_03_TIFFWriter与RIPReader协议设计.md
```

### 3.4 PRD_04 / DEV_04：3MF 与多材料模型支持

目标：

```text
3MF mesh/material/color
MaterialId
SurfaceType
Alpha
多材料映射
```

建议文档：

```text
PRD_04_3MF与多材料模型支持.md
DEV_04_3MF导入与MaterialVolume设计.md
```

### 3.5 PRD_05 / DEV_05：Qt 调试 UI 与模型预览

目标：

```text
Qt 配置面板
日志查看
layer/channel preview
QOpenGLWidget mesh preview
build plate / bbox / slice plane
```

建议文档：

```text
PRD_05_Qt切片调试UI.md
DEV_05_Qt_OpenGL模型预览与通道查看设计.md
```

### 3.6 PRD_06 / DEV_06：OpenVDB / SDF 正式体素内核

目标：

```text
OpenVDB sparse volume
SDF-based Level Set
color shell thickness
white/varnish shell
support interface thickness
```

建议文档：

```text
PRD_06_正式SDF体素切片内核.md
DEV_06_OpenVDB_SDF_LevelSet切片内核设计.md
```

## 4. Codex 后续生成原则

Codex 在生成后续文档时必须遵守：

```text
先阅读 AGENTS.md
先阅读 CODEX_HANDOFF
先阅读当前 PRD/DEV/DEMO
不要跳过 P0 直接实现全彩
不要改变 RGBWSV 通道顺序
不要引入 Unity
不要在 P0 引入 VTK
```

## 5. 文档状态建议

每个新文档开头应包含：

```text
文档版本
文档状态
适用阶段
冻结项
可变更项
非目标
验收标准
```
