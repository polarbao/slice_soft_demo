# ROADMAP_v1.0_REPORT06后续路线_3MF兼容性优先

> 文档版本：v1.0  
> 文档状态：Draft / 后续路线  
> 适用阶段：REPORT_06 之后  
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
```

---

## 2. 当前能力基线

当前项目已经具备：

```text
p0.rgbwsv.2 SlicePackage
stripped / tiled TIFF StorageMode
OBJ/MTL/Texture RGB 输入
OBJ/MTL MaterialRoleMapping
3MF stored package 基础输入
RGB/W/V MaterialPolicy
Support S 通道体系
quick/full/heavy regression
rip_reader_test summary / quiet
```

---

## 3. 下一阶段：06A 3MF 兼容性增强与负向测试

目标：

```text
让 3MF importer 从基础样例可用，提升到更接近真实 3MF 文件可用。
```

重点：

```text
deflate ZIP 支持
受限 XML parser
bad 3MF package
unsupported extension report
material id / object reference 负向测试
安全边界
```

---

## 4. 后续阶段建议

### 06B：3MF Texture / ColorGroup / 高级材质扩展

```text
3MF texture2d
texture2dgroup
colorgroup
与当前 TextureSampler 对接
```

### 05A：真实模型材料工艺参数验证

```text
白墨 underbase 参数
光油 top_n_layers 参数
真实美甲模型打印反馈
材料组合 profile
```

### 07：Qt 调试 UI

```text
配置编辑
运行切片
查看 preview/reports
模型与切层预览
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
3MF Production Extension 完整支持
```

---

## 6. 结论

06 后的最近路线建议为：

```text
06A：3MF 兼容性增强与负向测试
```

完成 06A 后，再根据业务优先级选择 05A、06B 或 07。
