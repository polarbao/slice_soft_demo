# ROADMAP_v1.1_REPORT06A后续路线_3MF纹理颜色扩展

> 文档版本：v1.1  
> 文档状态：Draft / 后续路线  
> 适用阶段：REPORT_06A 之后  
> 建议提交目录：`docs/slicer/`

---

## 1. 当前已完成阶段

```text
P0：基础切片闭环
00A：Preview / Report / 导入稳定化
00B：uint8 + black_is_print
00C：Relief V 光油 + S 下表面支撑
01：2.5D / Relief 正式样例路线
02：支撑生成、孤岛检测与 SupportType
03：RGBWSV 协议固化
04：彩色纹理 OBJ/MTL/Texture 基础版
04A：纹理 fallback 与支撑诊断收口
05：MaterialPolicy 白墨/光油策略基础版
03B：TIFF storageMode stripped/tiled 兼容
03C：回归脚本分层与 RIP Reader 输出收口
06：3MF 与 OBJ/MTL 多材料输入基础版
06A：3MF deflate / validation / bad package 增强
```

---

## 2. 当前能力基线

当前项目已经具备：

```text
p0.rgbwsv.2 SlicePackage
stripped / tiled TIFF StorageMode
OBJ/MTL/Texture RGB 输入
OBJ/MTL MaterialRoleMapping
3MF stored + deflate package 读取
3MF basematerial/displaycolor 基础映射
3MF validation 与 bad package
RGB/W/V MaterialPolicy
Support S 通道体系
quick/full/heavy regression
rip_reader_test summary / quiet
```

---

## 3. 下一阶段：06B 3MF Texture2D / ColorGroup 基础支持

目标：

```text
让 3MF 输入支持更接近真实彩色模型的颜色来源：
ColorGroup 与 Texture2DGroup。
```

主要内容：

```text
tinyxml2 或增强 XML reader
3MF colorgroup 解析
3MF texture2d 资源解析
3MF texture2dgroup 解析
triangle property / uv 映射
复用 TextureImage / TextureSampler
three_mf_report 增强
```

---

## 4. 后续阶段建议

### 05A：真实模型材料工艺参数验证

```text
白墨 underbase 参数
光油 top_n_layers 参数
真实美甲模型打印效果反馈
材料组合 profile
```

### 07：Qt 调试 UI

```text
配置编辑
运行切片
查看 preview/reports
模型与切层预览
```

### 06C：3MF CompositeMaterials / MultiProperties

```text
CompositeMaterials
MultiProperties
更复杂材料组合
```

### 08：高级支撑与工艺优化

```text
支撑连通性
支撑小岛处理
支撑膨胀/收缩
支撑形态工艺优化
```

### 09：OpenVDB / SDF 正式几何内核

```text
GeometryKernel 抽象
OpenVDB SDF
Surface shell
厚度/距离场
复杂闭合模型鲁棒体素化
```

---

## 5. 当前不建议提前做

```text
OpenVDB
Qt 完整 UI
RIP 半色调
ICC / CMYK
复杂支撑树
PBR 材质
Production Extension 完整支持
```

---

## 6. 结论

06A 后的最近路线建议为：

```text
06B：3MF Texture2D / ColorGroup 基础支持
```

完成 06B 后，根据业务优先级转入 05A 或 07。
