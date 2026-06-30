# DEMO_01_2_5D浮雕切片验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：PRD_01 / DEV_01  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

在 00C 已完成基础浮雕能力后，DEMO_01 用于验证正式 Relief 路线的可重复性。

目标：

```text
多个 relief 样例
多个 materialChannel
多个 fillMode
稳定 reports
稳定 preview
稳定 rip_reader_test
```

---

## 2. 样例组织建议

```text
samples/
  models/
    relief/
      relief_nail_arched.obj
      relief_flat_badge.stl
      relief_thin_shell.obj
      relief_height_variation.obj
  configs/
    relief/
      relief_nail_varnish_support.json
      relief_nail_white_support.json
      relief_flat_varnish_no_support.json
      relief_rgb_gray.json
```

---

## 3. 必须验证的配置

### 3.1 美甲甲片光油 + 支撑

```text
slicingMode = relief_heightfield
relief.fillMode = intersection_range
materialChannel = V
support.enabled = true
```

验收：

```text
V print pixels > 0
S print pixels > 0
rip_reader_test pass
```

### 3.2 美甲甲片白墨 + 支撑

```text
materialChannel = W
support.enabled = true
```

验收：

```text
W print pixels > 0
S print pixels > 0
```

### 3.3 贴底浮雕光油

```text
relief.fillMode = surface_to_base
support.enabled = false
materialChannel = V
```

验收：

```text
V print pixels > 0
S print pixels = 0
```

### 3.4 RGB 单材料浮雕

```text
materialChannel = RGB
rgb = [0, 0, 0]
```

验收：

```text
R/G/B print pixels > 0
W/S/V unused = 255
```

---

## 4. 运行方式

构建：

```powershell
cmake --build build --config Debug
```

执行：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\relief\relief_nail_varnish_support.json
build\Debug\rip_reader_test.exe --package output\ReliefNailVarnishSupport
```

---

## 5. 验收 Checklist

- [ ] 普通 P0 配置仍可运行。
- [ ] 00C 当前配置仍可运行。
- [ ] Relief 样例目录建立。
- [ ] 至少 4 个 relief 配置可运行。
- [ ] 每个 package 都有 manifest。
- [ ] 每个 package 都有 relief_report。
- [ ] 每个 package 都有 slice_report。
- [ ] 每个 package 都有 preview_report。
- [ ] V / W / RGB / S 通道预览正确。
- [ ] rip_reader_test 全部通过。
- [ ] 关键通道像素统计写入报告。

---

## 6. 暂不验证

DEMO_01 不验证：

```text
彩色纹理
局部光油
top_surface_only
top_n_layers
OpenVDB
Qt UI
```

---

## 7. Codex 推荐指令

```text
请阅读 PRD_01_2_5D浮雕正式切片路线.md、
DEV_01_relief_heightfield正式切片设计.md、
DEMO_01_2_5D浮雕切片验证方案.md。

当前任务不是做彩色纹理，而是把 00C relief_heightfield 能力整理为正式 Relief 路线。

优先完成：
1. samples/models/relief 与 samples/configs/relief 目录；
2. relief_nail_varnish_support 配置迁移；
3. relief_report 字段增强；
4. 支撑与材料通道像素统计；
5. 至少 4 个 relief 样例配置；
6. 更新 REPORT_00 或新增 REPORT_01_Relief当前实现状态.md。
```
