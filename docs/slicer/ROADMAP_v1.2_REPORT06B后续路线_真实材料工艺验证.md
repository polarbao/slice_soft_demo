# ROADMAP_v1.2_REPORT06B后续路线_真实材料工艺验证

> 文档版本：v1.2  
> 文档状态：Draft / 后续路线  
> 适用阶段：REPORT_06B 之后  
> 建议提交目录：`docs/slicer/`

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
```

## 2. 当前能力基线

```text
p0.rgbwsv.2 SlicePackage
stripped / tiled TIFF StorageMode
OBJ/MTL/Texture RGB 输入
OBJ/MTL MaterialRoleMapping
3MF stored + deflate package 读取
3MF basematerial / ColorGroup / Texture2DGroup
RGB/W/V MaterialPolicy
Support S 通道体系
quick/full/heavy regression
rip_reader_test summary / quiet
```

## 3. 下一阶段：05A 真实材料工艺参数验证

目标：

```text
把已有 RGB / W / V / S 输出能力从“功能可用”推进到“工艺 profile 可验证”。
```

重点：

```text
MaterialProcessProfile
真实或准真实模型样例
白墨 underbase profile
光油 top_n_layers profile
RGB + W + V 组合 profile
通道覆盖率与层分布统计
Profile diff / compare
工艺验收报告
```

## 4. 后续阶段建议

```text
07：Qt 调试 UI
06C：复杂 3MF 材料扩展
08：高级支撑与工艺优化
09：OpenVDB / SDF 正式几何内核
```

## 5. 结论

06B 后建议优先进入：

```text
05A：真实材料工艺参数验证
```

完成后再进入 07 UI 或 06C 规范扩展。
