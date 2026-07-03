# REPORT_11B_文档规划与OpenVDB替代评估当前状态

> 文档版本：v0.2  
> 文档状态：Stage Report / In Progress  
> 生成日期：2026-07-04

---

## 1. 本次完成内容

本报告最初记录 Stage 11B 文档规划和 OpenVDB 替代评估基线。当前已进入 11B 小收口开发，完成了 OpenVDB candidate UI 姿态配置修复，并补充 core-only benchmark 设计。

新增文档：

```text
docs/slice/DOC/DOC_ANALYSIS_11B_OpenVDB姿态配置与同姿态性能对比.md
docs/slice/DOC/DOC_DECISION_11B_UI配置生产预览与OpenVDB姿态收口.md
docs/slice/PRD/PRD_11B_UI配置生产预览与OpenVDB姿态收口.md
docs/slice/DEV/DEV_11B_UI配置生产预览与OpenVDB姿态收口设计.md
docs/slice/DEV/DEV_11B_OpenVDB_LegacyCoreBenchmark设计.md
docs/slice/DEMO/DEMO_11B_UI配置生产预览与OpenVDB同姿态验证方案.md
docs/slice/ROADMAP/ROADMAP_11B_OpenVDB替代Legacy生产引擎判定路线.md
docs/codex_task/current/TASKS_11B_UI配置生产预览与OpenVDB姿态收口任务清单.md
```

已实现代码改动：

```text
apps/slicer_debug_ui/MainWindow.cpp
  MainWindow::CreateOpenVdbCandidateConfig
    新增 modelTransform.scale=[0.8,0.8,0.8]
    autoOrient.enabled=true
    maxHeightMm=6.0
```

---

## 2. 当前关键结论

### 2.1 OpenVDB 趴放问题

当前 OpenVDB candidate UI 不能把 `nai_you_new` 模型趴放的原因是配置生成差异：

```text
legacy 一键切片：
  modelTransform.scale=[0.8,0.8,0.8]
  autoOrient.enabled=true

OpenVDB candidate 一键切片修复前：
  未写 modelTransform
  autoOrient.enabled=false

OpenVDB candidate 一键切片当前代码：
  modelTransform.scale=[0.8,0.8,0.8]
  autoOrient.enabled=true
  autoOrient.maxHeightMm=6.0
```

该问题已完成代码修复，不是 OpenVDB 算法本身不能旋转模型。

### 2.2 生产预览解释

当前已有基础填充处理：

```text
模型实体填充 => RGB 通道；
支撑填充 => S 通道；
texture_rgb preview 只显示纹理表面，不等价于生产 RGB。
```

缺口：

```text
生产 RGB 预览模式；
六通道像素探针；
nonSurfaceRgbPolicy；
UI 预览 source 标识。
```

### 2.3 配置文件收敛

不能简单合并所有 JSON。

原因：

```text
很多 JSON 是回归 fixture；
不同阶段配置承担协议和功能验收；
OpenVDB / 3MF / material / support / storage 等目标不同。
```

可行方向：

```text
UI 设置界面覆盖长期配置项；
默认只展示少量长期 Profile；
fixture 进入高级/测试分类；
测试脚本仍读取完整配置集合。
```

### 2.4 OpenVDB 是否可替代 legacy

当前不能替代。

理由：

```text
真实模型 strict_closed 仍可失败；
candidate 可写 non-production package，但不是 production；
支撑策略未与 legacy 等价；
同姿态 Debug 探索未显示性能提升；
replacement gate 未满足。
```

---

## 3. 本次实际验证

### 3.1 Inspect 验证

Legacy：

```text
autoOrient.enabled = true
autoOrient.applied = true
selectedOrientation = rotate_x_90
oriented height = 4.97729mm
```

OpenVDB candidate 当前 UI 配置：

```text
autoOrient.enabled = false
selectedOrientation = identity
height = 24.0456mm
```

OpenVDB candidate 启用同样姿态配置：

```text
autoOrient.enabled = true
autoOrient.applied = true
selectedOrientation = rotate_x_90
oriented height = 4.97729mm
```

### 3.2 同姿态探索性耗时

```text
legacy_total_seconds = 22.653
openvdb_candidate_total_seconds = 40.794
```

OpenVDB candidate 输出：

```text
status = non_production_written
productionPackageWritten = false
nonProductionPackageWritten = true
strictClosedFailure = strict_closed rejected mesh with boundary edges
admissionStatus = non_production_only
warningCodes = MESH_SELF_INTERSECTION_SAMPLED,MESH_BOUNDARY_EDGES
supportPixels = 0
```

结论：

```text
当前没有证据证明 OpenVDB 对该真实模型显著提速；
当前输出也不能作为 production replacement 对比。
```

补充结论：

```text
上述耗时是 Debug 端到端探索结果；
它包含输出写包、preview 生成和 report 写入；
后续正式比较必须使用 coreComputeMs，不得把 TIFF/preview/report I/O 混入算法核心耗时。
```

---

## 4. 建议执行顺序

推荐进入 Stage 11B，按以下顺序执行：

```text
1. Task 11B-1：OpenVDB candidate 姿态配置修复；（已实现，构建和 UI self-test 通过）
2. Task 11B-2/3：生产 RGB 预览和像素探针；
3. Task 11B-4：nonSurfaceRgbPolicy；
4. Task 11B-5/6：UI 设置和 Profile/fixture 分层；
5. Task 11B-7：正式 replacement benchmark；（必须拆分 coreComputeMs / endToEndMs）
6. Task 11B-8：阶段报告。
```

Task 11B-1 已完成代码修复。后续若继续推进 OpenVDB 替代评估，应优先做 core-only benchmark，而不是继续用端到端 Debug 耗时做判断。

---

## 5. 后续阶段建议

OpenVDB 后续开发建议进入“按 gate 推进”的节奏：

```text
短期：完成 11B 小收口；
中期：真实模型 repair_then_strict + 支撑等价；
中期：Release benchmark + memory budget；
长期：只有 replacement gate 全部满足后，才讨论默认替代 legacy。
```

OpenVDB 性能路线新增硬性约束：

```text
必须使用 Release；
必须同模型、同摆放、同 dpi、同 layerThickness；
必须分别记录 coreComputeMs 和 endToEndMs；
coreComputeMs 不包含 TIFF 保存、preview 图片生成、report/manifest 写入；
OpenVDB non-production 输出不得通过 replacement gate。
```

在此之前：

```text
legacy 继续作为默认生产路径；
OpenVDB 继续作为 diagnostic/candidate 引擎；
UI 必须明确显示 OpenVDB 输出是否 production。
```

---

## 6. 本轮验证

已运行：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

结果：

```text
slicer_debug_ui 构建通过；
UI self-test 输出 PASS startup / PASS experimental-report-summary；
git diff --check 无空白错误，仅有 LF/CRLF 工作区提示。
```

未完成验证：

```text
尚未新增 UI smoke case 自动生成 OpenVDB candidate config；
尚未对修复后的 UI 生成配置执行 inspect-model；
尚未实现 benchmark-core-only CLI。
```
