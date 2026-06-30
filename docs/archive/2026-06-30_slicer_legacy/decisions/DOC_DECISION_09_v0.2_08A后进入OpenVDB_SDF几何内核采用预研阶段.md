# DOC_DECISION_09_v0.2_08A后进入OpenVDB_SDF几何内核采用预研阶段

> 文档版本：v0.2  
> 文档状态：Decision / 阶段决策  
> 适用阶段：08A 完成后  
> 建议提交目录：`docs/slicer/`  
> 推荐分支：`spike/09-openvdb-sdf-kernel`

---

## 1. 阶段判断

08A 已完成支撑桥接 fixture、支撑形态单元测试、SupportShapePipeline facade、真实 3MF support shape profile 与回归脚本收口。

项目现在可以进入：

```text
09：OpenVDB / SDF 几何内核采用预研阶段
```

注意：09 不是生产切片替换阶段，而是 OpenVDB 正式采用前的隔离预研阶段。

---

## 2. 为什么需要修订为 v0.2

09 v0.1 的定位是：

```text
OpenVDB / SDF 可选预研；
USE_OPENVDB=OFF 默认可构建；
OpenVDB adapter 可先 stub。
```

现在项目方向已明确：后续会使用 OpenVDB 作为切片几何内核能力之一。因此 09 v0.2 必须增加：

```text
1. 独立分支策略；
2. OpenVDB 真实依赖接入验证；
3. USE_OPENVDB=ON 构建验证；
4. mesh/heightfield/voxel mask → OpenVDB/SDF → slice mask 最小闭环；
5. OpenVDB 依赖风险记录；
6. 生产 pipeline 隔离红线。
```

---

## 3. 分支策略

必须从当前稳定分支切出：

```bash
git checkout r1-architecture-refactor
git pull
git checkout -b spike/09-openvdb-sdf-kernel
```

分支用途：

```text
OpenVDB / SDF 几何内核采用预研
```

不允许在该分支第一轮直接替换：

```text
slicer_cli 生产切片流程
RGBWSV TIFF 输出流程
SupportShapePipeline
现有 scanline / 2D mask pipeline
```

---

## 4. 阶段目标

09 v0.2 的目标是建立一个可隔离验证的 OpenVDB/SDF 几何内核实验通道：

```text
Scene / Mesh / Heightfield / VoxelMask
→ GeometryKernelBoundary
→ DistanceField / OpenVDB Grid / SDF
→ SliceMask / ShellMask / Diagnostics
→ geometry_kernel_report.json
→ preview PNG
```

第一轮不接入生产 RGBWSV 输出。

---

## 5. 必须保持不变

```text
p0.rgbwsv.2 不变
R G B W S V 通道顺序不变
8-bit / black_is_print 不变
Model > Support > Empty 不变
SupportType 不进入 TIFF channel
MaterialPolicy / MaterialRoleMapping / MaterialProcessProfile 语义不变
slicer_cli 默认生产路径不变
rip_reader_test 不受影响
run_ci_quick.ps1 不受影响
```

---

## 6. OpenVDB 采用策略

09 v0.2 使用双轨构建策略：

```text
默认轨：USE_OPENVDB=OFF
  主项目无 OpenVDB 时仍可完整构建；
  geometry kernel demo 使用纯 C++ DistanceField2D / stub adapter。

验证轨：USE_OPENVDB=ON
  在可用 OpenVDB 环境下验证真实 OpenVDB 编译、链接、运行链路；
  记录依赖、构建方式和失败风险。
```

---

## 7. 09 不做什么

```text
不将 SDF/OpenVDB 结果写入生产 RGBWSV TIFF
不替换当前 slicer.cpp 主流程
不替换 SupportShapeOptimizer
不实现 production surface_shell_texture
不实现 production compensated_varnish
不引入设备通信
不做 RIP 半色调
不做 ICC / CMYK
不强制所有开发环境必须安装 OpenVDB
```

---

## 8. 完成后路线

09 完成后，根据报告判断是否进入：

```text
09A：SDF surface shell texture prototype
09B：SDF compensated varnish prototype
09C：SDF support clearance / overhang diagnostics
10：RIP / 设备链路真实集成
```

09 本身只做几何内核采用预研。
