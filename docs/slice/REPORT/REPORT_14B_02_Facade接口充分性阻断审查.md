# REPORT_14B-02 Facade 接口充分性阻断审查

> 文档状态：**RESOLVED BY 14B-01-R1 / 14B-02 READY**
> 日期：2026-08-05
> 对应任务：`14B-02`
> 本轮范围：仅审查 `ModelFacade`、`PackageQueryFacade` 是否足以无损包装既有模型导入与包读取能力

## 1. 结论

审查时不能在不修改内部 public C++ DTO 的前提下完成 `14B-02`。本轮未开始 Facade 实现，未修改
CLI、生产 TIFF、CMake、`TASKS_14` 或 Stage 14 总状态报告。

阻断原因不是既有模型导入和包读取服务不可复用，而是 `14B-01` 生成的内部 C++ DTO 无法承载
`contracts/slicer_capability_dtos.json` v1.2 已冻结的全部必需响应字段。若现在直接实现，只能丢字段、
伪造值，或让后续 DLL 再次绕过 Facade 读取 manifest/TIFF，三种做法都违反 Stage 14 合同。

## 2. 已确认可复用能力

| 能力 | 既有权威实现 | 可复用结论 |
|---|---|---|
| OBJ/STL/3MF 导入 | `load_model_report(ModelLoadConfig, configDir)` | 可复用，位于 `slicer_base` |
| 严格包验证 | `validate_slice_package(packageDir)` | 可复用，返回 schema、网格、层校验和与通道统计 |
| manifest 层索引与 TIFF 解码 | `TiffLayerSource` | 可复用，按真实 layerIndex 读取生产 TIFF |
| 生产预览合成 | `MaterialPreviewComposer` | 可复用，伪彩仅用于显示，不改变生产值 |
| 报告文件读取 | `Json::parse` 与受限 package 路径解析 | 可复用，但 Facade 需要结构化返回合同 |

## 3. Public DTO 阻断项

### 3.1 `PackageSummary` 无法表达冻结响应

`PackageSummary` 当前只有 package 路径、schema、层数、宽高和 DPI。能力 DTO v1.2 还要求：

- `packageIdentity`；
- 固定 `channels`、`bitDepth`、`polarity`；
- `perInstance[]`；
- `profileEcho`。

这些字段不能由 14C 的 JSON 适配层在不绕过 Facade 的情况下恢复。

### 3.2 `LayerDescriptor` 缺少层合同字段

当前 DTO 缺少 `widthPx`、`heightPx`、`emptyPixels[6]` 与 `storageMode`。其中 storage mode 和
empty pixel 统计是 `package.get_layer_descriptor` 的单次响应必需字段，不能用默认值代替。

### 3.3 `VerifyResult` 丢失严格验证证据

当前 DTO 只有 `valid` 与 `warnings`，但 v1.2 要求：

- 结构化 `errors[].code/message`；
- `perLayerChecksum[]`；
- `layerCount`。

既有 `validate_slice_package` 已产生逐层六通道 checksum；若 Facade 不返回该证据，后续 DLL 只能
再次直接调用 Reader，破坏 `PackageQueryFacade` 的唯一读取边界。

### 3.4 `ReadReport` 返回类型不闭合

接口当前返回原始 `std::string`，而 v1.2 响应要求 `reportName`、`reportSchema`、结构化 `data` 与
`sourcePath`。可保留原文作为内部字段，但需要一个结构化 `ReportResult` 才能闭合合同。

### 3.5 模型法线元数据缺少权威来源

`ModelMetadata` 要求 `has_normals`，但既有 `ModelReport` 不保留法线数量或 `has_normals`。OBJ
解析器虽解析 face 中的 normal index，却没有把法线存在性写入报告。实现层若恒写 `false` 会使
带法线 OBJ 的 `model.import/get_metadata` 响应失真；重新解析 OBJ 又违反“严格复用既有导入能力”。

此外，`ModelImportRequest.compute_bbox/extract_materials` 当前只能控制返回内容，既有 loader 仍会
执行 bbox 与 MTL 提取。该差异不阻断正确性，但应在受控修订时明确其性能语义。

## 4. 最小修订建议

应新建 `14B-01-R1`（或等价受控修订），只补齐内部 C++ Facade DTO，不改变 ABI v1、15 项能力、
`p0.rgbwsv.2` 或生产路径：

1. 为 `PackageSummary`、`LayerDescriptor`、`VerifyResult` 补齐 v1.2 必需字段；
2. 新增结构化 `PackageReportResult`，让 `ReadReport` 返回该类型；
3. 让 `ModelReport` 权威记录 normal count/has normals，并映射到 `ModelMetadata`；
4. 增加自动合同测试，逐项比对 C++ DTO 与 `slicer_capability_dtos.json` v1.2；
5. 修订获授权并冻结后，再实现 `src/slicer_core/api/implementation`。

这是一项内部 DTO 补全，不需要新增能力、导出符号或协议版本。

## 5. 边界与未执行项

- 未修改任何冻结 public DTO，因为当前任务明确要求接口不足时先报告。
- 未创建不完整的 `ModelFacade` / `PackageQueryFacade` 实现，避免把缺字段实现误标为可用。
- 未运行 Facade 单元测试，因为本轮没有可满足合同的实现目标。
- 既有合同静态验证可继续运行；它们只能证明外部合同自身有效，不能消除本报告指出的 C++ DTO
  承载缺口。

## 6. 退出条件

`14B-02` 只有在以下条件全部满足后才能恢复：

```text
INTERNAL_DTO_V12_ALIGNED = PASS
MODEL_NORMAL_METADATA    = AUTHORITATIVE
FROZEN_PROTOCOL_DRIFT    = ZERO
PUBLIC_DTO_AMENDMENT     = USER AUTHORIZED
```

## 7. 解除记录

2026-08-05 已由受控任务 `14B-01-R1` 完成上述五项修订：Package DTO 完整承载 v1.2、
`ReadReport` 改为结构化结果、模型报告补齐源法线证据，并增加 Debug/Release 自动门禁。
外部 SPI、能力数量与生产协议未变化。`14B-02` 可恢复开发。
