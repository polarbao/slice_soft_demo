# DEMO_04A_纹理Fallback与支撑诊断验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：04A  
> 建议提交目录：`docs/slicer/`

---

## 1. 验证目标

验证 04A 收口修复：

```text
1. TexturedReliefRgb 主样例仍通过
2. Missing texture fallback 被真实触发
3. No UV fallback 被真实触发
4. Fallback 用例快速通过 rip_reader_test
5. 支撑割裂能输出 report 诊断
```

---

## 2. 验证命令

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_relief_rgb.json
build\Debug\rip_reader_test.exe --package output\TexturedReliefRgb

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_missing_texture_fallback.json
build\Debug\rip_reader_test.exe --package output\TexturedMissingTextureFallback

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_no_uv_fallback.json
build\Debug\rip_reader_test.exe --package output\TexturedNoUvFallback

.\scripts\run_regression.ps1
```

---

## 3. Missing Texture 验收

检查：

```text
reports/texture_report.json
```

必须满足：

```text
missingTextures > 0
warnings 非空
fallbackPixels > 0
rip_reader_test pass
```

---

## 4. No UV 验收

检查：

```text
reports/model_report.json
reports/texture_report.json
```

必须满足：

```text
facesWithUv = 0
facesWithoutUv > 0
fallbackPixels > 0
rip_reader_test pass
```

---

## 5. 支撑割裂诊断验收

针对 `TexturedReliefRgb`，报告应至少能定位：

```text
support component count
largest support component
small support component count
tiny support component count
```

如果仍出现小支撑岛，04A 可以接受，但必须在 report 中显示，不再只依赖人工观察。

---

## 6. 回归 Checklist

- [ ] TexturedReliefRgb pass
- [ ] TexturedMissingTextureFallback pass
- [ ] TexturedNoUvFallback pass
- [ ] missing texture 真实触发 fallback
- [ ] no UV 真实触发 fallback
- [ ] fallback 样例不使用 38MB 大模型
- [ ] run_regression.ps1 pass
- [ ] support connectivity diagnostics 输出
- [ ] RGBWSV 协议不变

---

## 7. 非目标

不验证：

```text
材料策略
白墨/光油组合
3MF
OpenVDB
Qt UI
RIP 半色调
ICC
```
