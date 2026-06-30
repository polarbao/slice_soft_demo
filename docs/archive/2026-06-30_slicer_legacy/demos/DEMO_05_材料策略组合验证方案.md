# DEMO_05_材料策略组合验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：PRD_05 / DEV_05  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

验证 MaterialPolicy 基础策略：

```text
RGB texture only
RGB + W underbase
RGB + V top_n_layers
RGB + W + V
V only
W only
```

---

## 2. 样例目录

新增：

```text
samples/configs/material_policy/
```

可复用：

```text
samples/models/textured/
samples/models/textured/fixtures/
```

---

## 3. 必须验证样例

### 3.1 RGB Texture Only

```text
materialPolicy.enabled = true
rgb.enabled = true
white.enabled = false
varnish.enabled = false
```

验收：

```text
RGB printPixels > 0
W printPixels = 0
V printPixels = 0
S support 正常
```

### 3.2 RGB + White Underbase

```text
white.enabled = true
white.mode = underbase
```

验收：

```text
RGB printPixels > 0
W printPixels > 0
V printPixels = 0
```

### 3.3 RGB + Varnish Top 2 Layers

```text
varnish.enabled = true
varnish.mode = top_n_layers
varnish.topLayers = 2
```

验收：

```text
V printPixels > 0
V 只出现在模型顶部层范围
```

### 3.4 RGB + W + V

```text
white.enabled = true
varnish.enabled = true
```

验收：

```text
RGB/W/V 均有 printPixels
S support 不被覆盖
```

### 3.5 V Only / W Only 回归

确保旧单材料语义不被破坏。

---

## 4. 验证命令

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\material_policy\textured_rgb_white_varnish.json
build\Debug\rip_reader_test.exe --package output\MaterialPolicyRgbWhiteVarnish

.\scripts\run_regression.ps1
```

---

## 5. 验收 Checklist

- [ ] material_policy_report.json 存在。
- [ ] RGB texture only 通过。
- [ ] RGB + W underbase 通过。
- [ ] RGB + V top_n_layers 通过。
- [ ] RGB + W + V 通过。
- [ ] V only / W only 回归通过。
- [ ] support S 通道不被覆盖。
- [ ] run_regression.ps1 通过。
- [ ] RGBWSV 协议不变。
- [ ] 04A fallback 用例仍通过。

---

## 6. 非目标

```text
ICC
CMYK
RIP
3MF
OpenVDB
Qt UI
texture-driven material masks
support morphology
```

---

## 7. 状态报告

完成后生成：

```text
docs/slicer/REPORT_05_材料策略当前实现状态.md
```
