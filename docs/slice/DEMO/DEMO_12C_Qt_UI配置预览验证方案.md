# DEMO_12C_Qt_UI配置预览验证方案

> 文档版本：v0.1
> 文档状态：DEMO / Stage 12C
> 生成日期：2026-07-05

---

## 1. 验证目标

验证 UI 是否能从“多个配置文件入口”收束为“Profile + 设置 + 统一预览工作区”。

---

## 2. 验证场景

### Case 12C-01 Profile 中文显示

步骤：

```text
1. 打开 slicer_debug_ui；
2. 展开场景/Profile；
3. 查看中文短名是否完整显示；
4. 鼠标悬停显示完整说明。
```

通过：

```text
下拉框无截断关键字；
长路径不直接挤占界面；
高级 fixture 默认不干扰普通 Profile。
```

### Case 12C-02 一键切片配置生成

步骤：

```text
1. 选择彩色纹理甲片 Profile；
2. 选择模型；
3. 设置模型填充材料；
4. 设置支撑 placement；
5. 运行切片。
```

通过：

```text
generated config 中包含 UI override；
输出 package 正常；
UI 显示配置摘要。
```

### Case 12C-03 统一预览工作区

步骤：

```text
1. 切换生产层检查；
2. 切换材料叠加；
3. 切换原始调试预览；
4. 使用像素探针检查同一 layer。
```

通过：

```text
三种模式共享 layerIndex；
图例显示 RGB/W/S/V；
原始调试预览明确标注不是生产真源。
```

### Case 12C-04 报告曲线诊断抽屉

步骤：

```text
1. 打开报告；
2. 打开曲线；
3. 折叠诊断区域；
4. 调整窗口大小。
```

通过：

```text
报告/曲线不遮挡主预览；
小窗口无控件重叠。
```

### Case 12C-05 OpenVDB 候选提示

步骤：

```text
1. 选择 OpenVDB candidate；
2. 对真实 OBJ 模型运行；
3. 观察成功或失败提示。
```

通过：

```text
显示候选/诊断标签；
失败时显示 failureReason；
提示可回退 legacy；
不把 non-production 输出标为 production。
```

---

## 3. 验证记录

每轮 UI 验证记录：

```text
build type；
Profile；
模型路径；
生成 config；
输出 package；
关键截图；
日志退出码；
手动检查结论。
```
