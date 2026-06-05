# ROADMAP_v0.2_00C后续PRD_DEV文档生成计划

> 文档版本：v0.2  
> 文档状态：Draft / 00C 后续路线更新  
> 适用阶段：00C 完成后  
> 建议提交目录：`docs/slicer/`

---

## 1. 路线调整背景

原始 ROADMAP 将 PRD_01 定义为彩色纹理模型切片。

但当前 00C 已经完成单材料浮雕光油与下表面支撑联合输出，且浮雕模型是当前业务高频模型。因此后续路线需要调整：

```text
浮雕正式路线提前
彩色纹理阶段后移
协议固化并行进行
```

---

## 2. 当前已完成阶段

### 2.1 P0 / 00B / 00A / 00C

已完成：

```text
P0：基础切片闭环
00B：uint8 + black_is_print
00A：preview / report / 导入稳定化
00C：relief_heightfield + V 光油 + S 支撑
```

当前能力：

```text
普通模型：closed_mesh_scanline
浮雕模型：relief_heightfield
材料输出：RGB / W / V / auto
支撑输出：S bottom_projection
协议：RGBWSV uint8 TIFF
验证：rip_reader_test
```

---

## 3. 新文档编号计划

### PRD_01 / DEV_01 / DEMO_01：2.5D / Relief 浮雕正式切片路线

目标：

```text
将 00C relief_heightfield 从 Demo 能力升级为正式产品路线。
```

范围：

```text
正式 slicingMode = relief_heightfield
高度场 / 浮雕模型诊断
单材料 V / W / RGB 输出
下表面支撑
relief_report 规范化
relief 专用测试模型集
material applyMode 阶段定义
```

文件：

```text
PRD_01_2_5D浮雕正式切片路线.md
DEV_01_relief_heightfield正式切片设计.md
DEMO_01_2_5D浮雕切片验证方案.md
TASKS_01_2_5D浮雕正式路线任务清单.md
```

---

### PRD_02 / DEV_02：支撑生成、孤岛检测与 SupportType 扩展

目标：

```text
将当前 bottom_projection 支撑升级为可扩展支撑体系。
```

范围：

```text
unsupported_only
layer-to-layer overlap
island detection
support report
SupportType
支撑材料统计
支撑预览
```

文件：

```text
PRD_02_支撑生成与孤岛检测.md
DEV_02_高级支撑体素生成设计.md
```

---

### PRD_03 / DEV_03：RGBWSV TIFF 协议与 RIP 输入规范固化

目标：

```text
将 00B/00C 中已经事实稳定的 RGBWSV 输出协议固化为规范。
```

范围：

```text
PrivateMultiChannelTiffSpec
manifest schema
RIP reader contract
错误码
协议升级策略
preview 与生产数据分离
black_is_print
```

文件：

```text
PRD_03_RGBWSV多通道TIFF协议.md
DEV_03_TIFFWriter与RIPReader协议设计.md
```

---

### PRD_04 / DEV_04：彩色纹理模型切片

目标：

```text
OBJ + MTL + Texture
UV 采样
ColorShellVolume
真实 RGB 输出
侧壁颜色基础处理
```

文件：

```text
PRD_04_彩色纹理模型切片.md
DEV_04_彩色壳体体素与纹理采样设计.md
```

---

### PRD_05 / DEV_05：3MF 与多材料模型支持

目标：

```text
3MF mesh/material/color
MaterialId
SurfaceType
Alpha
多材料映射
```

文件：

```text
PRD_05_3MF与多材料模型支持.md
DEV_05_3MF导入与MaterialVolume设计.md
```

---

### PRD_06 / DEV_06：Qt 调试 UI 与模型预览

目标：

```text
Qt 配置面板
日志查看
layer/channel preview
QOpenGLWidget mesh preview
build plate / bbox / slice plane
```

文件：

```text
PRD_06_Qt切片调试UI.md
DEV_06_Qt_OpenGL模型预览与通道查看设计.md
```

---

### PRD_07 / DEV_07：OpenVDB / SDF 正式体素内核

目标：

```text
OpenVDB sparse volume
SDF-based Level Set
color shell thickness
white/varnish shell
support interface thickness
```

文件：

```text
PRD_07_正式SDF体素切片内核.md
DEV_07_OpenVDB_SDF_LevelSet切片内核设计.md
```

---

## 4. 近期建议执行顺序

### Step 1：先补充 PRD_01 / DEV_01 / DEMO_01

用于把 00C 能力升级为正式 2.5D Relief 路线。

### Step 2：同步补 PRD_03 / DEV_03

用于协议固化，不一定立即大改代码。

### Step 3：再做 PRD_02 / DEV_02

支撑与孤岛检测依赖 PRD_01 的高度场和层统计能力，建议排在 PRD_01 之后。

### Step 4：彩色纹理后移

彩色纹理应在 relief 路线和 RGBWSV 协议稳定后再进入。

---

## 5. Codex 后续生成原则

Codex 在生成后续文档或代码前必须遵守：

```text
1. 先阅读 AGENTS.md
2. 先阅读 CODEX_HANDOFF
3. 先阅读 REPORT_00_P0_Demo当前实现状态.md
4. 先阅读本 ROADMAP_v0.2
5. 不要改变 RGBWSV 通道顺序
6. 不要把 0/255 极性改回去
7. 不要引入 Unity
8. 不要在当前阶段引入 VTK
9. 不要跳过 PRD_01 直接做彩色纹理
```

---

## 6. 结论

00C 完成后，项目路线应调整为：

```text
先正式化 2.5D / Relief
再增强支撑与协议
再进入彩色纹理
```
