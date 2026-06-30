# DEMO_07_Qt调试UI验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：PRD_07 / DEV_07  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

验证 Qt 调试 UI 能完成：

```text
选择 config
运行 slicer
运行 rip_reader
查看 reports
查看 preview
查看 material process summary
比较 profile
运行 quick regression
```

---

## 2. 启动方式

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.uild\Debug\slicer_debug_ui.exe
```

---

## 3. 验证样例

### 3.1 Material Process Top2

配置：

```text
samples/configs/material_process/nail_rgb_white_varnish_top2.json
```

UI 验收：

```text
Run Slicer 成功
Open Package 成功
MaterialProcess validation.pass = true
RGB/W/V/S printPixels 可见
V activeLayerIndices 可见
preview 可查看
```

---

### 3.2 3MF Texture RGBWV

配置：

```text
samples/configs/material_process/three_mf_texture_rgb_white_varnish.json
```

UI 验收：

```text
texture_report.source = 3mf_internal
material_process_report validation.pass = true
RIP summary pass
```

---

### 3.3 OBJ/MTL Texture RGBWV

配置：

```text
samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json
```

UI 验收：

```text
texture_report sampledPixels > 0
material_role_mapping_report mappedRgb > 0
RIP summary pass
```

---

### 3.4 Profile Compare

Package A：

```text
output/NailRgbWhiteVarnishTop1
```

Package B：

```text
output/NailRgbWhiteVarnishTop3
```

UI 验收：

```text
Compare Profiles 成功
delta.varnishPrintPixels > 0
changedLayers > 0
```

---

## 4. 回归 Checklist

- [ ] UI 可启动。
- [ ] 可选择 config。
- [ ] 可运行 slicer_cli。
- [ ] 可打开 package。
- [ ] 可运行 rip_reader_test --summary。
- [ ] 可查看 manifest。
- [ ] 可查看 material_process_report。
- [ ] 可查看 texture_report。
- [ ] 可查看 three_mf_report。
- [ ] 可查看 preview PNG / PPM。
- [ ] 可运行 profile compare。
- [ ] 可运行 quick regression。
- [ ] QProcess 执行时 UI 不假死。
- [ ] CLI quick regression 仍通过。

---

## 5. 状态报告

完成后生成：

```text
docs/slicer/REPORT_07_Qt调试UI当前实现状态.md
```
