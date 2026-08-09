# R-B LOD 缺陷修复选型准备

> 状态：**DECISION READY / IMPLEMENTATION BLOCKED BY RD-B**
> 日期：2026-08-09
> 前置：R-A-01 COMPLETE，真实资产风险 P1 CONFIRMED。

## 1. 问题与影响

当前 `SceneViewMeshBuilder` 通过均匀跳过三角形满足 `triangleLimit`。该方法不是网格简化，
会删除相邻关系中的任意三角形并产生破洞；多材质时还把全局 stride 重复用于每个材质组，
最终三角数与预算可能显著偏离。R-A-01 已确认 47.2% 的真实 OBJ 会触发该路径。

R-B 是 H-D-02 生产 3D 画布的硬前置。俯视 `surfacePreview` 不依赖该 mesh，可继续使用。

## 2. 两个候选方案

| 维度 | A：`meshoptimizer`（推荐） | B：自研保守顶点聚类/边折叠 |
|---|---|---|
| 简化能力 | 提供 `meshopt_simplify` 和属性感知简化，可考虑 UV/法线误差 | 需自行实现拓扑、边界、UV seam、材质边界保护 |
| 质量风险 | 中；需要对甲片轮廓、UV seam 和多材质做项目级门禁 | 高；很容易形成裂缝、非流形或纹理漂移 |
| 集成成本 | 中；新增一个静态 C++ 依赖和 CMake target link | 初期低、长期高；算法、测试和维护全部由项目承担 |
| CMake | 官方支持 CMake；项目采用 `find_package(meshoptimizer CONFIG REQUIRED)` 与 target-based link | 不新增依赖，但需新增独立 core 模块和测试 target |
| vcpkg | 官方 vcpkg 有 `meshoptimizer` port；应写入根 `vcpkg.json` 并沿用锁定 baseline/triplet | 无 vcpkg 变化 |
| 许可证 | MIT，可与本项目分发；仍需在第三方声明中保留版权/许可证 | 无第三方许可证 |
| 部署 | 默认静态链接，避免新增运行时 DLL；需在 Debug/Release 与 x64 triplet 验证 | 无新增 DLL |
| 维护风险 | 上游 API/版本变化，需锁定 baseline 并设适配层 | 算法缺陷完全自担，人员单点和回归成本更高 |

官方资料确认 `meshoptimizer` 提供 C/C++ 接口、CMake 构建与三角形简化，并说明属性感知
简化可把 UV 作为误差属性；vcpkg 当前提供 MIT 许可的 port。正式引入时只使用简化库，
不启用 `gltfpack` feature，避免带入不需要的 BasisU 等依赖。

权威来源：

- `https://github.com/zeux/meshoptimizer`（官方 README、CMake、MIT 许可证与简化 API）；
- `https://github.com/microsoft/vcpkg/tree/master/ports/meshoptimizer`（官方 vcpkg port）；
- `https://raw.githubusercontent.com/microsoft/vcpkg/master/ports/meshoptimizer/vcpkg.json`
  （当前 port 元数据；正式落地仍由项目已锁定的 vcpkg baseline 决定实际版本）。

## 3. 推荐裁决

推荐 **RD-B = meshoptimizer**，理由：

1. 当前缺陷的本质是拓扑不连续，自研“快速修补”最容易重复制造同类问题；
2. 真实资产带 UV、纹理和多材质，属性感知简化比纯几何聚类更符合冻结 ViewData 合同；
3. MIT、CMake 和 vcpkg 路径明确，可通过适配层隔离第三方 API；
4. 单一贡献者项目不适合长期维护完整网格简化算法。

该推荐不是实现授权。用户明确选择 RD-B 之前，不修改 `vcpkg.json`、CMake 或生产源码。

## 4. 实施分解

### R-B-01 多材质预算修复

- 以全实例目标三角数分配各材质组预算，不再对每组重复套用全局 stride；
- 验证总三角数与 `triangleLimit` 偏差不超过 5%；
- 该卡可独立编码，但不能单独解锁 H-D-02，因为破洞仍存在。

### R-B-02 真正网格简化

若选择 meshoptimizer：

1. 在 slicer core 内建立 `MeshSimplifier` 适配边界，不让第三方类型进入公开 DTO；
2. 按材质/纹理连续域生成索引，保护外轮廓、UV seam 与材质边界；
3. 优先使用属性感知简化，禁止 `meshopt_simplifySloppy` 作为生产默认；
4. 达不到目标预算时返回明确降级理由，不得继续跳采样；
5. 计算 Hausdorff/轮廓误差、连通性、孤立三角、UV 边界与 ViewData 字节数。

若选择自研：必须先单独形成算法 DEV 文档和至少同等级门禁，再允许编码。

### R-B-03/04

R-B-03 在 R-B-02 后细分 `truncationReason`。R-B-04 顶点量化是后续性能项，不是
H-D-02 的解锁条件，不与基础简化混在同一提交。

## 5. 验收资产

- `model/obj/aishen_fudiao`、`meigui_fudiao`、`titian_fudiao` 中超过阈值的纹理甲片；
- 小马物语低于阈值资产，验证不应发生无意义简化；
- 多材质 fixture，验证每组预算和 appearance identity；
- 开边界、UV seam、多个 submesh、退化三角 negative fixture。

至少满足：连通、无新增孤立三角、轮廓误差不超过模型尺寸 2%、UV 边界不破裂、
ViewData 不超过 `maxBytes`、Debug/Release 确定性一致。

## 6. 决策门

```text
RD-B = meshoptimizer（推荐）
  → R-B-01 / R-B-02 准备完成，可按独立提交执行
  → R-B-02 PASS 后解锁 H-D-02

RD-B = custom
  → 先补自研算法 DEV/DEMO/任务卡
  → 评审通过后再执行 R-B-01 / R-B-02
```
