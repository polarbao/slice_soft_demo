# DOC_REVIEW_02_03_业务逻辑审查与修订结论

> 文档版本：v0.1  
> 文档状态：Review / 修订结论  
> 适用阶段：REPORT_01 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：审查 02 支撑阶段与 03 协议阶段文档是否足够严谨，并给出修订版执行口径

---

## 1. 审查结论

原 02 / 03 阶段文档方向正确，但业务逻辑还不够严谨，主要问题包括：

```text
1. 支撑业务语义不够清晰：
   支撑材料 S、浮雕基底、模型实体、未来 SupportType 的边界需要进一步拆清楚。

2. unsupported_only 定义不够精确：
   原文只说“逐层判断是否承托”，但没有明确 component、overlap、previous model/support、过滤阈值、投影策略。

3. SupportType 容易被误解为新增 TIFF 通道：
   必须明确 SupportType 只进入 report / preview metadata，不改变 RGBWSV 六通道协议。

4. PRD_02 与 DEV_02 缺少“组合支撑模式”：
   当前真实业务既需要 bottom_projection，又可能需要 island/unsupported 补充，因此应支持 bottom_projection_plus_unsupported。

5. PRD_03 协议固化还不够硬：
   需要明确 manifest schema、错误码、reader 校验顺序、负向测试、tile padding、preview/production 分离。

6. 缺少执行优先级：
   PRD_02 的最小闭环应先做 bottom_projection 回归 + unsupported_only + report stats，而不是一次性追求复杂支撑树。
```

---

## 2. 修订后阶段定位

### 阶段 02

阶段 02 定位为：

```text
支撑生成、孤岛检测与 SupportType 元数据扩展
```

不是：

```text
复杂支撑树
支撑可拆结构
支撑力学优化
```

核心目标：

```text
在现有 bottom_projection 基础上，增加 island/unsupported 诊断与最小支撑补充能力。
```

---

### 阶段 03

阶段 03 定位为：

```text
RGBWSV TIFF / Manifest / RIP Reader 输入协议固化
```

不是：

```text
RIP 半色调
CMYK 分色
喷头数据流
色彩管理
```

核心目标：

```text
把当前已经稳定的 RGBWSV / uint8 / black_is_print 协议变成可测试、可升级、可拒绝错误包的正式契约。
```

---

## 3. 修订后执行顺序

推荐执行顺序：

```text
1. 先执行 PRD_02 v0.2：
   - support.mode 配置扩展
   - bottom_projection 回归
   - unsupported_only
   - bottom_projection_plus_unsupported
   - island detection
   - support report 增强

2. 并行阅读 PRD_03 v0.3，但不要改变协议。

3. PRD_02 完成后生成 REPORT_02。

4. 再执行 PRD_03 v0.3：
   - manifest schema
   - error code
   - rip_reader negative tests
   - regression script
   - REPORT_03
```

---

## 4. 必须保持不变的冻结项

```text
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
PlanarConfig = contiguous
tiled = true
Model > Support > Empty
SupportType 不增加 TIFF 通道
```

---

## 5. 替换建议

建议用本次文档包中的新文件替换或覆盖旧版：

```text
PRD_02_支撑生成孤岛检测与SupportType扩展_v0.2.md
DEV_02_支撑孤岛检测与SupportType设计_v0.2.md
DEMO_02_支撑与孤岛检测验证方案_v0.2.md
TASKS_02_支撑孤岛检测任务清单_v0.2.md

PRD_03_v0.3_RGBWSV协议固化与负向测试.md
DEV_03_v0.3_TIFFWriter_RIPReader协议固化设计.md
TASKS_03_v0.3_RGBWSV协议固化任务清单.md
```

旧版可保留到 `docs/slicer/archive/`，但不要让 Codex 优先阅读旧版。

---

## 6. 结论

02 / 03 阶段值得继续推进，但应按照本次 v0.2/v0.3 文档重新收敛业务边界：

```text
02：支撑体系化，但不做复杂支撑树；
03：协议固化，但不做 RIP 算法；
彩色纹理继续后移。
```
