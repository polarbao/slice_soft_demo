# REPORT_14B_02 ModelFacade 与 PackageQueryFacade 实现状态

> 更新时间：2026-08-05
> 当前验证基线：`8ddf1eb`
> 任务：Stage 14B-02
> 状态：`IMPLEMENTATION_COMPLETE / BASE_LAYERING_GATE_PASS`

## 1. 本次范围

本次在不修改冻结 public DTO、不修改 CLI、不改变生产 TIFF、不修改根 `CMakeLists.txt` 的约束下，完成以下 Qt-free Facade 实现：

- `ModelFacade`：模型导入、权威元数据读取、句柄缓存与释放。
- `PackageQueryFacade`：包摘要、层描述、生产 TIFF 预览、严格校验和报告读取。
- 14B-02 独立行为测试：直接编译本任务实现文件；测试夹具通过既有 Writer 生成，查询实现来自 `slicer_base`。
- 14B-02 base-only 链接测试：只链接 `slicer_base`，证明 Model/PackageQuery/preview/TIFF Reader 不需要 `slicer_engine` 符号。

## 2. ModelFacade 实现

### 2.1 权威能力复用

- 模型解析严格复用 `load_model_report`，未复制 OBJ/STL/3MF 解析规则。
- `has_normals`、UV、材质、纹理、包围盒均来自 `ModelReport` 权威结果。
- `appearanceIdentity` 复用 `ComputeSceneResourceHash`。
- `sourceDigest` 对源文件原始字节计算 SHA-256。
- `meshIdentity` 对权威三角面数据按稳定二进制序列计算 SHA-256，不伪造必需字段。

### 2.2 生命周期与错误

- 导入成功后分配进程内 `ModelId`，支持并发安全的 `GetMetadata` 与 `Release`。
- 空路径、文件不存在、解析失败、资源耗尽、取消和未知异常均映射到稳定 `PM-SLICER-*` 错误。
- 现有 loader 不提供处理中断回调，因此取消检查位于模型加载、源摘要和元数据生成的边界；不声称支持解析器内部的即时取消。

## 3. PackageQueryFacade 实现

### 3.1 包摘要与层描述

- `GetSummary` 先执行 `validate_slice_package`，再通过 `TiffLayerSource` 建立生产包索引。
- schema、网格、DPI、通道、位深、层数和包身份均来自严格校验或生产包索引。
- capability v1.2 必需的 `perInstance[]` 与 `profileEcho` 必须真实存在并满足最小结构合同；缺失时 fail-closed，不补默认值。
- `GetLayerDescriptor` 只读取 manifest 列出的真实层，并以生产 TIFF 解码后的通道统计返回 `printPixels`、`emptyPixels`。

### 3.2 生产 TIFF 预览

- 预览数据源严格为生产 TIFF，复用 `TiffLayerSource` 与 `MaterialPreviewComposer`，不读取 preview PNG。
- 支持 R/G/B/W/S/V 单通道以及 RGB、RGBW、RGBS、RGBV、RGBWSV 合成模式。
- 支持 BMP、PNG、PPM 输出；显示缩放采用最近邻，仅影响显示产物，不修改生产像素。
- cache key 包含 package identity、manifest hash、layer、mode、channels、max width 和预览语义版本。
- TIFF 解码期间通过现有 `TiffLayerLoadControl` 传递取消请求。

### 3.3 校验与报告

- `Verify` 复用 `validate_slice_package`；合法包返回逐层六通道 checksum，协议错误返回 `valid=false` 与既有 `ValidationErrorCode`。
- 包不存在或 manifest 缺失属于 API 输入失败，不伪装成合法的负向校验结果。
- `ReadReport` 只允许读取 manifest `reports` 映射中登记的逻辑报告名。
- 报告路径必须位于包目录内，报告根必须为 JSON object 且必须包含非空 `schema`。

### 3.4 G1 职责拆分修正

原 `PackageQueryFacadeImplementation.cpp` 同时承载包查询、预览合成、图像编码和报告读取，达到 1057 行，违反新增源文件 500 行上限。现已按职责拆分，DTO v1.2 和外部行为保持不变：

- `PackageQueryFacadeImplementation.cpp`：15 行，仅保留 Facade 工厂。
- `PackageQueryFacadeCommon.cpp`：102 行，统一承载路径、JSON 和错误映射公共设施。
- `PackageQueryFacadePackage.cpp`：332 行，承载摘要、层描述和严格校验。
- `PackageQueryFacadePreview.cpp`：268 行，承载预览请求解析和生产 TIFF 合成流程。
- `PackageQueryFacadePreviewEncoding.cpp`：257 行，承载显示缩放与 BMP/PNG/PPM 编码。
- `PackageQueryFacadeReport.cpp`：119 行，承载 manifest 约束下的报告读取。
- `PackageQueryFacadeInternal.h`：87 行，仅声明内部服务和共享设施，不扩展冻结 public DTO。
- 专属测试 `Main.cpp` 同步收口至 500 行，测试生命周期辅助类型移入 60 行的 `TestSupport.h`。

### 3.5 base / engine 架构阻断修正

按照 `REPORT_14B_00` 第 4 节与 `DEV_14` 第 5 节完成最小职责拆分：

- 新增 Qt-free 的 `TiffReadApi.h`，只暴露 TIFF 公共只读类型、模式转换和 RGBWSV Reader API。
- `tiff_io.cpp` 收口为只读实现，不再包含 Writer factory、手写编码或 LibTIFF 写出符号，并被分配到 `slicer_base`。
- `ProductionLayerRef`、`TiffLayerSource`、`TiffLayerCache`、`MaterialPreviewComposer` 整组分配到 `slicer_base`；其工程内依赖只指向 base 文件。
- 兼容头 `tiff_io.h` 保留既有 Writer API 声明，继续由 engine 侧生产调用者使用，不破坏现有 read/write 源码接口。
- 手写 stripped/tiled Writer 与 PackBits 编码移入 `output/tiff/HandwrittenTiffWriter.cpp` 及窄 internal codec；Writer factory 路由和 `write_rgbwsv_tiff` 兼容入口留在 `slicer_engine`。
- `LibTiffWriter` 及 backend 选择保持 engine 所有权，没有形成 `slicer_base -> slicer_engine` 链接或符号依赖。

生成的分层清单关键结果：

```text
base   preview/ProductionLayerRef
base   preview/MaterialPreviewComposer
base   preview/TiffLayerCache
base   preview/TiffLayerSource
base   tiff_io.cpp
engine output/tiff/HandwrittenTiffWriter.cpp
engine output/tiff/TiffWriterFactory.cpp
engine tiff_io.h
```

本次新增或重构的实现文件均不超过 500 行：Reader 主实现 414 行，手写 Writer 主实现 249 行，其余窄 API/codec 文件为 68 至 199 行。

## 4. 验证结果

### 4.1 独立编译与单元验证

以下命令均在 `feature/14-slicer-capability-package`、当前验证基线 `8ddf1eb` 上实际执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/stage14b_02/RunTests.ps1 -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File tests/stage14b_02/RunTests.ps1 -Configuration Release
```

Debug 与 Release 均为 `PASS`，覆盖 5 组行为：

1. `model_facade_uses_authoritative_loader`
2. `model_facade_fails_closed`
3. `package_summary_and_layer_descriptor_are_authoritative`
4. `package_preview_uses_production_tiff`
5. `verify_and_report_reading_are_strict`

此外，以下 CMake 目标在 Debug 与 Release 均完成编译和运行：

```powershell
cmake --build build-slicesoft/main --config Debug --target slicer_base slicer_engine package_query_facade_14b02_base_link_test package_query_facade_14b02_unit_tests --parallel
cmake --build build-slicesoft/main --config Release --target slicer_base slicer_engine package_query_facade_14b02_base_link_test package_query_facade_14b02_unit_tests --parallel
```

- `package_query_facade_14b02_base_link_test`：仅链接 `slicer_base`，Debug/Release 运行退出码均为 0。
- `package_query_facade_14b02_unit_tests`：Debug/Release 均为 5/5 PASS。
- 独立行为测试仍链接 engine 仅用于在测试进程中创建生产 TIFF fixture；这不是 PackageQuery 的运行依赖，base-only 链接门禁已单独覆盖该边界。
- TIFF Reader/Writer、RIP、layer source/cache 与材料预览相关 10 项 CTest：Debug 10/10 PASS，Release 10/10 PASS。

### 4.2 既有门禁

- `ValidateCapabilityDtos.py`：PASS。
- `ValidateThreeLaneContract.py`：PASS。
- `ValidateStage14BFacadeDtos.py`：PASS。
- `ValidateStage14BLayeringFeasibility.py`：PASS。
- `ValidateStage14BTargetGraph.py --assignment build-slicesoft/main/stage14b_layer_assignment.txt`：PASS。
- `ValidateSourceSizeGuard.py --base-ref HEAD`：PASS，G1/G3 阻断已消除；输出的 33 项均为仓库既有 G4/G5 警告。
- `cmake --build build-slicesoft/main --config Debug --target slicer_base slicer_engine --parallel`：PASS。
- `ValidateStage14BPreparation.py`：未通过；当前总准备状态报告缺少该脚本要求的旧文本 `CURRENT_NEXT_TASK = 14B-02 / 14B-03 / 14B-04（可并行）`。该文件不属于 14B-02 独占写入范围，本任务未修改。

## 5. 保持不变的边界

- 本次架构修正未修改根 `CMakeLists.txt`；仅更新专用分层映射与对应合同验证器。当前工作区中的 14B-02 target 登记由并行集成工作提供，不计入本任务改动。
- 未修改 TASKS_14、Stage 14 总状态报告、14B-03/04 文件。
- 未修改 public DTO、SPI v1、11 个 `pm_*` 导出和 15 项能力集合。
- 未修改 CLI、生产 RGBWSV TIFF、位深、通道顺序、极性、压缩或 manifest 生产逻辑。
- 未引入 Qt 依赖，未复制模型解析、RIP 校验、TIFF 解码或材料预览业务规则。

## 6. 未决集成问题

1. 既有生产包若没有持久化 capability v1.2 的 `perInstance[]` 与 `profileEcho`，`GetSummary` 会按合同拒绝，而不会伪造数据。后续 SliceFacade/包生产适配需负责真实落盘。
2. `load_model_report` 与 `validate_slice_package` 当前没有细粒度取消接口；本实现只在可复用能力允许的位置检查取消。
3. `ModelImportRequest::compute_bbox` 的现有 loader 总会生成权威包围盒；本任务未复制 loader 或人为删除结果。`extract_materials=false` 仅控制 Facade 返回的材质明细，不改变权威解析。
4. 仓库现有 `.gitignore` 使用 `tests/*` 忽略新测试目录，因此 `tests/stage14b_02/*` 不会出现在普通 `git status` 中；后续提交所有者需显式纳入专属测试文件，不能漏交。

## 7. 结论

14B-02 的独立实现、专属测试和 base/engine 架构阻断修正已经完成。ModelFacade 与 PackageQueryFacade 均复用现有权威服务；PackageQuery、生产 TIFF Reader、缓存和材料预览现在可由纯 `slicer_base` 编译链接，Writer 与 backend 选择继续留在 `slicer_engine`。Qt、CLI、生产 TIFF 字节合同和 DTO v1.2 行为保持不变。
