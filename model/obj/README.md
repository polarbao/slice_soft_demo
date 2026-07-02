# OBJ 彩色纹理功能性测试模板

> 状态：标准功能性测试模型模板
> 适用范围：OBJ / MTL / Texture 彩色纹理导入、legacy 一键切片、后续 OpenVDB surface-shell OBJ 纹理切片验收。

本目录作为后续 OBJ 彩色纹理模型功能性测试的标准模板目录。

当前模板文件：

```text
MF_aishen_damuzhi_L_tx02.obj
MF_aishen_damuzhi_L_tx02.mtl
T_aishen_damuzhi_L_tx02.png
```

依赖关系：

```text
OBJ: mtllib MF_aishen_damuzhi_L_tx02.mtl
MTL: map_Kd T_aishen_damuzhi_L_tx02.png
```

使用约定：

```text
1. 不移动 OBJ / MTL / PNG 的相对位置。
2. 新增同类 OBJ 模型时，优先保持 OBJ、MTL、贴图同级目录。
3. legacy 功能性测试使用 samples/configs/obj_standard/standard_obj_texture_legacy.json。
4. OpenVDB OBJ 彩色纹理生产候选测试在 11A 阶段计划完成后再新增正式配置。
```

当前用途：

```text
legacy OBJ 彩色纹理切片功能性测试
UI 一键导入模型测试
后续 OpenVDB surface-shell OBJ 纹理转移验收基准
```

标准 legacy 配置会使用：

```text
modelTransform.scale = [0.8, 0.8, 0.8]
autoOrient.maxHeightMm = 6.0
```

原因是该模型原始最小包围盒厚度约 7.16mm，仅靠直角旋转无法落入 6mm 功能性测试高度约束。后续若需要真实尺寸验收，应新增单独的 real-size profile，不要覆盖当前功能性测试模板。
