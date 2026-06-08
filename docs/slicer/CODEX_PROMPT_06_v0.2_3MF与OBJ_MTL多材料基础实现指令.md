# CODEX_PROMPT_06_v0.2_3MF与OBJ_MTL多材料基础实现指令

> 文档版本：v0.2  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_03C_回归与RIP输出收口当前实现状态.md
docs/slicer/DOC_DECISION_06_v0.2_3MF与OBJ_MTL多材料输入阶段修订.md
docs/slicer/PRD_06_v0.2_3MF与OBJ_MTL多材料输入基础版.md
docs/slicer/DEV_06_v0.2_3MFImporter_OBJMTLMaterialMapping设计.md
docs/slicer/DEMO_06_v0.2_3MF与OBJ_MTL多材料输入验证方案.md
docs/slicer/TASKS_06_v0.2_3MF与OBJ_MTL多材料任务清单.md
```

当前阶段是：

```text
06：3MF 与 OBJ/MTL 多材料输入基础版
```

目标：

```text
1. 建立统一 MaterialRoleMapping；
2. 让 3MF material/color 映射到 RGB/W/V；
3. 让 OBJ usemtl + MTL newmtl/Kd/map_Kd 映射到 RGB/W/V；
4. 复用现有 slicing / texture / MaterialPolicy / support / TIFF / RIP pipeline。
```

必须保持：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
black_is_print
Model > Support > Empty
SupportType 不进入 TIFF 通道
MaterialPolicy RGB/W/V 语义不变
S support 默认仍由 support system 生成
```

优先实现：

```text
1. materialRoleMapping 配置；
2. MaterialRole enum 和 mapping rules；
3. OBJ/MTL material role mapping；
4. obj_mtl_material_report.json；
5. 3MF zip + 3dmodel.model 基础解析；
6. 3MF material/color role mapping；
7. three_mf_report.json；
8. material_role_mapping_report.json；
9. OBJ/MTL 与 3MF 小型样例；
10. quick regression；
11. REPORT_06_3MF与OBJ_MTL多材料输入当前实现状态.md。
```

不要做：

```text
OpenVDB
新的体素内核
完整 3MF texture2dgroup
PBR
Production Extension
Beam Lattice
Slice Stack
Qt UI
RIP 半色调
ICC / CMYK
复杂支撑形态优化
texture-driven varnish mask
texture-driven white mask
```
