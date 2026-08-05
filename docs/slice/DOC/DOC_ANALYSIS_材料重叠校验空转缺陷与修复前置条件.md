# DOC_ANALYSIS 材料重叠校验空转缺陷与修复前置条件

> 文档状态：**OPEN DEFECT / NOT SCHEDULED**（独立缺陷卡，不并入任何在建阶段）
> 版本：v1.0 ｜ 建卡日期：2026-08-04 ｜ 来源：Stage 15 分析副产物（R3）
> 证据等级：A=已核实代码事实
> 建卡依据：`REPORT_15_纹理纯白区按需补白当前状态.md` §7

---

## 1. 缺陷事实（A）

`src/slicer_core/slicer.cpp:3711`：

```cpp
constexpr std::uint64_t unexpected_overlap_pixels{0U};
```

该值是**编译期常量 0，从不实际计算**。随后 :3728-3731 用它做校验：

```cpp
if (unexpected_overlap_pixels
    > static_cast<std::uint64_t>(profile.validation.max_unexpected_overlap_pixels)) {
    validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP");
}
```

由于 `max_unexpected_overlap_pixels` 的最小合法值为 0（`config.cpp:1058-1059` 拒绝负数），
判据恒为 `0 > 0` → **永远为假**。

**结论：`materialProcessProfile.validation.maxUnexpectedOverlapPixels` 这项配置在全仓库范围内永久空转。**
所有声明 `"maxUnexpectedOverlapPixels": 0` 的 Profile 都在依赖一项不存在的保护。

## 2. 影响范围

| 项 | 影响 |
|---|---|
| 受影响配置 | 所有启用 `materialProcessProfile` 且声明该字段的 Profile |
| 实际后果 | 意外的多材料同像素写入**不会被发现**，报告与校验均显示通过 |
| 严重度 | **中** —— 不产生错误输出，但让一道设计中的护栏形同虚设 |
| 是否阻断生产 | 否。材料闭合校验（`SourcePixelHasClosure`）仍在工作，是真正的兜底 |

## 3. 为什么不能直接"把它改成真算"（关键）

**朴素修复会立即打破绿基线。** 现有多份配置本就**合法地**在同一像素写入多种材料：

| 配置 | 共写情况 |
|---|---|
| `samples/configs/scene/13b_07_xiao_ma_legacy.json` | `modelFill.material=white, scope=all_model` + 纹理 RGB → **整个模型 RGB 与 W 共存** |
| `samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json` | RGB 表层 + 白墨模型填充 → 同上 |
| `samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json`（Stage 15） | 纯白纹理像素 RGB=255,255,255 与 W=0 共存 |

这些都是**设计内的正确行为** —— 白墨打底本来就是"在颜色之下再上一层白"，
物理上必然同像素共存。若把重叠计数改为真算，这些配置会立刻报
`E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP`。

**所以缺陷的本质不是"少写了一段计数代码"，而是"从未定义过什么叫意外重叠"。**

## 4. 修复前置条件

修复必须先产出一份**「合法共写 vs 意外重叠」判定规则**，至少覆盖：

```text
① 哪些通道组合属于设计内共写？
   已知合法：RGB + W（白墨打底 / 按需补白）
   已知合法：RGB + V（光油覆盖）
   待定：    W + V（白墨 + 光油同像素）
   待定：    任意模型材料 + S（支撑）—— 闭合校验已按 ownership 严格约束，可能天然不可达

② 判定依据是配置意图还是像素事实？
   建议：由 Profile 声明"预期共写集合"，实际共写超出声明集合才算意外
   —— 这样既能真正发现问题，又不误伤设计内组合

③ 与 SourcePixelHasClosure 的分工
   闭合校验管"至少一种材料"（下界）
   重叠校验管"不超出预期材料集"（上界）
   两者应互补而非重复
```

Stage 15 的收口实际上**为这项工作提供了词汇** —— `unprintable_white_underbase`
这类具名材料语义，正是"声明式预期共写集合"的可用载体。

## 5. 建议处置

| 项 | 建议 |
|---|---|
| 时机 | **不并入 Stage 14**（集成工程，范围无关）。建议作为 12A 材料语义的独立增量立项 |
| 前置 | 先出规则文档，再动代码。规则未定不得改计数逻辑 |
| 回归要求 | 修复后必须验证上表三份配置**仍然通过**，这是规则正确性的直接检验 |
| 临时缓解 | 无需缓解。闭合校验是真正的兜底，当前不存在输出正确性风险 |

## 6. 不建议的处置

```text
✗ 顺手在其它阶段里"把常量改成真算"—— 必然打破绿基线
✗ 把 maxUnexpectedOverlapPixels 默认值调大以"避免误报"—— 让空转变成半空转，更糟
✗ 删除该字段 —— 会丢失一个本应存在的护栏位置
```

## 7. 关联

| 文档 | 关系 |
|---|---|
| `REPORT_15_纹理纯白区按需补白当前状态.md` §7 | 本卡来源（R3） |
| `DOC_DECISION_15_...md` §10 | 明确将本项划出 Stage 15 范围 |
| `PRD_12A_彩色纹理材料填充支撑光油策略.md` | 材料语义真源，规则文档的归属地 |
