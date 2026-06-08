# DOC_DECISION_05_REPORT04A后进入材料策略基础阶段

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_04A 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：04A 纹理阶段收口完成后，进入 05 材料策略与白墨/光油控制基础阶段

---

## 1. 阶段判断

根据 `REPORT_04A_纹理阶段收口修复当前实现状态.md`，04A 已完成：

```text
1. Missing texture fallback 小型 fixture 已恢复
2. No-UV fallback 小型 fixture 已恢复
3. fallback 用例不再依赖 38MB 真实大模型
4. texture fallback 语义已进入 run_regression.ps1
5. support connectivity diagnostics 已输出
6. 完整回归通过
```

这说明 04 彩色纹理基础阶段已完成收口，可以进入下一阶段。

---

## 2. 为什么进入 05

当前已经具备 05 的前置条件：

```text
1. RGBWSV 协议已经由 03 固化
2. RGB 纹理主链路已经由 04 打通
3. missing texture / no-UV fallback 已由 04A 收口
4. 支撑割裂已有 report 级诊断
5. 回归脚本可以防止 05 破坏 P0 / Relief / Support / Texture
```

因此下一阶段应进入：

```text
05：材料策略与白墨 / 光油控制基础阶段
```

---

## 3. 05 阶段定位

05 是材料策略基础版，目标是把当前单通道/常量通道输出升级为可配置的材料组合策略：

```text
RGB texture
+ W white underbase
+ V varnish top / top_n_layers
+ S support
```

05 不是完整工业色彩系统。

---

## 4. 05 不做什么

05 不做：

```text
ICC / 色彩管理
CMYK / RIP 半色调
喷头 bitstream
3MF 多材料
OpenVDB / SDF
Qt UI
复杂支撑形态修复
完整 color_shell_volume
真实墨量曲线
```

---

## 5. 与第 68 层支撑割裂的关系

04A 已把第 68 层支撑割裂做成 report 诊断。

05 只处理材料组合策略，不处理支撑形态修复。

如果后续需要解决支撑小岛合并、支撑割裂、支撑连通形态，应另开：

```text
08：高级支撑与工艺优化
```

或轻量：

```text
04B：支撑形态修复专项
```

---

## 6. 05 必须保持的冻结项

```text
schema = p0.rgbwsv.1
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
```

---

## 7. 结论

04A 完成后，建议继续生成并执行 05 阶段文档：

```text
PRD_05_材料策略与白墨光油控制基础版.md
DEV_05_MaterialPolicy白墨光油策略设计.md
DEMO_05_材料策略组合验证方案.md
TASKS_05_材料策略任务清单.md
```
