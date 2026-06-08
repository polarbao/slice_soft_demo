# CODEX_PROMPT_04A_纹理阶段收口修复指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_04_彩色纹理切片当前实现状态.md
docs/slicer/DOC_DECISION_04A_REPORT04后纹理阶段收口修复.md
docs/slicer/PRD_04A_纹理Fallback样例与支撑割裂诊断修复.md
docs/slicer/DEV_04A_纹理FallbackFixture与支撑诊断设计.md
docs/slicer/DEMO_04A_纹理Fallback与支撑诊断验证方案.md
docs/slicer/TASKS_04A_纹理阶段收口任务清单.md
```

当前任务不是 05 材料策略，而是 04A 收口修复。

请完成：

```text
1. 重建小型 missing texture fixture
2. 重建小型 no-UV fixture
3. 修改 fallback configs 指向小型 fixture
4. 确保 fallback 用例 rip_reader_test 快速通过
5. run_regression.ps1 覆盖 texture fallback
6. 增加 support connectivity diagnostics
7. 生成 REPORT_04A_纹理阶段收口修复当前实现状态.md
```

必须保持：

```text
schema = p0.rgbwsv.1
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
```

不要做：

```text
白墨/光油材料策略
texture-driven varnish
color_shell_volume
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
```

验收：

```text
TexturedReliefRgb pass
TexturedMissingTextureFallback pass 且 missingTextures > 0 / fallbackPixels > 0
TexturedNoUvFallback pass 且 facesWithUv = 0 / facesWithoutUv > 0 / fallbackPixels > 0
run_regression.ps1 pass
support connectivity diagnostics 输出
```
