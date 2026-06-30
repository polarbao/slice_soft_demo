# PRD_00A_P0Demo增强_预览导入与几何鲁棒性

> 文档版本：v0.1  
> 文档状态：Draft / P0+ 稳定化需求  
> 所属模块：切片软件 / Slicer  
> 上游依据：`REPORT_00_P0_Demo当前实现状态.md`  
> 目标阶段：P0 Demo 已跑通后的稳定化增强阶段

---

## 1. 背景

当前 P0 Demo 已经实现以下核心闭环：

```text
STL/OBJ 输入
→ 配置读取
→ 模型变换与 autoOrient
→ 三角面 Z 截面采样
→ 下表面投影支撑
→ RGBWSV layer compose
→ tiled uint16 TIFF
→ manifest/reports
→ rip_reader_test
```

这说明 P0 Demo 已经从“骨架阶段”进入“可运行验证阶段”。

但当前实现仍存在 P0 工程稳定性不足的问题：

```text
preview 仍为 PPM，不是 PNG
模型导入不支持 binary STL
OBJ MTL/材质信息未读取
mesh repair 仍为轻量检查
多轮廓/孔洞/退化几何鲁棒性不足
没有完整自动化测试
```

因此需要增加 P0+ 稳定化阶段。

---

## 2. 产品目标

P0+ 的目标不是进入全彩纹理，也不是进入正式 OpenVDB/SDF 内核，而是让当前 P0 Demo 更适合真实模型验证和后续开发接力。

目标：

```text
提升 preview 可读性
提升模型导入兼容性
提升几何采样可诊断性
提升 report 可审核性
提升 demo 验证稳定性
```

---

## 3. 范围

### 3.1 本阶段必须完成

| 编号 | 功能 | 说明 |
|---|---|---|
| PRD00A-PREVIEW-001 | PNG Preview 输出 | 在现有 PPM 基础上增加 PNG 输出，便于调试和 UI 展示 |
| PRD00A-PREVIEW-002 | Preview 通道选择 | 支持 RGB / S / W / V 通道选择 |
| PRD00A-PREVIEW-003 | Preview 层范围选择 | 支持 layer range 或 interval |
| PRD00A-IMPORT-001 | Binary STL 读取 | 支持真实 STL 常见格式 |
| PRD00A-IMPORT-002 | OBJ MTL 基础读取 | 至少能识别 mtllib/usemtl，并输出报告 |
| PRD00A-REPORT-001 | 模型导入统计增强 | 输出 vertex/face/triangle/material 统计 |
| PRD00A-GEO-001 | 每层非零像素统计 | model/support/W/V/RGB 非零统计 |
| PRD00A-GEO-002 | Slice contour report | 输出每层 segment 数、fill 状态、异常层 |
| PRD00A-TEST-001 | 测试样例集 | 增加 cube、sphere、binary STL、孔洞模型、薄片模型 |
| PRD00A-TEST-002 | 基础自动化验证 | 至少验证 CLI、manifest、TIFF、RIP reader |

### 3.2 本阶段可选

| 功能 | 说明 |
|---|---|
| Assimp 接入评估 | 可作为 OBJ/STL 导入增强方向 |
| libtiff 接入评估 | 当前内部 TIFF writer 可保留，后续评估标准库 |
| Qt layer/channel preview | 可选，不阻塞 P0+ 验收 |

### 3.3 本阶段不做

```text
全彩纹理采样
UV 贴图烘焙
3MF
glTF/GLB
复杂支撑树
上表面支撑
侧壁辅助支撑
OpenVDB 正式 SDF 内核替换
VTK / Unity
完整 Qt 产品 UI
```

---

## 4. 用户价值

P0+ 主要服务对象：

| 用户 | 价值 |
|---|---|
| 算法开发人员 | 更容易定位某层切片错误、支撑错误、轮廓异常 |
| 工艺调试人员 | 可以通过 PNG 快速查看 S/W/V/RGB 区域 |
| RIP 开发人员 | 可以获得更稳定的 TIFF/manifest 样例 |
| Codex / 后续开发者 | 可以通过报告和测试样例理解工程现状 |

---

## 5. 功能需求

### 5.1 Preview 正式化

当前 preview 是 PPM P6。P0+ 需要支持 PNG preview。

输出目录：

```text
preview/
  model_rgb_000010.png
  support_s_000010.png
  white_w_000010.png
  varnish_v_000010.png
```

配置示例：

```json
{
  "preview": {
    "enabled": true,
    "format": "png",
    "interval": 10,
    "layerRange": [0, 200],
    "channels": ["rgb", "support", "white", "varnish"],
    "onlyNonEmptyLayers": true
  }
}
```

### 5.2 模型导入增强

必须支持：

```text
ASCII STL
Binary STL
OBJ v/f
OBJ f v/vt/vn
OBJ 多边形 fan triangulation
OBJ mtllib/usemtl 基础识别
```

P0+ 不要求真正使用材质做切片，但必须在报告中体现材质信息。

### 5.3 几何采样可诊断性

每层输出统计：

```text
layerIndex
zMm
segmentCount
modelNonZeroPixels
supportNonZeroPixels
whiteNonZeroPixels
varnishNonZeroPixels
fillWarnings
```

### 5.4 Reports 增强

增强以下报告：

```text
model_report.json
slice_report.json
support_report.json
preview_report.json
```

新增：

```text
contour_report.json
```

### 5.5 自动化验证

至少提供：

```text
rip_reader_test
基础 sample 配置
最小测试模型
checksum 输出
错误配置测试
缺层测试
错误通道测试
```

---

## 6. 验收标准

P0+ 完成后应满足：

1. `slicer_cli` 可以读取 ASCII STL、Binary STL、OBJ。
2. OBJ 中的 MTL 相关信息可以进入报告。
3. preview 可以输出 PNG。
4. preview 可以按通道、间隔、层范围输出。
5. `preview_report.json` 包含每张 preview 的非零像素统计。
6. `slice_report.json` 包含每层 model/support 非零统计。
7. `contour_report.json` 可以指出异常层。
8. `rip_reader_test` 仍可验证 RGBWSV TIFF 数据包。
9. 现有 P0 通道顺序不变：`R G B W S V`。
10. 不破坏已有 `0.3.obj` 测试流程。

---

## 7. 阶段结论

P0+ 是从“Demo 能跑通”到“Demo 可稳定验证”的过渡阶段。

它的目标不是新增大功能，而是提高当前 Demo 的：

```text
可视化
可诊断
可复现
可测试
输入兼容性
```

完成 P0+ 后，再进入：

```text
PRD_01 彩色纹理模型切片
PRD_02 高级支撑
PRD_03 RGBWSV 协议固化
```
