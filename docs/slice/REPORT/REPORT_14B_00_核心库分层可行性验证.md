# REPORT_14B-00 核心库分层可行性验证

> 状态：**COMPLETE / GATE PASS**  
> 日期：2026-08-05  
> 任务：14B-00  
> 权威准备：`DOC_PREP_14B_核心Facade与BaseEngine分层实施准备.md`

## 1. 结论

```text
14B-00_LAYERING_FEASIBILITY = PASS
model.import                = base
currentTarget               = slicer_core (301 source entries)
targetGraph                 = slicer_engine -> slicer_base
knownExtractionEdges        = 4
```

`model.import` 的唯一归属结论为 **base**。`model.cpp` 的实现只直接包含
`model.h` 与 miniz，不依赖 OpenVDB、Qt、生产 Writer 或切片算法。新增的独立编译探针
直接编译 `model.cpp + miniz`，不链接 `slicer_core`，并能读取 OBJ fixture。当前阻碍它
直接进入 base 的不是算法依赖，而是 `model.h` 经 `config.h` 暴露了完整 `SliceConfig`；
14B-01/01A 必须先抽取窄化的导入 DTO，不得把完整 engine 配置带进 base。

## 2. 当前构建图

```text
slicer_cli ---------------------------> slicer_core
apps/slicer_debug_ui (Qt Widgets) ----> slicer_core
tests / tools ------------------------> slicer_core

slicer_core
  source entries: 301
  public include: src, src/third_party/miniz
  Windows private libs: windowscodecs, ole32, psapi
  optional private libs: TIFF::TIFF, OpenVDB::openvdb
```

当前单库把轻量读取、场景 DTO、几何查询、生产写包、修复与 OpenVDB 混装。14B-01A
应先保留 `slicer_core` 为兼容 `INTERFACE` target，再形成：

```text
slicer_engine -> slicer_base
slicer_core (compatibility interface) -> slicer_engine
```

禁止 `slicer_base -> slicer_engine`，也禁止 base/engine 依赖 Qt。

## 3. 文件级归属

完整 301 项归属证据见：
`docs/slice/REPORT/evidence/STAGE14B_00_SlicerCoreLayerAssignment.txt`。
该清单由 `ValidateStage14BLayeringFeasibility.py --list` 根据当前 CMake source 列表
确定性生成；新增或移除 source 未更新规则时，门禁会失败。

| 区域 | base | engine / 后续切分 |
|---|---|---|
| 模型导入 | `model.*`、miniz、`importers/` | 抽离完整 `SliceConfig` 后迁入 base |
| 场景与排版 | `scene/`、`layout/` | `SceneEffectiveConfig.*` 保留 engine |
| 轻量几何 | `TriangleMeshData`、`MeshTopologyDiagnostics`、两个 Model Adapter | OpenVDB、repair、SDF、重诊断留 engine |
| 包读取 | `rip_reader.*`、`RgbwsvPackage` DTO、通用 JSON/Schema | `tiff_io.cpp` 与 Writer 留 engine；Reader 接口需拆出 |
| 报告 | `ReportBase`、`ReportSchema`、`ReportSchemaValidator` | 报告生成器、生产写出与纹理诊断留 engine |
| 诊断 | `Diagnostics`、`ValidationIssue`、`ProductionAdmissionPolicy` | 修复、材质闭合生成和基准诊断留 engine |
| 纹理显示数据 | `texture_image.*`、场景 ViewGeometry | 切片纹理转移和生产材质计算留 engine |

## 4. 已识别跨界引用

当前确定存在 4 条 base 候选到 engine 的 include 边，均有明确抽取路径：

| base 候选 | 当前 engine 依赖 | 14B 处理 |
|---|---|---|
| `model.h` | `config.h` | 14B-01 定义 Qt-free `ModelImportRequest/Options` |
| `ObjImporter.h` | `config.h` | 改用相同窄导入 DTO |
| `ThreeMfImporter.h` | `config.h` | 抽取 3MF 导入所需最小选项 |
| `rip_reader.cpp` | `config.h` | PackageQuery 传入协议预期值，不读取生产 `SliceConfig` |

另外，`tiff_io.h/.cpp` 同时承载 Reader/Writer。14B-02 应把只读 TIFF API 放入 base，
Writer factory 与 LibTIFF/手写 Writer 实现留在 engine。不得通过让 base 链接整个 Writer
解决符号问题。

## 5. 编译探针

新增 `model_import_layering_probe`，其 source 仅为：

```text
tests/contracts/ModelImportLayeringProbe.cpp
src/slicer_core/model.cpp
src/third_party/miniz/*.c
```

该 target 不链接 `slicer_core`。探针使用
`samples/models/textured/fixtures/policy_textured_small.obj`，验证导入结果包含非零顶点和
三角面。它证明 `model.import` 可在抽离配置 DTO 后留在 base，不需要改由 Worker 承载。

## 6. 后续约束

1. 14B-01 先定义窄 DTO 和 Facade，不修改生产 TIFF。
2. 14B-01A 再落地两库；同一 `.cpp` 不得同时编入两库。
3. 14B-02 拆 Reader/Writer 边界并实现 Model/PackageQuery Facade。
4. 14C-04 的 `syncCapabilities[]` 保留 `model.import`，承载方式为 inprocess。
5. 若后续出现新的 base -> engine include，必须先抽取窄接口；不得反向链接。

## 7. 验证

```text
python tests/contracts/ValidateStage14BLayeringFeasibility.py
cmake --build <build-dir> --config Debug --target model_import_layering_probe
ctest --test-dir <build-dir> -C Debug -R "stage14b_layering|model_import_layering" --output-on-failure
```

合同脚本同时固定 source 总量、逐文件归属、4 条已知抽取边和 `model.cpp` 的依赖上限。

