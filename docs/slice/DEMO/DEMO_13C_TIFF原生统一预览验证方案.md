# DEMO_13C TIFF 原生统一预览验证方案

> 文档版本：v0.1
> 文档状态：Formal DEMO / PREPARED
> 生成日期：2026-07-24

## 1. 验证目标

验证 UI 不依赖逐通道 preview PNG，也能从生产 TIFF 正确显示所有材料通道、组合叠加、真实层序和
六通道像素值。

## 2. Fixture

准备最小 RGBWSV package：

```text
RGB only；
W only；
S only；
V only；
RGB+W；
RGB+S；
RGB+V；
RGB+S+W+V；
真实空白；
同像素多通道；
stripped；
tiled；
600/600；
635/600；
坏 TIFF；
缺失 layer。
```

fixture 的 TIFF 和 manifest 必须由共享 writer 生成，不手工伪造无法被 RIP strict 接受的生产包。

## 3. 用例

### Case 13C-01 无 preview 目录

```text
package 只保留 layers/*.tiff 和 reports/manifest；
加载生产预览；
浏览首层、中间层、末层；
R/G/B/W/S/V 均可显示；
不得提示“找不到 preview”作为失败原因。
```

### Case 13C-02 全材料叠加

```text
选择 RGB+S+W+V；
确认四类材料图例；
确认同层合成；
点击 RGB、W、S、V、Empty 和多通道像素；
探针值与 TIFF Reader 一致。
```

### Case 13C-03 层序和物理比例

```text
首层显示最低 zMm；
末层显示最高 zMm；
不跨层兜底；
600/600 像素方形；
635/600 按物理尺寸校正显示。
```

### Case 13C-04 伪彩设置

```text
修改 W/S/V 颜色和 alpha；
图像立即更新；
TIFF hash 不变化；
重载 package 后按 UI/Profile 规则恢复。
```

### Case 13C-05 诊断边界

```text
没有 semantic mask 时显示“未提供”；
存在 Texture/Fill/Partition mask 时绑定同一 layerIndex；
closure gap 不从 TIFF 猜测；
诊断失败不改变生产 TIFF 显示。
```

### Case 13C-06 缓存和取消

```text
快速滑动 50 层；
UI 不阻塞；
旧 generation 不覆盖当前层；
缓存大小不超过预算；
关闭窗口后无悬挂 Worker。
```

### Case 13C-07 IO 对比

同一模型分别运行：

```text
旧 preview PNG ON；
生产 TIFF 原生预览 / preview PNG OFF。
```

比较：

```text
TIFF write ms；
preview write ms；
report write ms；
输出文件数量和字节数；
切片核心时间；
UI 首层加载时间。
```

## 4. 自动化入口

计划增加：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case tiff-native-preview-all-materials --package <package>
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case tiff-native-preview-no-png --package <package>
```

完整回归：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
.\build\Debug\rip_reader_test.exe --package <package> --summary
git diff --check
```

本文不宣称尚未实现的命令已运行。

## 5. 通过标准

```text
生产预览完全可由 TIFF 驱动；
RGB+S+W+V 可用；
像素探针与 Reader 一致；
无跨层兜底；
无 preview PNG 时可用；
诊断边界明确；
IO 文件数下降且协议回归通过。
```
