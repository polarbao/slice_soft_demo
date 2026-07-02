# OPENVDB_OBJ彩色纹理切片能力评估与改进方案

> 日期：2026-07-02
> 文档类型：User Guide / Engineering Assessment
> 结论：当前 OpenVDB 能力处于 experimental diagnostic / hardening 阶段，不能正式用于 OBJ 彩色纹理模型生产切片。相关正式开发计划已纳入 Stage 11A，并要求在 Stage 12 前处理。

## 1. 当前结论

当前项目存在 OpenVDB 相关服务、demo、测试和诊断报告，但它不是默认生产切片路径。

Stage 11A 正式计划入口：

```text
docs/slice/DOC/DOC_DECISION_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划.md
docs/slice/PRD/PRD_11A_OpenVDB_OBJ彩色纹理切片前置计划.md
docs/slice/DEV/DEV_11A_OpenVDB_OBJ彩色纹理切片改造计划.md
docs/slice/DEMO/DEMO_11A_OpenVDB_OBJ彩色纹理切片验证方案.md
docs/codex_task/current/TASKS_11A_OpenVDB_OBJ彩色纹理切片前置任务清单.md
```

当前 UI 中：

```text
导入模型并切片
```

走 legacy production path，能够生成 RGBWSV 输出包。

当前 UI 中：

```text
导入模型并 OpenVDB 诊断
```

走 experimental diagnostic path，只生成实验报告，不生成 production RGBWSV package。

## 2. 当前 OpenVDB 阶段判断

根据当前代码和正式文档，OpenVDB 当前阶段为：

```text
09P-R2 experimental OpenVDB surface-shell pipeline hardening
```

已经具备：

```text
OpenVDB optional adapter
OpenVdbGeometryKernelService
SurfaceShellTextureService 契约
MaterialChannelComposer bridge
ProductionAdmissionPolicy
experimental report schema
UI 读取 experimental report
OpenVDB OFF / ON 分层 CI 脚本
```

仍未具备：

```text
OpenVDB production RGBWSV package writer path
OBJ 彩色纹理 surface-shell 正式输出管线
严格 production admission 后的 package 写出
真实模型 topology blocker 的 repair_then_strict 实现
OpenVDB per-layer RGBWSV 与 legacy package manifest/list 的完整一致性验证
RIP reader 对 OpenVDB 生成包的 golden 验收
```

## 3. 为什么不能直接用 OpenVDB 正式切片

当前 `slicer_cli --experimental-openvdb-shell` 的安全不变量是：

```text
productionPackageWritten = false
writeProductionRgbwsv = false
legacyPathExecuted = false
textureTransfer = not_executed_cli_diagnostic_only
```

也就是说它的定位是：

```text
诊断 OpenVDB 可用性
诊断 topology/admission
输出实验 report
不写生产 TIFF / manifest / layer list
```

如果直接把 UI 勾选或按钮改成 production 写出，会破坏以下项目红线：

```text
OpenVDB optional and disabled by default
legacy slicer_cli production path is not replaced
experimental OpenVDB path must not implicitly write production RGBWSV
warn_and_attempt / diagnostic_only must not be treated as production-safe
```

## 4. OBJ 彩色纹理 OpenVDB 正式切片目标链路

目标链路应为：

```text
OBJ / MTL / Texture
-> SceneModel / TriangleMeshData / UV / material mapping
-> topology diagnostics
-> ProductionAdmissionPolicy strict_closed
-> OpenVdbGeometryKernelService level set
-> surface shell / interior classification
-> SurfaceShellTextureService nearest-triangle UV transfer
-> MaterialChannelComposer RGBWSV channel buffer
-> RGBWSV TIFF writer / manifest writer
-> reports / preview / layer summary
-> rip_reader_test strict validation
```

## 5. 必须新增或改造的模块

### 5.1 Pipeline 分支

新增明确分支：

```text
texture.applyMode = surface_shell_from_sdf
experimental.openvdbPipeline.engine = openvdb
experimental.openvdbPipeline.writeProductionRgbwsv = true
```

只有同时满足 strict admission 时才允许写 production candidate package。

### 5.2 OBJ/MTL/Texture 资源传递

需要保证：

```text
OBJ mtllib 正确解析
MTL map_Kd 正确解析
贴图路径可来自模型同目录或 MTL 相对路径
每个 triangle 保留 materialName + UV
缺 UV / 缺贴图 / decode 失败进入 stable issue code
```

### 5.3 SurfaceShellTextureService 正式接入

需要实现：

```text
shell voxel / raster sample -> nearest triangle
nearest triangle -> barycentric
barycentric -> UV
UV -> texture RGB
texture RGB -> shell RGB buffer
```

要求：

```text
不跨 UV seam 平均
不跨 material seam 混色
missing texture 根据策略 warn_and_fallback / fail_fast
UV out-of-range 根据 clamp / repeat 处理并写 report
```

### 5.4 RGBWSV 输出写出

需要将 OpenVDB channel buffer 写成与 legacy 完全一致的输出包：

```text
manifest.json
layers/layer_*.tiff
reports/*.json
preview/*.png
```

并保持：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
```

### 5.5 UI 交互

UI 应提供两个明确入口：

```text
导入模型并切片
导入模型并 OpenVDB 诊断
```

未来 production candidate 完成后，再新增：

```text
导入模型并 OpenVDB 候选切片
```

该按钮必须在 UI 中显示 admission 结果：

```text
production_allowed
non_production_only
diagnostic_only
fail_fast
blockerCodes
warningCodes
```

## 6. 建议阶段拆分

### Stage A：UI 与诊断收口

目标：

```text
一键导入模型
legacy production 一键切片
OpenVDB diagnostic 一键报告
操作手册
```

验收：

```text
任意目录 OBJ 可导入并生成 legacy package
OpenVDB 按钮明确只生成 diagnostic report
UI self-test 通过
```

### Stage B：OpenVDB OBJ texture production candidate 原型

目标：

```text
新增 surface_shell_from_sdf pipeline 分支
OBJ/MTL/Texture transfer 到 shell RGB
MaterialChannelComposer 生成 per-layer RGBWSV buffer
只在 strict_closed 且无 blocker 时写 candidate package
```

验收：

```text
OBJ 彩色纹理颜色与贴图一致
UV seam 不串色
missing texture / missing UV report 正确
rip_reader_test PASS
legacy path 回归不变
```

### Stage C：真实模型鲁棒性与性能

目标：

```text
真实指甲 OBJ/3MF 集合验证
topology blocker 分类
repair_then_strict 决策
性能和内存报告
golden package
```

验收：

```text
OpenVDB ON / OFF CI 分层通过
OpenVDB unavailable 报错清晰
复杂模型失败时不写 production package
```

## 7. 当前推荐使用方式

生产演示或正式输出：

```text
使用“导入模型并切片”
```

OpenVDB 能力检查：

```text
使用“导入模型并 OpenVDB 诊断”
```

不要把 OpenVDB diagnostic report 当作生产切片包。
