# CODEX_PROMPT_06B_3MF纹理与ColorGroup执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_06A_3MF兼容性增强与负向测试当前实现状态.md
docs/slicer/DOC_DECISION_06B_REPORT06A后进入3MF纹理与ColorGroup基础支持.md
docs/slicer/ROADMAP_v1.1_REPORT06A后续路线_3MF纹理颜色扩展.md
docs/slicer/PRD_06B_3MF_Texture2D_ColorGroup基础支持.md
docs/slicer/DEV_06B_3MF_Texture2D_ColorGroup_XMLParser设计.md
docs/slicer/DEMO_06B_3MF纹理与ColorGroup验证方案.md
docs/slicer/TASKS_06B_3MF纹理与ColorGroup任务清单.md
```

当前阶段是：

```text
06B：3MF Texture2D / ColorGroup 基础支持
```

目标：

```text
1. 支持 3MF ColorGroup；
2. 支持 3MF Texture2D；
3. 支持 3MF Texture2DGroup；
4. 将 triangle property / UV resolve 到 RGB source；
5. 复用当前 TextureImage / TextureSampler；
6. 增强 three_mf_report 和 texture_report；
7. 增加正向与负向样例；
8. 保持 06A 的 deflate / validation / bad package 能力。
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
MaterialPolicy RGB/W/V 语义不变
S support 默认仍由 support system 生成
```

不要做：

```text
OpenVDB
新的体素内核
完整 PBR
ICC / CMYK
RIP 半色调
Qt UI
Production Extension 完整语义
Beam lattice
CompositeMaterials / MultiProperties 完整语义
texture-driven varnish mask
texture-driven white mask
```

优先实现：

```text
1. XML parser 强化或 tinyxml2 引入；
2. ColorGroup 解析；
3. Texture2D / Texture2DGroup 解析；
4. 3MF 内部 texture resource loading；
5. TextureSampler 复用；
6. three_mf_report / texture_report 增强；
7. ColorGroup / Texture2DGroup 样例；
8. quick regression；
9. REPORT_06B_3MF纹理与ColorGroup当前实现状态.md。
```
