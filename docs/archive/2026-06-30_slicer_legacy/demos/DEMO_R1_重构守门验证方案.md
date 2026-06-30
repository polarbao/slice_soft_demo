# DEMO_R1_重构守门验证方案

> 文档版本：v0.1  
> 文档状态：Demo / Regression Gate  
> 适用阶段：R1  
> 建议提交目录：`docs/slicer/`

---

## 1. 验证目标

R1 是结构重构阶段，验证重点不是新增输出，而是确认：

```text
1. 所有 target 可构建；
2. 旧 CLI 行为不变；
3. 旧 package 输出可被 RIP reader 读取；
4. quick regression 通过；
5. UI self-test 通过；
6. UI overlay smoke 通过；
7. p0.rgbwsv.2 协议不变。
```

---

## 2. 必须执行命令

```powershell
cmake --build build --config Debug

.\scripts\run_regression.ps1 -Mode quick

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

---

## 3. 关键样例验证

至少保留以下样例通过：

```text
P0 basic
storage stripped default
storage tiled compatibility
support small cases
texture fallback small cases
MaterialPolicy samples
MaterialProcessProfile samples
3MF stored / deflate
3MF ColorGroup
3MF Texture2DGroup
UI overlay smoke fixture
```

---

## 4. 验收标准

```text
所有命令返回 0；
run_regression.ps1 输出 Regression complete. mode=quick；
slicer_debug_ui --self-test 返回 0；
overlay-load-real 返回 PASS；
rip_reader_test summary 仍显示 schema=p0.rgbwsv.2；
没有引入新输出协议。
```
