# ROADMAP_v1.3_REPORT05A后续路线_Qt调试UI

> 文档版本：v1.3  
> 文档状态：Draft / 后续路线  
> 适用阶段：REPORT_05A 之后  
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
06B：3MF Texture2D / ColorGroup 基础支持
05A：真实材料工艺参数验证
```

---

## 2. 当前能力基线

当前项目已经具备：

```text
p0.rgbwsv.2 SlicePackage
stripped / tiled TIFF StorageMode
OBJ/MTL/Texture RGB 输入
3MF basematerial / ColorGroup / Texture2DGroup
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
Support S 通道体系
material_process_report.json
material_profile_compare.v1
quick/full/heavy regression
rip_reader_test summary / quiet
```

---

## 3. 下一阶段：07 Qt 调试 UI

目标：

```text
把命令行切片、RIP 验证、报告查看、preview 查看、profile compare 集成到一个本地 Qt 调试工具中。
```

---

## 4. 07 第一版能力

```text
打开配置文件
打开输出 package
运行 slicer_cli
运行 rip_reader_test --summary
运行 run_regression.ps1 -Mode quick
运行 compare_material_profiles.ps1
查看 manifest
查看 reports
查看 preview images
查看 RGB/W/V/S channel summary
查看 material process profile 结果
查看日志与错误码
```

---

## 5. 后续阶段建议

### 07A：Qt 参数编辑与 profile 可视化增强

```text
JSON 表单编辑
MaterialProcessProfile 可视化编辑
topLayers slider
white/varnish 配置面板
```

### 08：支撑形态与工艺优化

```text
支撑小岛
支撑连通性
膨胀/收缩
支撑 preview overlay
```

### 06C：复杂 3MF 材料扩展

```text
CompositeMaterials
MultiProperties
external relationship texture
tinyxml2 正式替换
```

### 09：OpenVDB / SDF 几何内核预研

```text
GeometryKernel 抽象
OpenVDB SDF
Surface shell
复杂闭合模型鲁棒体素化
```

---

## 6. 结论

05A 后优先进入：

```text
07：Qt 调试 UI
```

因为当前最缺的不是新算法，而是高效查看、调参、对比和诊断工具。
