# DOC_DECISION_09B_09A_R2后进入OpenVDB_SDF表面壳层纹理原型阶段

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_09A_R2 之后  
> 建议提交目录：`docs/slicer/`  
> 推荐分支：`spike/09B-openvdb-surface-shell-texture`

---

## 1. 阶段判断

根据 `REPORT_09A_R2_OpenVDB真实Smoke收口当前状态.md`，09A-R2 已完成 OpenVDB 真实依赖链路收口：

```text
VCPKG_ROOT = D:\vcpkg-openvdb
USE_OPENVDB=ON configure 成功
geometry_kernel_demo ON build 成功
openvdb-smoke 成功
OpenVDB version = 12.0.1
activeVoxels = 27
USE_OPENVDB=OFF 默认构建通过
run_ci_quick.ps1 通过
```

因此可以进入：

```text
09B：OpenVDB / SDF 表面壳层纹理原型
```

---

## 2. 分支策略

09A-R2 变更应先提交到：

```text
spike/09-openvdb-sdf-kernel
```

再从该稳定点切出：

```bash
git checkout spike/09-openvdb-sdf-kernel
git status
git checkout -b spike/09B-openvdb-surface-shell-texture
```

09B 不应继续直接堆在 09A 分支上，避免依赖验证与壳层算法原型混杂。

---

## 3. 09B 阶段性质

09B 是真实 OpenVDB 3D SDF 壳层分类和壳层 RGB 应用的实验阶段，不是 production slicer 集成阶段。

目标链路：

```text
Triangle Mesh / Generated Fixture
→ OpenVDB Level Set / SDF
→ Inside / Outer Shell / Interior 分类
→ Shell RGB + Interior Fill Role
→ Experimental Report / Preview
```

---

## 4. 09B 必须验证的核心语义

OpenVDB level set 约定：

```text
phi < 0：模型内部
phi = 0：模型表面
phi > 0：模型外部
```

只允许在模型内部壳层写入 RGB：

```text
-shellThicknessMm <= phi < 0
```

内部填充区域：

```text
phi < -shellThicknessMm
```

模型外部：

```text
phi >= 0
```

09B 不允许通过壳层纹理扩大模型外包络。

---

## 5. 必须保持不变

```text
p0.rgbwsv.2 不变
R G B W S V 通道顺序不变
8-bit / black_is_print 不变
Model > Support > Empty 不变
SupportType 不进入 TIFF channel
USE_OPENVDB=OFF 默认构建继续通过
production slicer_cli 默认路径不变
SupportShapePipeline 不替换
MaterialPolicy 默认行为不变
```

---

## 6. 09B 不做什么

```text
不把壳层结果写入 production RGBWSV TIFF
不替换当前 full-volume texture 行为
不实现 compensated varnish
不做支撑 clearance / overhang
不做设备通信
不做 RIP 半色调
不做 ICC / CMYK
不把 OpenVDB 设为所有构建的强制依赖
```

---

## 7. 完成后路线

09B 完成后根据报告判断：

```text
09B-R1：真实 OBJ/3MF 纹理模型壳层验证与鲁棒性收口
09C：SDF compensated varnish prototype
09D：SDF support clearance / overhang diagnostics
09P：OpenVDB production pipeline 接入设计
```

09B 不直接进入 production 接入。
