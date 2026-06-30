# DEV_MASTER_SliceSoft_正式切片软件总体架构与实现路线

> 文档版本：v0.1
> 文档类型：产品级总 DEV / 技术总体方案
> 适用项目：Slice Soft / UV 彩色多材料切片软件
> 当前基线：`spike/09B-R3-shell-production-readiness`
> 当前阶段判断：09B-R3 已完成，下一阶段进入 09P OpenVDB 表面壳层纹理实验生产管线接入
> 建议提交目录：`docs/slicer/`

---

## 1. 文档目的

本 DEV 是当前切片软件项目的总体技术实现方案，用于统一：

```text
模型导入
几何内核
纹理转移
材料策略
支撑策略
光油策略
RGBWSV 输出
RIP Reader
Qt Debug UI
配置/Profile
报告/Schema
测试/CI
```

本文件不是 OpenVDB 专项文档，而是整个切片软件的工程架构总文档。

---

## 2. 总体架构分层

建议正式项目分为 10 层：

```text
[1] Application Layer
    slicer_cli
    slicer_debug_ui
    demo apps
    unit test apps

[2] Config/Profile Layer
    SliceConfig
    ConfigSchema
    ConfigMigration
    NormalizedConfig
    MaterialProcessProfile

[3] Import Layer
    OBJ Importer
    MTL Importer
    3MF Importer
    Texture Image Loader

[4] Scene Layer
    SceneModel / ModelReport
    MaterialInfo
    TriangleTextureInfo
    BoundingBox / Triangle / TexCoord

[5] Geometry Layer
    Legacy Raster Geometry
    OpenVDB Level Set
    SDF Shell / Interior / Distance
    Topology Diagnostics
    Nearest Triangle Query

[6] Feature Policy Layer
    TextureApplicationPolicy
    MaterialPolicy
    SupportPolicy
    VarnishGeometryPolicy
    MaterialRoleMapping

[7] Pipeline Layer
    SlicePipeline
    PipelineContext
    PipelineStepResult
    Legacy Pipeline
    OpenVDB Experimental Pipeline

[8] Output Layer
    RGBWSV Package
    TIFF Writer / Reader
    Manifest
    RIP Reader

[9] Diagnostics Layer
    ReportBase
    ReportSchema
    ReportWriter
    Preview
    Golden

[10] Engineering Layer
    CMake
    scripts
    CI quick/extended/benchmark
    docs/slicer
```

---

## 3. 当前代码能力对应关系

当前最新阶段：

```text
当前最新阶段：09B-R3 已完成
当前工作分支基线：spike/09B-R3-shell-production-readiness
下一阶段：09P OpenVDB 表面壳层纹理实验生产管线接入
```

09B-R3 已完成 production-readiness pre-admission diagnostics，包括 narrow-phase 自相交、稳定 issue code、repeat/clamp 纹理边界 fixture、Windows process peak working set 和真实模型 topology production admission 策略。

09B-R3 没有接入 production `slicer_cli`，没有写 production RGBWSV TIFF，没有修改 `p0.rgbwsv.2`，没有修改 RGBWSV 通道顺序、uint8 位深和 `black_is_print` 极性。真实 OBJ / 3MF 当前仍不得直接视为 production-safe。

下一阶段 09P-R1 只做 experimental path / feature flag / diagnostic / report，不默认启用 OpenVDB，不替代 legacy production path。

### 3.1 Application Layer

当前包含：

```text
apps/slicer_cli
apps/rip_reader_test
apps/geometry_kernel_demo
apps/surface_shell_texture_demo
apps/surface_shell_real_model_demo
apps/slicer_debug_ui
```

后续需要：

```text
surface_shell_robustness_demo
production experimental slicer path
Qt UI OpenVDB report viewer
```

### 3.2 Config/Profile Layer

当前包含：

```text
src/slicer_core/config.*
src/slicer_core/config/ConfigSchema.*
src/slicer_core/config/ConfigMigration.*
src/slicer_core/config/NormalizedConfig.*
src/slicer_core/materials/process_profile.*
```

后续需要：

```text
geometry_kernel config
surface_shell_texture config
failure_policy
mesh_policy
report config
benchmark profile
```

### 3.3 Import/Scene Layer

当前包含：

```text
OBJ / MTL importer
3MF importer
Texture2D / ColorGroup 基础能力
SceneModel = ModelReport
MaterialInfo
TriangleTextureInfo
```

后续原则：

```text
Importer 只负责导入，不直接产生 OpenVDB grid；
SceneModel 是几何与材料策略的统一输入。
```

### 3.4 Geometry Layer

当前包含：

```text
DistanceField2D
ShellMask
OpenVdbAdapter
OpenVdbLevelSetBuilder
OpenVdbSurfaceShell
TriangleMeshData
SceneModelTriangleMeshAdapter
MeshTopologyDiagnostics
NearestTriangleQuery
```

后续需要：

```text
MeshScaleTolerance
MeshRobustnessDiagnostics
TriangleIntersectionQuery
OpenVdbGeometryKernelService
SupportClearanceSdf
VarnishCompensationSdf
```

### 3.5 Texture / Material Layer

当前包含：

```text
TextureApplicationPolicy
SurfaceShellTexturePrototype
SurfaceShellRealModelPrototype
SurfaceTextureTransfer
SurfaceAttributeMap
MaterialPolicy
MaterialRoleMapping
VarnishGeometryPolicy
```

后续需要：

```text
SurfaceShellProductionPolicy
MaterialChannelComposer
OpenVdbShellToRgbwsvBridge
MultiTextureSeamPolicy
VarnishCompensationPolicy
```

### 3.6 Output / Report / CI

当前包含：

```text
RgbwsvPackage
tiff_io
rip_reader
ReportSchema
ReportWriter
run_ci_quick.ps1
run_openvdb_smoke.ps1
run_surface_shell_texture_tests.ps1
run_surface_shell_real_model_tests.ps1
```

后续需要：

```text
OpenVDB production golden
SurfaceShell production report
Benchmark report
OpenVDB CI matrix
Qt report viewer
```

---

## 4. 核心数据流

### 4.1 Legacy 默认切片路径

```text
SliceConfig
→ load_model_report
→ legacy slicer / raster mask
→ TextureApplicationPolicy legacy mode
→ MaterialPolicy
→ RGBWSV Package
→ TIFF Writer
→ RIP Reader test
→ Report
```

该路径必须保持默认可用。

### 4.2 OpenVDB 实验路径

```text
SliceConfig
→ load_model_report
→ SceneModelTriangleMeshAdapter
→ MeshTopologyDiagnostics
→ OpenVdbLevelSetBuilder
→ OpenVdbSurfaceShell
→ NearestTriangleQuery
→ SurfaceTextureTransfer
→ SurfaceShellRealModelReport
→ Preview
```

当前该路径还不写 production RGBWSV。

### 4.3 OpenVDB Production 候选路径

```text
SliceConfig
→ load_model_report
→ OpenVdbGeometryKernelService
→ SurfaceShellTextureService
→ MaterialChannelComposer
→ RGBWSV Package
→ TIFF Writer
→ Manifest
→ RIP Reader
→ Production Report
```

该路径应在 09P-R1 后才开始实现。

---

## 5. 模块边界设计

### 5.1 Importer 边界

Importer 负责：

```text
读取模型
解析材质
解析纹理引用
解析 UV
输出 SceneModel
记录 warnings/errors
```

Importer 不负责：

```text
OpenVDB grid
SDF shell
RGBWSV channel
支撑
光油
设备逻辑
```

### 5.2 Geometry Kernel 边界

Geometry Kernel 负责：

```text
level set
SDF
inside/shell/interior/outside
distance
nearest triangle
topology diagnostics
```

Geometry Kernel 不负责：

```text
纹理采样策略
材料角色
TIFF 写入
RIP
UI
```

### 5.3 Texture Transfer 边界

Texture Transfer 负责：

```text
shell voxel → nearest triangle
barycentric UV
texture sample
diffuse fallback
configured fallback
统计来源
```

Texture Transfer 不负责：

```text
决定白墨/光油/支撑
决定 RGBWSV 优先级
写 package
```

### 5.4 Material Policy 边界

Material Policy 负责：

```text
根据几何分类和工艺参数决定 RGB/W/S/V role
处理 Model > Support > Empty 优先级
处理 base/shell/support/varnish 角色
```

Material Policy 不应直接依赖 OpenVDB 类型。

### 5.5 Output 边界

Output 负责：

```text
RGBWSV channel
TIFF
manifest
RIP Reader compatibility
```

Output 不做策略判断。

---

## 6. 建议新增核心抽象

### 6.1 GeometryKernelService

```cpp
enum class GeometryKernelEngine {
    Legacy,
    OpenVdb
};

struct GeometryKernelOptions {
    GeometryKernelEngine engine{GeometryKernelEngine::Legacy};
    double voxel_size_mm{0.05};
    std::string mesh_policy{"strict_closed"};
    std::string failure_policy{"fail_fast"};
};

struct GeometryKernelResult {
    bool ok{false};
    MeshDiagnostics mesh;
    SurfaceShellClassification shell;
    InteriorClassification interior;
    DistanceDiagnostics distance;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

class IGeometryKernelService {
public:
    virtual ~IGeometryKernelService() = default;
    virtual GeometryKernelResult Run(const SceneModel&, const GeometryKernelOptions&) = 0;
};
```

实现：

```text
LegacyGeometryKernelService
OpenVdbGeometryKernelService
```

### 6.2 SurfaceShellTextureService

```cpp
struct SurfaceShellTextureServiceOptions {
    double shell_thickness_mm{0.10};
    std::string shell_region{"outer_surface"};
    std::string fill_role{"base"};
    std::array<std::uint8_t, 3> fallback_rgb{255, 255, 255};
    double max_transfer_distance_mm{0.0};
};

struct SurfaceShellTextureServiceResult {
    VoxelMask3D shell_mask;
    VoxelMask3D interior_mask;
    std::vector<Rgb8> shell_rgb;
    TextureTransferStats transfer_stats;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};
```

### 6.3 MaterialChannelComposer

```cpp
struct MaterialChannelComposerInput {
    ModelMask model;
    SupportMask support;
    SurfaceShellTextureServiceResult shell_texture;
    VarnishMask varnish;
    MaterialPolicyResult material_policy;
};

RgbwsvPackage ComposeMaterialChannels(const MaterialChannelComposerInput&);
```

作用：

```text
将 shell texture、support、varnish、base material 统一组合到 RGBWSV。
```

---

## 7. Config Schema 技术路线

### 7.1 保持旧配置兼容

现有配置模式继续有效：

```text
solid_volume_from_top_surface
top_surface_only
top_surface_band
```

### 7.2 新增 experimental config

建议新增：

```json
{
  "geometry_kernel": {
    "enabled": true,
    "engine": "openvdb",
    "voxel_size_mm": 0.05,
    "mesh_policy": "strict_closed",
    "failure_policy": "fail_fast"
  },
  "texture": {
    "apply_mode": "surface_shell",
    "shell_thickness_mm": 0.10,
    "shell_region": "outer_surface",
    "fill_role": "base",
    "fallback_rgb": [255, 255, 255],
    "sampler": "bilinear",
    "uv_address_mode": "clamp",
    "max_transfer_distance_mm": 0.0
  }
}
```

### 7.3 Migration

ConfigMigration 应支持：

```text
旧配置不变；
新字段缺省值可预测；
experimental 字段默认不开启；
OpenVDB 不可用时有清晰错误。
```

---

## 8. OpenVDB 接入策略

### 8.1 构建策略

保持：

```text
USE_OPENVDB=OFF 默认
USE_OPENVDB=ON 显式开启
```

不能让 OpenVDB 成为所有开发环境的强制依赖。

### 8.2 运行策略

早期 production experimental：

```text
geometry_kernel.enabled = true
engine = openvdb
failure_policy = fail_fast
```

后续可支持：

```text
fallback_legacy
diagnostic_only
```

### 8.3 依赖策略

继续使用：

```text
D:\vcpkg-openvdb
vcpkg manifest feature openvdb
```

文档记录到：

```text
OPENVDB_DEPENDENCY_NOTES.md
```

---

## 9. Topology / Robustness 技术路线

09B-R2 应从基础拓扑升级到生产前诊断：

```text
boundaryEdges
nonManifoldEdges
duplicateFaces
oppositeDuplicateFaces
localWindingInconsistency
selfIntersectionPairs
connectedComponents
zeroVolumeComponents
minEdgeLength
minTriangleArea
maxAspectRatio
thinFeatureWarnings
```

拓扑策略：

```text
strict_closed:
  不可接受拓扑直接失败

warn_and_attempt:
  允许实验执行，但 report 标记 nonProduction

diagnostic_only:
  只输出诊断，不切片
```

---

## 10. Nearest Triangle / Seam 技术路线

当前 BVH 应扩展：

```text
query_count
visited_nodes
tested_triangles
max_visited_nodes
node_count
estimated_bytes
```

Tie-break：

```text
1. distance
2. barycentric interior margin
3. source triangle index
```

UV seam：

```text
命中哪一个 triangle，就使用该 triangle 的 UV。
不跨 seam 平均。
```

Material seam：

```text
命中 triangle 的 material 是唯一来源。
不跨 material seam 自动混色。
```

---

## 11. 材料与通道组合路线

未来 production 组合顺序建议：

```text
1. Empty 初始化
2. Model base / interior
3. Surface shell RGB
4. White ink W
5. Varnish V
6. Support S
7. Priority resolver
```

必须保持：

```text
Model > Support > Empty
SupportType 不进入 TIFF channel
```

如果 support 和 model 冲突，应由 MaterialChannelComposer 明确处理，而不是分散在各模块中。

---

## 12. Report Schema 技术路线

### 12.1 当前实验报告

```text
p0.geometry_kernel_report.1
p0.surface_shell_texture_report.1
p0.surface_shell_texture_report.2
```

### 12.2 建议 production 报告

```text
p0.slicer_job_report.1
p0.openvdb_geometry_report.1
p0.material_composition_report.1
p0.rgbwsv_package_report.1
```

### 12.3 Report 统一字段

```text
schema
jobId
input
config
engine
meshDiagnostics
geometryStats
textureStats
materialStats
supportStats
varnishStats
output
warnings
errors
timings
memory
```

---

## 13. Testing / CI 技术路线

### 13.1 Unit

```text
config schema
mesh topology
nearest triangle
texture sampling
material policy
support policy
varnish policy
report schema
RGBWSV package
```

### 13.2 Smoke

```text
slicer_cli default
rip_reader_test
geometry_kernel_demo
openvdb-smoke
surface_shell_texture_demo
surface_shell_real_model_demo
```

### 13.3 Golden

```text
legacy package golden
RGBWSV manifest golden
OpenVDB generated-box report golden
OBJ/3MF shell report golden
production package golden
```

### 13.4 Benchmark

```text
Release only
non-blocking
records performance trend
does not use strict time equality
```

---

## 14. Qt Debug UI 技术路线

Qt UI 后续应通过 report 和 output artifacts 展示，而不是直接内嵌复杂切片逻辑。

UI 需要支持：

```text
config/profile 加载
模型摘要
拓扑诊断
壳层/内部/支撑/光油 overlay
纹理转移统计
warning/error 展示
preview 图层切换
输出路径打开
RIP Reader 摘要
```

---

## 15. Production Pipeline 接入步骤

### 15.1 09P：Experimental pipeline boundary

产物：

```text
experimental path
feature flag
diagnostic/report
service abstraction
```

### 15.2 09P-R1：experimental implementation

实现：

```text
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer bridge
experimental slicer_cli diagnostic/report path
report output
preview output
不直接写真实 OBJ/3MF 的 production RGBWSV TIFF
```

### 15.3 09P-R2：production hardening

完成：

```text
schema validation
golden package
Qt UI integration
failure/fallback
documentation
CI matrix
```

---

## 16. 当前阶段开发建议

当前 09B-R3 已完成，下一阶段 09P-R1 应优先实现：

```text
1. feature flag / experimental path；
2. ProductionAdmissionPolicy；
3. OpenVdbGeometryKernelService；
4. SurfaceShellTextureService；
5. MaterialChannelComposer bridge；
6. slicer_cli experimental diagnostic/report；
7. 09P 验证脚本；
8. 保持 production RGBWSV 协议不变。
```

不应在 09P-R1 中做：

```text
默认启用 OpenVDB
替代 legacy slicer_cli production path
写真实 OBJ/3MF 的 production RGBWSV TIFF
修改 p0.rgbwsv.2
修改 RGBWSV 通道顺序、uint8 位深和 black_is_print 极性
把 warn_and_attempt 输出声明为 production-safe
compensated varnish
support clearance
Qt UI production 改造
```

---

## 17. 风险与控制

### 17.1 技术风险

```text
OpenVDB 对复杂非流形模型不稳定
纹理 nearest triangle 在 seam 处不稳定
高分辨率 voxel 导致内存爆炸
BVH 性能不足
配置组合复杂
report schema 膨胀
```

### 17.2 控制策略

```text
strict_closed 默认
feature flag
diagnostic report
golden fixtures
Release benchmark
fallback policy
CI 分层
```

---

## 18. 总体结论

当前项目已经具备正式化基础，但尚未具备直接 production 接入条件。

推荐技术路线：

```text
09B-R3：生产准入前诊断策略收口，已完成
→ 09P：OpenVDB 表面壳层纹理实验生产管线接入
→ 09P-R1：experimental path / feature flag / diagnostic / report
→ 09P-R2：production hardening
→ 09C / 09D：光油补偿与支撑 SDF 继续扩展
```

09B-R3 没有接入 production `slicer_cli`，没有写 production RGBWSV TIFF，没有修改 `p0.rgbwsv.2`，没有修改 RGBWSV 通道顺序、uint8 位深和 `black_is_print` 极性。

真实 OBJ / 3MF 当前仍不得直接视为 production-safe。09P-R1 仍应保持 OpenVDB 默认关闭，并把 OpenVDB 壳层纹理输出限制在 experimental path / diagnostic / report。
