# DOC_DECISION_09B_R2_09B_R1后进入真实模型鲁棒性性能与多材质策略收口

> 文档版本：v0.1
> 文档状态：Decision / 阶段决策
> 适用阶段：REPORT_09B_R1 之后
> 建议提交目录：`docs/slicer/`
> 推荐分支：`spike/09B-R2-shell-robustness-performance`

---

## 1. 阶段判断

09B-R1 已完成真实 importer 数据链路：

```text
OBJ/MTL/PNG 或 3MF Texture2D
→ SceneModel
→ indexed TriangleMeshData + UV/material
→ topology diagnostics
→ OpenVDB level set
→ shell/interior
→ BVH nearest triangle
→ barycentric UV
→ texture/diffuse/fallback RGB
→ report v2 / preview
```

并完成：

```text
OBJ/3MF 等价闭合 fixture
missing texture
no UV
open mesh
non-manifold mesh
OpenVDB ON
OFF CI quick
```

09B-R1 可以收口。

但当前 fixture 仍是低复杂度闭合盒体，尚不能作为 production OpenVDB 壳层纹理接入依据。下一阶段应为：

```text
09B-R2：真实模型鲁棒性、性能/内存与多材质策略收口
```

---

## 2. 为什么不直接进入 09P

当前仍未验证：

```text
真实指甲/浮雕模型
高三角面数模型
薄壁、尖角、小间隙
自交、重复面、局部反向面
多个 connected components
多 material / 多 texture seam
UV seam 与 tie-break
Release 性能与内存
不同 voxel size 的规模增长
```

此外现有拓扑诊断主要覆盖 boundary edge、non-manifold edge 和全局 signed volume；现有内存统计也尚未覆盖 OpenVDB grid、BVH node、texture cache 等主要开销。

因此不应直接接入 production `slicer_cli`。

---

## 3. 09B-R2 阶段目标

```text
1. 建立真实指甲 OBJ/3MF golden fixtures；
2. 建立复杂拓扑与几何负向 fixtures；
3. 扩展 mesh diagnostics；
4. 明确多材质、UV seam 与 nearest hit tie-break 策略；
5. 建立 Release 性能与内存基线；
6. 验证 BVH 在高三角面数量下的收益；
7. 建立体素尺寸/壳层厚度的规模与精度矩阵；
8. 继续保持 production pipeline 隔离。
```

---

## 4. 分支策略

先提交 09B-R1：

```bash
git checkout spike/09B-R1-real-model-shell-texture
git status
git add .
git commit -m "feat(openvdb): complete 09B-R1 real-model shell texture validation"
git push -u origin spike/09B-R1-real-model-shell-texture
```

再切出：

```bash
git checkout -b spike/09B-R2-shell-robustness-performance
```

---

## 5. 必须保持不变

```text
p0.rgbwsv.2 不变
R G B W S V 通道顺序不变
8-bit / black_is_print 不变
Model > Support > Empty 不变
SupportType 不进入 TIFF channel
production slicer_cli 默认路径不变
MaterialPolicy 默认行为不变
SupportShapePipeline 不替换
USE_OPENVDB=OFF 默认构建继续通过
```

---

## 6. 09B-R2 不做什么

```text
不写 production RGBWSV TIFF
不新增正式 surface_shell config 入口
不实现 compensated varnish
不实现 support clearance / overhang
不做自动 remesh / hole repair 生产方案
不保证 warn_and_attempt 输出可生产
不做设备通信、RIP 半色调、ICC
```

---

## 7. 完成后路线

09B-R2 完成后再判断：

```text
09P：OpenVDB production pipeline 接入设计
09C：SDF compensated varnish prototype
09D：SDF support clearance / overhang diagnostics
```

如果真实指甲 golden、复杂拓扑、性能和多材质策略仍未满足门槛，则继续 09B-R3，而不是进入 09P。
