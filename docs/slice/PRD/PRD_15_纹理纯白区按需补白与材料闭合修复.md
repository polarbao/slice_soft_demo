# PRD_15 纹理纯白区按需补白与材料闭合修复

> 阶段：Stage 15 ｜ 状态：**ACTIVE / DEVELOPMENT READY** ｜ 版本：v1.2 ｜ 日期：2026-08-04
> 决策依据：`docs/slice/DOC/DOC_DECISION_15_纹理纯白区按需补白与材料闭合修复专项.md`
> 证据等级：A=已核实事实，B=目标需求，P=判断

---

## 1. 背景与问题陈述

在 `black_is_print` 语义下 `255 = 无墨`。「全实体 RGB」Profile 只写 R/G/B，因此纹理中的纯白像素（`R=G=B=255`）产生的六通道字节为 `255,255,255,255,255,255` —— 与背景像素完全相同。场景合成的材料闭合校验判定该像素"属于模型但无任何材料"，生产包被拒（`SCENE_PRODUCTION_PACKAGE_INVALID`，退出码 2）。

**含白色纹理的模型当前无法用全实体 RGB 策略完成生产切片。** 这不是缺陷，是能力缺口 —— 减色语义下"需要打印的白"物理上无法由 RGB 表达。

## 2. 目标

**让含纯白纹理的模型能够在保留全实体 RGB 语义的前提下完成生产切片**，方式是对无法表达的纯白像素补写白墨（W 通道）。

### 2.1 明确的非目标

```text
✗ 不修改 p0.rgbwsv.2 协议
✗ 不修改材料闭合校验规则
✗ 不要求 RIP 侧任何改动
✗ 不实现 V（光油）通道共写
✗ 不修改既有 obj_mtl_texture_rgb_only.json 的行为
✗ 不修复 unexpected_overlap_pixels 恒 0 缺陷（独立立项）
✗ 不解冻 12G-TCWS，不承诺同一个全 RGB Package 同时支持透明/不透明白决策
✗ 不修改 Global Surface Shell 既有精确白写 W 行为
```

## 3. 用户故事

| ID | 角色 | 故事 | 验收 |
|---|---|---|---|
| U-01 | 切片操作员 | 我导入含白色区域的彩色模型，选择「全实体 RGB + 按需补白」Profile，能够正常完成切片 | G1 |
| U-02 | 切片操作员 | 我选择了不支持纯白的 Profile 且模型含白色纹理时，**在切片开始前**就被告知，而不是等 3 秒后拿到一条看不懂的报错 | G6 |
| U-03 | 工艺工程师 | 我需要确认白墨只补在真正的纯白区，不会污染其它颜色区域 | G2 |
| U-04 | 工艺工程师 | 我需要能用旧 Profile 复现旧行为，做 A/B 对照 | G3 / G4 |
| U-05 | RIP 侧 | 我不需要为此做任何改动，收到的 W=0 就是常规白墨信号 | G5 |

## 4. 功能需求

### FR-01 纯白判定（B）

对纹理表面像素，写入 R/G/B 之后按下式判定：

```text
ink = 255 - min(R, G, B)
不可打印白  ⟺  ink <= texture.unprintableWhiteInkThreshold
```

默认阈值 `0`，即严格 `R=G=B=255`。

> **依据（A）**：闭合判据为 `value != empty_value(255)`，故 `254,254,254` 已能通过闭合。阈值 0 对消除闭合失败**充分且必要**；`> 0` 属印刷质量调优，非正确性要求。

### FR-02 补白写入（B）

判定为不可打印白时，向该像素的 W 通道（`base + 3`）写入 `texture.unprintableWhiteValue`（默认 `0` = 满墨）。

三条硬性不变量：

```text
① 只写 W(idx 3)；绝不写 S(idx 4) 与 V(idx 5)
② 不改变 R/G/B 既有取值 —— 纯白像素仍保持 255,255,255
③ 不改变任何 ownership 掩码
```

不变量 ② 的工艺含义：补白是**在保持颜色不变的前提下补上承载它的白墨**，与真实 UV 打印一致 —— 不是把白色改成别的颜色。

### FR-03 策略开关（B）

| 字段 | 类型 | 默认 | 取值 |
|---|---|---|---|
| `texture.unprintableWhitePolicy` | string | `"fail_closed"` | `fail_closed` / `white_underbase` |
| `texture.unprintableWhiteInkThreshold` | uint8 | `0` | 0..255 |
| `texture.unprintableWhiteValue` | uint8 | `0` | 策略启用时 0..254；255 是空值，必须拒绝 |

**默认 `fail_closed` 保证既有配置行为逐字节不变。**

`white_underbase` 表示在**同一 layerIndex、同一 XY 像素**补写 W，不新增 Z 层，也不等同于支撑铺底或全模型白墨底层。首版能力仅支持 Legacy 且 `materialPolicy.enabled=false`、未启用 `materialRoleMapping`；其它组合必须在配置期 fail-closed。

### FR-04 新增 Profile（B · 已交付配置）

| 项 | 值 |
|---|---|
| id | `textured_nail_rgb_white_ondemand_lower_support` |
| 显示名 | 彩色纹理甲片 - 全实体 RGB + 按需补白 + 下表面支撑 |
| 配置 | `samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json` |
| 能力集 | `rgb_full_volume_texture`、`unprintable_white_underbase`、`lower_support`、`internal_void_support`、`no_varnish` |
| 输出目录 | `output/ProfileTexturedNailRgbWhiteOnDemandLowerSupport` |

既有 `textured_nail_rgb_only_lower_support` **保持不变**，作为"严格 RGB / 遇纯白 fail-closed"的兼容基线保留。

### FR-05 报告与统计（B）

- 新增计数 `unprintableWhiteCarrierPixels`（逐层 + 汇总）
- `white_print_pixels` 必须覆盖新写入路径
- `materialProcessProfile` 报告中如实反映白墨覆盖

报告字段固定写入：

```text
reports/slice_report.json
  layers[].unprintableWhiteCarrierPixels
  totals.unprintableWhiteCarrierPixels

reports/material_process_report.json
  layers[].unprintableWhiteCarrierPixels
  white.unprintableWhiteCarrierPixels
```

### FR-06 切片前预检（B）

```text
条件：导入资产/表面预览发现严格纯白像素 AND 选定 Profile 能力集不含 unprintable_white_underbase
动作：切片开始前给出明确的保守告警，指名可用的替代 Profile
```

该扫描由独立资产预检服务负责并缓存；`ConfigValidator` 只负责字段结构与取值。整张源贴图扫描可能覆盖未被 UV 使用的 texel，因此预检告警不作为硬阻断，生产闭合校验仍是最终真源。

### FR-07 错误信息业务化（B）

`SCENE_PRODUCTION_PACKAGE_INVALID` 失败像素满足「六通道全 255 且 `ownership.model=1`」时，在协议层措辞后追加业务解释：

> 该像素位于纯白纹理区域，当前 Profile 无法表达需打印的纯白，请改用「全实体 RGB + 按需补白」或白墨填充 Profile。

## 5. 非功能需求

| ID | 需求 |
|---|---|
| NFR-01 | 默认关闭时**零行为漂移**：既有 golden TIFF SHA-256 逐字节不变 |
| NFR-02 | 补白判定为逐像素常数时间；相同 Release 构建、隔离切片/组合计时、1 次预热 + 7 次计量的 p50 退化 ≤ 2% |
| NFR-03 | 不引入新的跨模块依赖 |
| NFR-04 | 新配置对 pre-15 构建 fail-closed，不得静默降级为危险行为 |
| NFR-05 | 纹理纯白预检不得阻塞 UI 线程；结果绑定 scene/revision/contentHash，过期结果不得显示到当前场景 |

## 6. 验收标准（对应出口门）

| 门 | 标准 |
|---|---|
| G1 | 小马物语小指 + 新 Profile 切片 PASS，无 `SCENE_PRODUCTION_PACKAGE_INVALID` |
| G2 | 纯策略/单层组合 fixture 证明启用策略相对关闭策略的差异**仅**落在 W 通道且**仅**在命中判据的纹理像素上；旧 Profile 仍作为独立 fail-closed 负向用例 |
| G3 | `policy=fail_closed` 时既有 golden 逐字节不变 |
| G4 | 无纯白纹理模型的新旧 Profile，manifest layer list 指向的 TIFF 文件 SHA-256 一致；报告/配置元数据允许不同 |
| G5 | 项目内 `rip_reader_test` strict 读取通过且本仓库 RIP 侧零改动；不以此替代外部目标 RIP 证据 |
| G6 | 异步预检对「纯白纹理 + 不支持补白 Profile」给出切片前告警；重复扫描命中缓存，场景切换后旧结果被丢弃 |
| G7 | **实物打样确认白墨区固化与外观可接受**（唯一非自动化门） |
| G8 | 注册表 `enabled` 翻转为 true，手册与 Profile 说明同步 |

## 7. 已知风险

| 编号 | 风险 | 等级 | 说明 |
|---|---|---|---|
| R1 | 纯白区材料由彩色墨变为白墨，固化/叠加特性不同 | **高** | 软件不可消除，须打样确认（G7） |
| R4 | 新 Profile 需自建 golden 基线 | 中 | 需逐像素人工确认差异范围 |

完整风险登记见决策文档 §8.1。
