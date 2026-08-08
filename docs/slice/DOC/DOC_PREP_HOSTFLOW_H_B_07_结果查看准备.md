# DOC_PREP_HOSTFLOW_H-B-07 结果查看实施准备

> 状态：**IMPLEMENTATION COMPLETE / VALIDATION PASS**
> 日期：2026-08-08
> 任务：`H-B-07` 结果查看
> 前置：`H-B-06 COMPLETE`

## 1. 目标与边界

参考宿主在一次 `slice.rgbwsv` 成功后，仅通过冻结的公开 SPI 完成以下闭环：

1. 调用 `package.verify` 校验生产包；
2. 调用 `package.get_summary` 显示协议、网格、层数和实例摘要；
3. 调用 `package.get_layer_descriptor` 读取逐层高度和 RGBWSV 通道统计；
4. 调用 `package.render_layer_preview` 从生产 TIFF 渲染层预览；
5. 调用 `package.read_report` 读取命名报告。

本卡不修改 `PM_SPI_VERSION=1`、11 个导出、15 项能力、`p0.rgbwsv.2`、RGBWSV 顺序、
8-bit 或 `black_is_print`。宿主不得包含 `slicer_core` 头文件，不得直接解析 manifest、
生产 TIFF 或报告文件，也不得使用 `preview/` 调试图片冒充生产结果。

## 2. 已核实依赖

| 依赖 | 代码事实 | 准入结论 |
|---|---|---|
| 五项 package 能力 | `ModuleInfo.cpp` 与 `PackageCapabilityAdapter.cpp` 已提供同步能力 | PASS |
| 生产 TIFF 预览 | `PackageQueryFacade::RenderLayerPreview` 经 `TiffLayerSource` 读取生产 TIFF | PASS |
| H-B-06 输出身份 | 终态保留并核对 `packageDir` | PASS |
| 报告读取 | `package.read_report` 只接受 manifest 已登记的报告名 | PASS |
| 通道图数据 | 每层 descriptor 提供 R/G/B/W/S/V `printPixels` | PASS |

准备期真实 H-B-06 包复核发现：场景 Writer 未持久化 capability v1.2 已冻结的顶层
`perInstance/profileEcho` 摘要，导致 `package.get_summary` 正确地拒绝自身生产包。不得通过
空数组、空对象或 Facade 降级掩盖生产缺口。本卡允许执行一处受控生产闭合修正：从写入前的
实例 RGBWSV raster、材料 ownership、有效变换和 Worker 已校验 Profile 身份生成真实摘要，
随 manifest 原子落盘；旧包缺失证据时仍 fail-closed。该修正不改变 TIFF、RGBWSV 协议、
能力数量或 C ABI。

`package.get_summary` 不暴露报告目录；参考宿主因此只提供冻结的常用报告名候选，默认读取
`slice`，不存在的报告必须显示明确错误，禁止绕过 ABI 读取 manifest 猜测路径。

## 3. 实现分层

```text
HostMainWindow
  -> HostPackageReviewController
       -> ModuleClient::Execute
            -> 五项 package.* 公开能力
  -> HostPackageReviewPanel
       -> 包校验/摘要、层选择、生产层预览、报告、通道图
```

- `HostPackageReviewController`：只处理公开 JSON DTO、错误闭合、预览缓存路径与逐层统计。
- `HostPackageReviewPanel`：只消费宿主 DTO；不解析 TIFF、manifest 或报告文件。
- 切片成功但结果查看失败时，切片作业仍保持成功，结果页单独显示查看错误。
- 通道图显示六条独立曲线，避免把 R/G/B 相加造成覆盖率误读。

## 4. 文件所有权

允许修改：

- `apps/slicer_ui_host_sim/**`
- `tests/hostflow/**`
- `apps/slicer_ui_host_sim/CMakeLists.txt`
- `src/slicer_core/reports/SceneCapabilitySummary.*`（生成真实 capability 摘要）
- `src/slicer_core/pipeline/MultiModelProductionService.cpp`（传递写入前证据）
- `src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.*`（原子持久化摘要）
- 根 `CMakeLists.txt`（登记新增核心源文件）
- 本准备文档、HOSTFLOW 任务清单和 Stage 14 当前状态报告

禁止修改：

- `apps/slicer_debug_ui/**`
- `src/slicer_core/**`（上述受控生产闭合文件之外）
- `src/slicer_module/**`
- `contracts/**`
- `.specstory/**`

## 5. 验收与验证

1. 真实小模型经 H-B-06 生成生产包；
2. verify 为 valid，summary 固定协议字段闭合；
3. descriptor 数量和层索引闭合，六通道统计可绘图；
4. 预览文件由 `package.render_layer_preview` 生成且可由 `QImage` 加载；
5. `slice` 报告可读；不存在包和越界层 fail-closed；
6. Debug/Release H-B-01..07 联合门禁通过；
7. 模块边界守卫、源码尺寸守卫与 `git diff --check` 通过。

## 6. 停止条件

出现以下任一情况必须停止编码并回到合同修订：

- 需要新增第 16 项能力或修改现有 DTO；
- 需要宿主直接链接 `slicer_core`；
- 生产层预览只能依赖调试 PNG；
- 报告名无法通过现有 `package.read_report` 显式失败语义处理。

当前未触发停止条件，`H-B-07` 准备 Gate 与实现 Gate 均为 **PASS**。

## 7. 实施与验证结果

- 参考宿主新增独立“结果”工作区，通过五项冻结 `package.*` 能力完成包校验、摘要、
  逐层 descriptor、生产 TIFF 预览、命名报告和 RGBWSV 六通道图展示；切片成功后自动加载结果。
- 生产 Writer 从写入前实例 raster、ownership、有效变换及 Worker 校验后的 Profile 身份生成
  `perInstance/profileEcho`，旧包缺失证据仍由 `package.get_summary` fail-closed。
- Debug：`hostflow_h[ab]` 16/16 PASS；H-B-07 与 package query 定向门禁 4/4 PASS。
- Release：`hostflow_h[ab]` 与 package query 联合门禁 18/18 PASS。
- Debug/Release：Qt 宿主边界、缺失模块、宿主 smoke 与源码尺寸守卫各 4/4 PASS。
- `python scripts/ValidateSourceSizeGuard.py --base-ref HEAD` PASS，仅保留既有 G4/G5 警告。
