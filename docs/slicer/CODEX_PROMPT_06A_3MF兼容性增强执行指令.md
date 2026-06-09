# CODEX_PROMPT_06A_3MF兼容性增强执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_06_3MF与OBJ_MTL多材料输入当前实现状态.md
docs/slicer/DOC_DECISION_06A_REPORT06后进入3MF兼容性增强与负向测试.md
docs/slicer/ROADMAP_v1.0_REPORT06后续路线_3MF兼容性优先.md
docs/slicer/PRD_06A_3MF兼容性增强与负向测试.md
docs/slicer/DEV_06A_3MFDeflate_XMLParser_BadPackage设计.md
docs/slicer/DEMO_06A_3MF兼容性与负向测试验证方案.md
docs/slicer/TASKS_06A_3MF兼容性增强任务清单.md
```

当前阶段是：

```text
06A：3MF 兼容性增强与负向测试
```

目标：

```text
1. 支持 deflate 3MF package；
2. 保留 stored 3MF package 支持；
3. 用受限 XML parser 或封装 XML reader 替换散落字符串解析；
4. 增加 bad 3MF package 负向测试；
5. 增强 three_mf_report.json；
6. 保持 06 的 OBJ/MTL material mapping 和 MaterialRoleMapping 语义不变。
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
完整 3MF Texture2DGroup
PBR
Production Extension 完整语义
Qt UI
RIP 半色调
ICC / CMYK
复杂支撑形态优化
材料策略重写
```

优先实现：

```text
1. ZIP deflate reader；
2. 3MF XML reader/parser 封装；
3. 3MF reference validation；
4. bad 3MF package；
5. three_mf_report 增强；
6. quick regression；
7. REPORT_06A_3MF兼容性增强与负向测试当前实现状态.md。
```
