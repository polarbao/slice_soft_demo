# DOC_DECISION_06_v0.2_3MF与OBJ_MTL多材料输入阶段修订

> 文档版本：v0.2  
> 文档状态：Decision  
> 适用阶段：REPORT_03C 之后  
> 建议提交目录：`docs/slicer/`

## 1. 修订结论

06 阶段不再只定义为：

```text
3MF 与多材料输入基础版
```

而应修订为：

```text
06：3MF 与 OBJ/MTL 多材料输入基础版
```

也就是说，06 阶段同时包含：

```text
1. 3MF material/color → RGB/W/V 材料角色映射；
2. OBJ usemtl + MTL newmtl/Kd/map_Kd → RGB/W/V 材料角色映射；
3. 建立统一 MaterialRoleMapping 层；
4. 后端继续复用现有 slicing / texture / MaterialPolicy / support / p0.rgbwsv.2 pipeline。
```

## 2. 为什么要把 OBJ/MTL 多材料映射纳入 06

原因：

```text
1. OBJ/MTL 是当前项目已经支持的重要输入路径；
2. 04 阶段已经支持 OBJ/MTL/Texture RGB 纹理；
3. 05 阶段已经建立 RGB/W/V MaterialPolicy；
4. OBJ 的 usemtl 与 MTL 的 newmtl/Kd/map_Kd 已具备基础材料语义；
5. 业务中可能会通过材质名表达 white / varnish / clear / ignore 等角色；
6. 如果 06 只做 3MF，会导致 3MF 与 OBJ/MTL 使用两套材料映射逻辑。
```

因此 06 应优先建立统一的：

```text
MaterialRoleMapping
```

由 3MF 与 OBJ/MTL 共用。

## 3. 06 阶段新边界

06 必须支持：

```text
3MF:
  package / model / object / component / transform / material / color

OBJ/MTL:
  usemtl / newmtl / Kd / map_Kd / face material assignment

统一映射:
  rgb / white / varnish / ignore / support_candidate
```

06 不做：

```text
完整 3MF texture2dgroup
完整 3MF Production Extension
PBR / metallic / roughness
OBJ alpha / transparency 正式语义
texture-driven varnish mask
texture-driven white mask
OpenVDB / SDF
Qt UI
RIP 半色调
ICC / CMYK
复杂支撑形态优化
```

## 4. S 支撑原则

默认不允许输入材料直接写 S：

```text
S support 默认仍由 support system 生成。
```

如果输入材料名包含 support：

```text
allowInputSupportMaterial = false:
  记录为 support_candidate
  不写 S
  report 中输出 warning
```

只有显式配置：

```json
{
  "materialRoleMapping": {
    "allowInputSupportMaterial": true
  }
}
```

后续才允许输入材料写 S。06 第一版不建议开启该能力。

## 5. 冻结项

06 必须保持：

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
MaterialPolicy RGB/W/V 语义不变
```

## 6. 结论

本次更新后，06 的准确名称为：

```text
06：3MF 与 OBJ/MTL 多材料输入基础版
```

核心是统一输入材料角色映射，而不是新增后端切片算法。
