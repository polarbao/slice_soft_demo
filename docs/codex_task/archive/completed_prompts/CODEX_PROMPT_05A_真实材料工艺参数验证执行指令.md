# CODEX_PROMPT_05A_真实材料工艺参数验证执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_06B_3MF纹理与ColorGroup当前实现状态.md
docs/slicer/DOC_DECISION_05A_REPORT06B后进入真实材料工艺参数验证.md
docs/slicer/ROADMAP_v1.2_REPORT06B后续路线_真实材料工艺验证.md
docs/slicer/PRD_05A_真实材料工艺参数验证.md
docs/slicer/DEV_05A_MaterialProcessProfile与工艺验证报告设计.md
docs/slicer/DEMO_05A_真实材料工艺参数验证方案.md
docs/slicer/TASKS_05A_真实材料工艺参数验证任务清单.md
```

当前阶段是：

```text
05A：真实材料工艺参数验证
```

目标：

```text
1. 新增 MaterialProcessProfile；
2. 新增 material_process_report.json；
3. 验证 RGB + W + V profile；
4. 验证 W underbase；
5. 验证 V top_n_layers；
6. 支持 profile compare；
7. 使用 3MF Texture2DGroup 与 OBJ/MTL Texture 样例验证；
8. 接入 quick regression。
```

必须保持：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
black_is_print
Model > Support > Empty
MaterialRoleMapping 语义不变
MaterialPolicy RGB/W/V overlay 语义不变
S support 仍由 Support pipeline 独立生成
```

不要做：

```text
OpenVDB
新的体素内核
Qt UI
RIP 半色调
ICC / CMYK
喷头 bitstream
设备通信
3MF CompositeMaterials 完整语义
PBR
支撑形态大改
```

优先实现：

```text
1. materialProcessProfile 配置；
2. material_process_report.json；
3. top1/top2/top3 RGB+W+V 样例；
4. 3MF Texture2DGroup + RGBWV profile；
5. OBJ/MTL Texture + RGBWV profile；
6. compare_material_profiles.ps1；
7. quick regression；
8. REPORT_05A_真实材料工艺参数验证当前实现状态.md。
```
