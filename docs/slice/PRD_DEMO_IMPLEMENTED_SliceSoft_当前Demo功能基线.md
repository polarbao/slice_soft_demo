# PRD_DEMO_IMPLEMENTED_SliceSoft_当前Demo功能基线

> 文档版本：v1.0
> 文档状态：Implemented Demo PRD Baseline
> 生成日期：2026-06-30
> 当前阶段：09P-R1 已完成，09P-R2 hardening 前置治理
> 证据范围：当前仓库代码、`CMakeLists.txt`、脚本、测试目录、已归档阶段报告

## 1. 产品定位

当前项目是 UV 3D 打印切片软件的工程化验证仓库，已经从单材料 demo 演进到包含 RGBWSV 输出协议、多格式模型输入、材料/支撑/光油策略、Qt 调试 UI、回归脚本、OpenVDB 表面壳层纹理实验链路的 demo/prototype 系统。

它当前不是完整生产软件。当前核心价值是：

```text
1. 验证 RGBWSV 切片包协议与 RIP 前置数据闭环；
2. 验证 OBJ/MTL/PNG、3MF、Texture2D/ColorGroup 等输入路径；
3. 验证材料策略、白墨、支撑、光油等通道组合；
4. 验证 OpenVDB/SDF 表面壳层纹理实验路线；
5. 为正式产品化阶段提供已实现能力、边界和风险基线。
```

## 2. 已实现用户价值

| 用户 | 已实现价值 |
|---|---|
| 切片算法/工艺工程师 | 可通过 CLI 生成 RGBWSV 包、预览图和报告，验证材料与通道策略 |
| 软件工程师 | 可运行单测、golden、schema、RIP reader、OpenVDB smoke 等回归入口 |
| UI/调试人员 | 可使用 Qt Debug UI 做参数编辑、profile 可视化和 smoke 检查 |
| 架构/质量负责人 | 可通过 report、schema、ProductionAdmissionPolicy 判断实验链路边界 |

## 3. 当前 Demo 已实现功能

### 3.1 RGBWSV 包与 RIP 前置数据

已实现：

```text
RGBWSV 多通道 TIFF 输出
p0.rgbwsv.2 package manifest
channelOrder = R G B W S V
bitDepth = 8
black_is_print 极性
legacy tiled package 兼容读取
rip_reader_test 摘要验证
bad package 负向测试样例
```

当前要求：

```text
不得修改 p0.rgbwsv.2；
不得修改 RGBWSV 通道顺序；
不得修改 uint8 位深；
不得修改 black_is_print 极性。
```

### 3.2 模型输入与纹理

已实现：

```text
OBJ 导入
MTL 导入
PNG 纹理采样
STL 基础几何输入
3MF stored / deflate package 解析
3MF BaseMaterial
3MF ColorGroup
3MF Texture2DGroup
3MF bad package 负向样例
```

当前边界：

```text
真实 OBJ / 3MF 可用于实验诊断和 golden；
真实 OBJ / 3MF 不能因实验路径跑通而直接视为 production-safe。
```

### 3.3 材料、白墨、光油与支撑

已实现：

```text
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
MaterialChannelComposer
TextureApplicationPolicy
VarnishGeometryPolicy
SupportPolicy
SupportShapePolicy / SupportShapePipeline
SupportComponentAnalysis
```

已覆盖能力：

```text
RGB 颜色通道组合；
W 白墨通道策略；
S 支撑通道策略；
V 光油通道策略；
支撑 bridge fixture；
支撑形态报告；
材料工艺参数报告。
```

### 3.4 报告、预览与回归

已实现：

```text
model / slice / package / texture / support / material / preview 等报告
ReportBase / ReportSchema / ReportSchemaValidator
preview PNG 输出
golden summary
schema tests
run_ci_quick.ps1
run_regression.ps1
run_golden_tests.ps1
```

产品意义：

```text
当前 demo 已经具备“可解释输出”的雏形，不只是生成图片或 TIFF。
后续正式项目需要把 report schema 进一步产品化、版本化和 UI 化。
```

### 3.5 Qt Debug UI

已实现：

```text
slicer_debug_ui
配置编辑
profile 可视化
preview overlay
channel chart
UI smoke self-test
```

当前边界：

```text
Qt UI 是调试工具，不是最终生产操作台；
UI 不应绕过 CLI/report 直接调用实验 OpenVDB 内部算法。
```

### 3.6 OpenVDB 表面壳层纹理实验链路

已实现：

```text
USE_OPENVDB 可选构建开关，默认 OFF
OpenVdbAdapter
OpenVdbGeometryKernelService
OpenVdbSurfaceShell
SurfaceShellTexturePrototype
SurfaceShellTextureService
SurfaceTextureTransfer
SurfaceShellRealModelPrototype
MeshTopologyDiagnostics
MeshRobustnessDiagnostics
ProductionAdmissionPolicy
slicer_cli --experimental-openvdb-shell diagnostic path
run_09p_experimental_pipeline_tests.ps1
run_09p_cli_experimental_tests.ps1
```

当前产品边界：

```text
OpenVDB 是 experimental path；
默认不启用；
不能替代 legacy slicer_cli production path；
不能直接写真实 OBJ/3MF production RGBWSV TIFF；
warn_and_attempt 不得 productionAllowed；
confirmed self-intersection 必须 fail fast。
```

## 4. 当前 Demo 非目标

当前 demo 不承诺：

```text
生产级全自动修网；
设备端 RIP / 喷头 / ICC / 半色调联调；
真实 OBJ/3MF 直接 production safe；
OpenVDB 默认启用；
compensated varnish 正式输出；
SDF support clearance 正式输出；
商业级 UI 工作流；
多用户项目管理、作业队列、设备管理。
```

## 5. Demo 转正式项目的产品缺口

| 缺口 | 正式化要求 |
|---|---|
| 输入准入 | 建立 strict / warn / repair_then_strict / reject 的用户可解释策略 |
| 报告稳定性 | report schema 版本化，字段兼容，UI 可读 |
| UI 产品化 | 从 debug UI 演进为作业式工作台 |
| 输出责任边界 | experimental 与 production 输出强隔离 |
| 工艺闭环 | 材料、白墨、光油、支撑策略需要与真实设备/RIP 联调 |
| 性能与内存 | 建立真实模型集合、耗时、内存、失败率门槛 |
| 测试矩阵 | OpenVDB OFF / ON、Debug / Release、Windows 环境分层 |

## 6. 后续产品路线

```text
09P-R2：hardening，收敛 report schema、admission gate、service contract、CI/UI
09P-R3：Qt UI/report/profile 工程化
09P-R4：production candidate gate
Mesh Repair 专项：仅在准入策略清楚后推进
09C：SDF compensated varnish prototype
09D：SDF support clearance diagnostics
10：RIP / 设备 / 工艺联调
```

## 7. 结论

当前 demo 已经证明核心切片协议、材料策略、输入兼容、报告回归和 OpenVDB 表面壳层纹理实验路径具备工程基础。
正式化的关键不再是继续堆功能，而是建立稳定的产品边界：输入能否准入、输出能否解释、报告能否回归、实验路径能否被 UI 和 CI 安全承载。
