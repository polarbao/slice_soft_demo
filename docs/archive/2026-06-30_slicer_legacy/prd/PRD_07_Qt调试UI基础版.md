# PRD_07_Qt调试UI基础版

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_05A 之后  
> 所属模块：Qt Debug UI / CLI Wrapper / Report Viewer  
> 技术栈：C++20 / Qt 5.15 Widgets / CMake  
> 建议提交目录：`docs/slicer/`

---

## 1. 产品目标

07 目标是构建一个本地调试 UI：

```text
slicer_debug_ui
```

用于：

```text
选择配置
运行切片
运行 RIP 验证
查看 manifest / reports
查看 preview
查看 MaterialProcessProfile 结果
对比 profile
查看 regression 结果
```

它是开发调试工具，不是最终生产 UI。

---

## 2. 用户场景

### 2.1 快速运行一个配置

用户选择：

```text
samples/configs/material_process/nail_rgb_white_varnish_top2.json
```

点击：

```text
Run Slicer
```

UI 显示：

```text
CLI stdout/stderr
package path
run result
耗时
```

---

### 2.2 查看输出 package

用户打开：

```text
output/NailRgbWhiteVarnishTop2
```

UI 读取：

```text
manifest.json
reports/material_process_report.json
reports/material_policy_report.json
reports/material_role_mapping_report.json
reports/texture_report.json
reports/three_mf_report.json
reports/support_report.json
reports/slice_report.json
preview/*.png or *.ppm
```

并显示：

```text
通道统计
profile 验证结果
warnings / errors
preview 图像
```

---

### 2.3 运行 RIP 验证

点击：

```text
Run RIP Reader
```

执行：

```text
rip_reader_test --package <package> --summary
```

UI 显示：

```text
schema
storageMode
channelOrder
layerCount
channel printPixels
PASS / FAIL
```

---

### 2.4 比较两个 profile

用户选择两个 package：

```text
Package A = output/NailRgbWhiteVarnishTop1
Package B = output/NailRgbWhiteVarnishTop3
```

点击：

```text
Compare Profiles
```

执行：

```text
scripts/compare_material_profiles.ps1
```

UI 展示：

```text
delta.rgbPrintPixels
delta.whitePrintPixels
delta.varnishPrintPixels
delta.supportPrintPixels
changedLayers
validation.pass
```

---

## 3. 必须支持功能

### 3.1 Project / Config Panel

```text
选择 config.json
选择 output package
显示当前 repo root
显示 build dir
显示 slicer_cli path
显示 rip_reader_test path
```

---

### 3.2 Run Panel

按钮：

```text
Build Debug
Run Slicer
Run RIP Summary
Run Quick Regression
Compare Profiles
Open Output Folder
```

所有执行必须异步，不阻塞 UI。

---

### 3.3 Report Panel

以 Tree / Tab 显示：

```text
manifest
slice_report
material_process_report
material_policy_report
material_role_mapping_report
texture_report
three_mf_report
support_report
preview_report
```

必须能显示：

```text
JSON raw view
关键 summary view
warnings / failures list
```

---

### 3.4 Preview Panel

支持：

```text
PNG
PPM
```

显示：

```text
layer slider
channel selector
zoom in/out
fit to window
actual size
```

第一版不要求三维模型预览。

---

### 3.5 Material Process Panel

读取：

```text
material_process_report.json
```

展示：

```text
profileName
target
RGB/W/V/S printPixels
coverageRatio
V activeLayerIndices
missingUnderbasePixels
validation.pass
validation.failures
warnings
```

---

### 3.6 Log Panel

显示：

```text
QProcess stdout
QProcess stderr
command line
exit code
duration
```

错误码高亮：

```text
E_*
```

---

## 4. 非目标

07 不做：

```text
设备通信
任务队列
多设备管理
喷头 bitstream
RIP 半色调
ICC / CMYK
OpenVDB
新的切片算法
完整 3D 模型视图
生产级用户权限
云端服务
```

---

## 5. 验收标准

1. `slicer_debug_ui` 可独立启动。
2. 可选择 config 并运行 slicer_cli。
3. 可打开 output package。
4. 可运行 rip_reader_test --summary。
5. 可查看 manifest 和主要 reports。
6. 可查看 PNG / PPM preview。
7. 可查看 material_process_report summary。
8. 可运行 profile compare。
9. 可运行 quick regression。
10. 所有命令异步执行，UI 不假死。
11. 不改变 slicer_core 输出协议。
12. CLI regression 仍通过。

---

## 6. 结论

07 的核心价值是：

```text
把当前命令行与 reports 体系可视化，降低调试成本。
```
