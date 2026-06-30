# DOC_DECISION_06A_REPORT06后进入3MF兼容性增强与负向测试

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_06 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：06 多材料输入基础版完成后，先进入 06A 做 3MF 兼容性增强与负向测试

---

## 1. 阶段判断

根据 `REPORT_06_3MF与OBJ_MTL多材料输入当前实现状态.md`，06 阶段已经完成：

```text
1. OBJ/MTL material role mapping；
2. OBJ usemtl / MTL newmtl / Kd / map_Kd 到 RGB/W/V 的基础映射；
3. 3MF stored ZIP package 基础读取；
4. 3MF 3D/3dmodel.model / mesh / components / basematerials/displaycolor 基础解析；
5. materialRoleMapping 统一配置；
6. material_role_mapping_report.json；
7. obj_mtl_material_report.json；
8. three_mf_report.json；
9. quick regression 通过。
```

因此 06 的主目标可以判定为完成。

---

## 2. 为什么不建议立刻进入 06B / 07 / OpenVDB

06 报告也明确当前 3MF 仍属于基础子集，主要限制包括：

```text
1. 仅支持 stored ZIP entry，不支持常见 deflate 压缩 3MF；
2. 不支持 ZIP64；
3. XML 解析仍是轻量字符串解析；
4. 缺少 bad 3MF package 负向测试；
5. 3MF Texture2D / ColorGroup / CompositeMaterials / MultiProperties 尚未支持；
6. Production extension / Beam lattice / Slice extension 尚未支持。
```

其中 1、3、4 会直接影响真实 3MF 文件兼容性和输入安全性。

因此在继续做高级材质、Qt UI、OpenVDB 之前，应先做：

```text
06A：3MF 兼容性增强与负向测试
```

---

## 3. 06A 阶段定位

06A 是 3MF importer 的工程可靠性增强阶段。

06A 重点：

```text
deflate 3MF package 支持
受限 XML parser 替换字符串解析
bad 3MF package 负向测试
3MF extension / unsupported resource 报告增强
3MF importer 安全边界补强
```

06A 不是新的材料策略阶段，也不是新的几何内核阶段。

---

## 4. 06A 不做什么

06A 不做：

```text
OpenVDB / SDF
Qt UI
RIP 半色调
ICC / CMYK
3MF Texture2DGroup 完整贴图采样
PBR / metallic / roughness
OBJ alpha 正式语义
复杂支撑形态优化
真实白墨/光油工艺参数调优
```

---

## 5. 06A 必须保持的冻结项

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
MaterialRoleMapping 语义不变
MaterialPolicy RGB/W/V 语义不变
S support 默认仍由 support system 生成
```

---

## 6. 06A 完成后的后续路线

06A 完成后再判断：

```text
路线 A：06B 3MF Texture2D / ColorGroup 扩展
路线 B：05A 真实模型材料工艺参数验证
路线 C：07 Qt 调试 UI
路线 D：09 OpenVDB / SDF 几何内核预研
```

默认建议：

```text
06A 完成后，如果真实 3MF 输入稳定，再进入 05A 或 07。
```

---

## 7. 结论

06 阶段主功能已经完成，不建议重修 06 主链路。

下一阶段建议执行：

```text
06A：3MF 兼容性增强与负向测试
```

这一步是为了让 3MF 输入从 Demo 子集走向真实工程文件可用。
