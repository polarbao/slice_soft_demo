# DOC_DECISION_14B-01-R1 Facade DTO 与能力合同 v1.2 对齐修订

> 文档状态：**ACCEPTED / IMPLEMENTATION AUTHORIZED**
> 日期：2026-08-05
> 任务：14B-01-R1
> 外部权威合同：`contracts/slicer_capability_dtos.json` v1.2、`contracts/slicer_capability_dtos.md` v1.2

## 1. 背景

14B-01 建立了 Qt-free Facade 外壳，但在 14B-02 实现审查时发现，首版内部 C++ DTO
没有完整承载已冻结能力合同 v1.2 的 package 字段。若直接实现，会被迫丢弃字段、在实现中
旁路返回 JSON，或让模块层重新读取生产包，均会破坏 Facade 单一边界。

本修订只修正**内部 C++ DTO 的承载充分性**，不修改外部能力数量、C ABI、SPI 版本、
生产 TIFF 或包协议。

## 2. 差异与裁定

| 能力 | 14B-01 缺口 | 14B-01-R1 裁定 |
|---|---|---|
| `package.get_summary` | 缺 `packageIdentity`、通道协议、`perInstance`、`profileEcho` | 补齐网格、固定 RGBWSV 协议及结构化 JSON 对象承载 |
| `package.get_layer_descriptor` | 缺尺寸、`emptyPixels`、`storageMode` | 补齐对应字段 |
| `package.verify` | 只有 warnings | 补齐结构化 errors、逐层六通道 checksum、layerCount |
| `package.read_report` | 仅返回裸字符串 | 改为 `PackageReport`，包含名称、Schema、对象数据和来源路径 |
| `model.import/get_metadata` | `hasNormals` 无权威来源 | 模型导入报告记录源法线是否存在并被面引用 |

未知结构的 `perInstance`、`profileEcho` 与 report `data` 使用经过校验的 UTF-8 JSON 对象
承载。这样保持 base 层 Qt-free，不向公开 Facade 头引入引擎配置或 ABI JSON 类型。

## 3. 冻结边界

以下项目保持不变：

- `PM_SPI_VERSION=1`、11 个 `pm_*` 导出、15 项能力；
- `p0.rgbwsv.2`、`R,G,B,W,S,V`、uint8、`black_is_print`；
- `contracts/slicer_capability_dtos.*` v1.2，不创建 v1.3；
- `DOC_DECISION_14A_04_R1` 与 ViewData v1.2，不弱化双视图纹理要求；
- Writer、RIP Reader、OpenVDB 默认值与生产像素不变。

## 4. 实施与门禁

1. 修订 `PackageDtos.h`、`PackageQueryFacade.h` 和模型元数据来源；
   OBJ face token 解析抽取到 base 内部 `ObjFaceParser`，避免既有 `model.cpp` 净增长；
2. 增加 DTO v1.2 字段门禁和带法线 OBJ 导入探针；
3. Debug/Release 编译并运行 Facade、模型导入和 Stage 14 合同测试；
4. 完成后才解除 14B-02 的 DTO 阻断，14B-03/04 不受影响。

禁止用默认值伪造成功响应。实现无法从权威生产包或模型来源得到必需字段时，必须返回稳定的
`PM-SLICER-*` 错误。
