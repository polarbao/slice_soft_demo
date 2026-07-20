# DOC_PREP_12E-R5 Qt UI 与 Effective Config 准备

> 文档状态：PREPARED / 12E-09A READY AFTER 12E-08C / 12E-09B BLOCKED BY 12E-08D
> 日期：2026-07-20
> 覆盖任务：12E-09 Qt UI 设置与 Effective Config
> 前置状态：12E-08A/08B COMPLETE；12E-08C/08D 未完成

## 1. 准备结论

12E-09 的 UI 范围、配置所有权、异步边界、状态显示、preview 合同和 smoke matrix 已准备完成。
但当前 12E 仍为 diagnostic-only，普通用户不能把 `global_surface_shell` 当作已准入生产策略。

执行可分为两层：

```text
12E-09A diagnostic UI：准备已完成，建议在 12E-08C 后实现并显式显示“诊断候选”；
12E-09B production Profile：必须等待 12E-08D admitted=true 后启用。
```

## 2. 用户目标

普通用户只需要理解：

```text
纹理表面层宽度；
模型填充材料；
模型分析给出的最小值、最大值和全纹理阈值；
当前能力是未分析、诊断候选、阻断还是已准入；
最终生效配置保存在哪里。
```

普通 UI 不暴露 Legacy CPU/OpenVDB backend 选择。OpenVDB 仍只出现在高级诊断区。

## 3. UI 信息架构

建议在现有“配置 -> 材料策略”中增加“全局纹理与模型填充”区域：

```text
策略：传统表面纹理 / 全局三维纹理（诊断候选）
纹理表面层宽度：QDoubleSpinBox + slider
模型填充材料：白墨 / 光油 / RGB / 已注册其他材料
分析状态：未分析 / 分析中 / 可诊断 / 阻断 / 已准入
动态范围：最小值、最大值、全纹理阈值
覆盖统计：TextureSurface、ModelFill、overlap、unassigned
后端状态：普通页只显示“可用/不可用”，不显示选择器
```

所有可见中文必须有短说明 tooltip；不得把整段 PRD 放进界面。

## 4. 宽度控件合同

```text
单位：mm；
步长：0.01 mm；
显示精度：2 位小数；
初始工程下限：0.10 mm；
模型分析后 minimum = effectiveMinimumWidthMm；
maximum = allTextureThresholdMm；
达到 maximum 时显示“全纹理，模型填充为 0”；
分析失败时控件禁用，不保留伪造范围。
```

slider 与 QDoubleSpinBox 必须双向同步，使用 Qt 函数指针 connect 语法；自定义槽以 `On` 开头，
自定义信号以 `Sig` 开头。

## 5. 异步和线程边界

三维 topology、distance、width sweep、texture transfer 和 raster mapping 不得运行在 Qt UI 线程。

```text
UI thread：收集输入、启动任务、显示状态、取消请求、消费不可变结果；
worker：执行 preflight 和 diagnostic pipeline；
完成信号：只传递拥有明确生命周期的值对象或共享不可变结果；
取消：不得在 writer 已开始后伪装成功；
窗口关闭：等待或安全取消 worker，不悬挂 QObject 指针。
```

## 6. Effective Config 所有权

UI 不直接覆盖 samples/configs 中的 fixture。每次会话生成独立 effective config：

```text
output/ui_sessions/<session>/slice_config.effective.json
```

应记录：

```text
来源 Profile ID；
用户可编辑项；
模型分析派生值；
global_surface_shell diagnostic/admission 状态；
modelFill.material；
support/varnish 既有策略；
OpenVDB compiled/runtime 状态，但不作为普通用户选择；
配置 schema/version；
生成时间和输入模型标识。
```

保存、另存为、回退必须继续使用现有 Config Editor 事务边界，不能静默修改长期 Profile。

## 7. Preview 合同

12E-09 只消费真实 raster mapping layer，不再按 preview 文件序号猜层号：

```text
Texture Surface：纹理表面区域与真实 RGB；
Model Fill：按材料伪彩显示 W/V/RGB 填充；
Partition：Texture 与 Fill 互补叠加；
状态栏：layerIndex、zMm、width、coverage、allTexture；
同层原则：所有 overlay 必须使用同一个 layerIndex。
```

12E-08B 已提供 Support/Varnish 真实诊断状态。UI 必须读取 `fullClosureLinkage`，证据缺失时
仍显示 `未评估`，不得用空 mask 或默认值显示绿色 PASS。

## 8. 能力状态机

```text
pending：尚未执行模型分析；
unavailable：构建或依赖不具备能力；
blocked：topology、纹理、grid 或不变量失败；
diagnostic：可以查看候选结果，但不能写生产包；
admitted：12E-08D 全部 Gate 通过且用户批准生产路径。
```

UI 按钮文案：

```text
diagnostic：分析全局纹理分区；
admitted：使用全局纹理策略切片；
```

不得在 diagnostic 状态显示“一键生产切片成功”。

## 9. 文件边界

预计允许修改：

```text
apps/slicer_debug_ui 中配置页、状态 DTO、worker adapter 和 preview panel；
src/slicer_core/config 中 12E effective config 序列化；
src/slicer_core/pipeline 中只读 diagnostic facade；
tests/ui 或现有 Qt self-test；
docs/user_guides。
```

12E-08D admitted 前禁止：

```text
把 12E 设为默认生产 Profile；
修改 legacy Profile 输出；
把 OpenVDB 设为默认；
由 UI 绕过 production admission policy；
改写 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print。
```

## 10. 验收矩阵

功能：

```text
0.01 mm spinbox/slider 同步；
模型切换后动态范围刷新；
minimum/intermediate/allTexture 三种状态正确；
modelFill.material 保存并回读；
effective config 不覆盖 fixture；
诊断状态不会启用生产写包；
真实 layerIndex/zMm 显示正确。
```

UI：

```text
Qt self-test PASS；
1280x720、1440x900、1920x1080 无遮挡；
最长中文文本不截断；
后台分析时 UI 可响应；
取消、失败、重复执行无重复 panel 和 stale preview；
tooltip 与用户手册字段一致。
```

回归：

```text
传统 Profile 与 OpenVDB 诊断入口行为不变；
12E diagnostic 不写 TIFF/package；
默认 OpenVDB OFF build PASS；
OpenVDB ON 可选 lane PASS。
```

## 11. 12E-09 启动 Gate

| 证据 | 当前状态 | 允许动作 |
|---|---|---|
| 12E config/partition/width | PASS | 可绑定诊断 UI |
| texture transfer | PASS | 可显示候选 RGB |
| classification-to-raster | 12E-08A PASS | 可显示真实 raster layer |
| full material closure | 12E-08B PASS / DIAGNOSTIC | 可显示真实 Support/Varnish 状态 |
| Release/legacy regression | 12E-08C TODO | 不得宣传生产性能 |
| production admission | 12E-08D NOT GRANTED | 不得启用生产 Profile |

## 12. 最终判断

```text
12E-09 文档准备：COMPLETE；
12E-09A diagnostic UI：PREPARED，建议在 12E-08C 后执行；
12E-09B production Profile：BLOCKED BY 12E-08D；
12E production：NOT ADMITTED。
```
