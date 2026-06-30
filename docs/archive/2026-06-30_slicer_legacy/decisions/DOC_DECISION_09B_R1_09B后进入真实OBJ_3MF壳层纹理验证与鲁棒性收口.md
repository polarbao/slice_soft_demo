# DOC_DECISION_09B_R1_09B后进入真实OBJ_3MF壳层纹理验证与鲁棒性收口

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_09B 之后  
> 建议提交目录：`docs/slicer/`  
> 推荐分支：`spike/09B-R1-real-model-shell-texture`

---

## 1. 阶段判断

根据 `REPORT_09B_OpenVDB_SDF表面壳层纹理原型当前状态.md`，09B 已完成：

```text
generated-box TriangleMeshData
→ OpenVDB meshToLevelSet
→ inside / shell / interior 分类
→ constant/checker shell RGB
→ report / preview
```

并已验证：

```text
OpenVDB 12.0.1
activeVoxels = 18061
insideVoxels = 4641
shellVoxels = 2652
interiorVoxels = 1989
outsideColoredVoxels = 0
unclassifiedVoxels = 0
shell + interior = inside
厚度单调性测试通过
OFF run_ci_quick.ps1 通过
```

09B 可以收口为：

```text
OpenVDB 3D SDF 壳层分类与程序化 RGB 原型完成
```

但当前不应直接进入 production pipeline 接入，也不建议立即展开 compensated varnish。

下一阶段应为：

```text
09B-R1：真实 OBJ/3MF 纹理模型壳层验证与鲁棒性收口
```

---

## 2. 为什么必须先做 09B-R1

09B 只验证了 generated-box，并使用 constant/checker RGB。尚未验证：

```text
1. 现有 SceneModel 到 TriangleMeshData 的转换；
2. 真实 OBJ/MTL/PNG 输入；
3. 真实 3MF Texture2D 输入；
4. 壳层 voxel 到源三角面的最近表面查询；
5. 重心坐标 UV 插值；
6. 材质纹理、diffuse color 与 fallback 的优先级；
7. 开口、非流形、退化三角形、方向错误；
8. 薄壁、尖角、小特征和高三角面数；
9. OBJ 与 3MF 同几何输入的一致性；
10. 真实模型性能与内存风险。
```

这些问题不解决，不能证明 `surface_shell_texture` 能服务真实业务模型。

---

## 3. 阶段目标

09B-R1 目标：

```text
1. 将现有 SceneModel 转换为 OpenVDB 可用的 TriangleMeshData；
2. 保留源 triangle → UV/material 的映射；
3. 实现 shell voxel → nearest source triangle 查询；
4. 使用重心坐标插值 UV；
5. 复用现有 texture_image sampler；
6. 验证真实 OBJ/MTL/PNG；
7. 验证真实 3MF Texture2D；
8. 增加 mesh topology/quality diagnostics；
9. 增加负向 fixture；
10. 仍然不接入 production RGBWSV TIFF。
```

---

## 4. 分支策略

先提交 09B：

```bash
git checkout spike/09B-openvdb-surface-shell-texture
git status
git add .
git commit -m "feat(openvdb): complete 09B surface shell texture prototype"
git push -u origin spike/09B-openvdb-surface-shell-texture
```

再切出：

```bash
git checkout -b spike/09B-R1-real-model-shell-texture
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

## 6. 09B-R1 不做什么

```text
不写 production RGBWSV TIFF
不新增正式 surface_shell 配置入口
不替换 full-volume texture
不实现 compensated varnish
不实现 support clearance / overhang
不做自动 mesh 修复器
不保证所有非流形/开口模型均可成功生成可靠 SDF
不做设备通信、RIP 半色调、ICC
```

---

## 7. 完成后路线

09B-R1 完成后，根据状态报告选择：

```text
09B-R2：性能、内存与复杂拓扑专项收口
09P：OpenVDB production pipeline 接入设计
09C：SDF compensated varnish prototype
09D：SDF support clearance / overhang diagnostics
```

推荐只有在真实 OBJ/3MF、UV transfer 和负向测试均通过后，才进入 09P。
