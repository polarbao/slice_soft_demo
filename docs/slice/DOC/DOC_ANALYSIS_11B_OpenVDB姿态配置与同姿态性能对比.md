# DOC_ANALYSIS_11B_OpenVDB姿态配置与同姿态性能对比

> 文档版本：v0.1  
> 文档状态：Analysis / Evidence  
> 生成日期：2026-07-04  
> 适用阶段：Stage 11B 前置分析  

---

## 1. 结论

针对 `model/obj/nai_you_new/MF_nai_you.obj`，legacy 一键切片可以把模型趴放，OpenVDB candidate 一键切片不能趴放的直接原因不是 OpenVDB 算法本身，而是 UI 生成的 OpenVDB candidate 配置没有沿用 legacy 的 `modelTransform` 和 `autoOrient`。

11B-1 修复前代码证据：

```text
MainWindow::CreateOneClickConfig
  modelTransform.scale = [0.8, 0.8, 0.8]
  autoOrient.enabled = true
  autoOrient.maxHeightMm = 6.0

MainWindow::CreateOpenVdbCandidateConfig
  未写 modelTransform
  autoOrient.enabled = false
```

因此修复前 OpenVDB candidate UI 路径保持模型原始竖放姿态，导致模型高度仍约 24mm。

11B-1 当前代码已补齐：

```text
MainWindow::CreateOpenVdbCandidateConfig
  modelTransform.scale = [0.8, 0.8, 0.8]
  autoOrient.enabled = true
  autoOrient.maxHeightMm = 6.0
```

该修复只影响 UI 生成 OpenVDB candidate 配置，不改变默认 legacy 生产路径。

---

## 2. Inspect 证据

本次只做轻量模型检查，不生成生产包。

### 2.1 Legacy 一键配置

命令：

```powershell
.\build\Debug\slicer_cli.exe --config output\analysis_20260704_orientation\legacy_inspect.json --inspect-model
```

结果：

```text
autoOrient.enabled: true
autoOrient.applied: true
autoOrient.selectedOrientation: rotate_x_90
originalBboxMm height = 19.2365mm
orientedBboxMm height = 4.97729mm
```

### 2.2 当前 OpenVDB candidate UI 配置

命令：

```powershell
.\build\Debug\slicer_cli.exe --config output\analysis_20260704_orientation\openvdb_ui_inspect.json --inspect-model
```

结果：

```text
autoOrient.enabled: false
autoOrient.applied: false
autoOrient.selectedOrientation: identity
orientedBboxMm height = 24.0456mm
```

### 2.3 OpenVDB candidate 启用同样姿态配置

命令：

```powershell
.\build\Debug\slicer_cli.exe --config output\analysis_20260704_orientation\openvdb_oriented_inspect.json --inspect-model
```

结果：

```text
autoOrient.enabled: true
autoOrient.applied: true
autoOrient.selectedOrientation: rotate_x_90
originalBboxMm height = 19.2365mm
orientedBboxMm height = 4.97729mm
```

判断：

```text
OpenVDB candidate 可以使用同样趴放姿态；
当前 UI 生成配置不一致是主要问题；
该问题可以通过修复 CreateOpenVdbCandidateConfig 解决。
```

---

## 3. 同姿态性能探索

本次使用同一模型、同样 `scale=[0.8,0.8,0.8]`、同样 `autoOrient.enabled=true` 后的趴放姿态做一次探索性对比。

### 3.1 Legacy

命令：

```powershell
$elapsed = Measure-Command {
  .\build\Debug\slicer_cli.exe --config output\analysis_20260704_orientation\legacy_inspect.json
}
```

结果：

```text
legacy_total_seconds = 22.653
package = output/analysis_20260704_orientation/legacy_package
grid = 229 x 455 x 498
modelPixels = 8367116
supportPixels = 25724342
supportPrintPixels = 25724342
```

### 3.2 OpenVDB Candidate

命令：

```powershell
$elapsed = Measure-Command {
  .\build-openvdb-09p\Debug\slicer_cli.exe `
    --config output\analysis_20260704_orientation\openvdb_oriented_inspect.json `
    --openvdb-candidate-slice
}
```

结果：

```text
openvdb_candidate_total_seconds = 40.794
package = output/analysis_20260704_orientation/openvdb_oriented_package
status = non_production_written
productionPackageWritten = false
nonProductionPackageWritten = true
strictClosedFailure = strict_closed rejected mesh with boundary edges
admissionStatus = non_production_only
warningCodes = MESH_SELF_INTERSECTION_SAMPLED,MESH_BOUNDARY_EDGES
grid = 200 x 391 x 105
modelPixels = 1196924
shellPixels = 543734
supportPixels = 0
```

### 3.3 性能结论

当前结果不支持“OpenVDB candidate 对真实 `nai_you_new` 模型显著提速”。

原因：

```text
1. Legacy 完整生成了支撑和生产 package；
2. OpenVDB candidate 当前是 non-production 输出；
3. OpenVDB candidate 支撑像素为 0，未与 legacy 支撑策略等价；
4. OpenVDB strict_closed 仍被 boundary edges 阻断；
5. OpenVDB candidate 输出 grid/layerCount 与 legacy 不同；
6. Debug 构建下 OpenVDB candidate 总耗时更长；
7. 当前总耗时包含输出写包、preview 生成和 report 写入，不是 core-only benchmark。
```

因此本次只能说明：

```text
同姿态下当前 OpenVDB candidate 仍不能替代 legacy；
当前没有证据证明 OpenVDB 已能显著提高切片效率；
后续必须建立 Release 模式、同输出语义、同 preview 策略的正式 benchmark。
```

11B 后续 benchmark 必须进一步拆分：

```text
coreComputeMs：不包含 TIFF 保存、preview 图片生成、report/manifest 写入；
endToEndMs：包含完整用户等待时间；
只有 outputSemanticsComparable=true 且 OpenVDB productionAllowed=true 时，才允许比较替代性能。
```

---

## 4. 当前问题拆分

### P0：OpenVDB candidate 姿态配置不一致

问题：

```text
OpenVDB candidate UI 配置未继承 legacy 的 modelTransform / autoOrient。
```

解决：

```text
已在 MainWindow::CreateOpenVdbCandidateConfig 中增加 modelTransform.scale=[0.8,0.8,0.8]；
autoOrient.enabled 已改为 true；
maxHeightMm 保持 6.0；
生成配置后 inspect 应显示 rotate_x_90 和高度 <= 6mm。
```

### P0：生产数据预览语义不清

问题：

```text
UI texture_rgb preview 会隐藏非表面纹理带内部 RGB；
Photoshop RGB 只看前三通道；
用户无法一眼区分 RGB 填充、S 支撑、W 白墨、V 光油和真空白。
```

解决：

```text
新增生产 RGB 预览模式；
新增六通道像素探针；
UI 明确标记 preview source：texture_rgb preview / production RGB / support pseudo color。
```

### P1：非表面 RGB 策略缺口

问题：

```text
当前非表面纹理带默认回退到 modelMaterial.rgb；
无法配置为 empty 或 fallbackRgb。
```

解决：

```text
新增 texture.nonSurfaceRgbPolicy；
候选值：model_material / empty / fallback_rgb / material_policy。
```

### P1：UI 配置收敛不足

问题：

```text
samples/configs 下大量 JSON 同时承担正式 profile、回归 fixture、阶段样例；
普通用户难以判断哪个配置可长期使用。
```

解决：

```text
保留 fixture；
UI 只展示长期 Profile；
新增切片设置界面覆盖常用参数；
测试夹具进入高级/测试分类。
```

### P2：OpenVDB 替代 legacy 的生产 gate 不完整

问题：

```text
OpenVDB candidate 已能写候选包，但真实模型 strict_closed、支撑策略、性能、内存和回归稳定性尚未达成替代条件。
```

解决：

```text
建立 OpenVDB replacement gate；
要求真实模型集合 PASS、RIP PASS、UI PASS、支撑等价、性能可接受、OpenVDB OFF 不退化。
```

---

## 5. 当前建议

可以进入 Stage 11B 小收口，先处理：

```text
1. OpenVDB candidate 姿态配置修复；
2. 生产 RGB 预览和六通道像素探针设计；
3. UI 配置收敛设计；
4. OpenVDB 替代 legacy 的 gate 和 benchmark 计划。
```

不建议现在宣布 OpenVDB 开发完全结束，也不建议现在让 OpenVDB 替代 legacy。OpenVDB 可以暂时从主线研发中降为 candidate 维护状态，后续按 replacement gate 决定是否继续进入 production replacement 阶段。
