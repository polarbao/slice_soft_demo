# REPORT_15 纹理纯白区按需补白当前状态

> 阶段：Stage 15 ｜ 状态：**ACTIVE / DEVELOPMENT** ｜ 版本：v1.3 ｜ 更新：2026-08-04
> 上游：`DOC_DECISION_15` / `PRD_15` / `DEV_15` / `DEMO_15`

---

## 1. 阶段状态

```text
状态：ACTIVE / HIGHEST PRIORITY
授权：用户于 2026-08-04 授权成立
基线：15A-01 已在首次代码编辑前生成 `output/benchmarks/stage15/baseline_identity.json`
优先级：高于 Stage 14（14 为集成工程；15 阻断实际生产使用）
```

## 2. 已完成（准备与配置侧）

| 项 | 产物 | 状态 |
|---|---|---|
| 根因定位 | 决策文档 §3 | ✅ 已核实到行号 |
| 协议可行性论证 | 决策文档 §4 | ✅ 证明协议层无需改动 |
| W/V 成本差异分析 | 决策文档 §4.1 | ✅ 排除 V |
| 阈值语义澄清 | 决策文档 §5 | ✅ 默认 0 充分 |
| 新 Profile 配置 | `samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json` | ✅ 已创建 |
| Profile 注册 | `samples/scenarios/slicer_scenarios.json` | ✅ 已注册（`enabled: false`）|
| 决策文档 | `DOC_DECISION_15` | ✅ |
| 需求文档 | `PRD_15` | ✅ |
| 设计文档 | `DEV_15` | ✅ |
| 验收方案 | `DEMO_15` | ✅ |
| 任务清单 | `TASKS_15` | ✅ |
| 执行指令 | `CODEX_PROMPT_15` | ✅ |
| 实施依赖审查 | `DOC_PREP_15_纹理纯白区按需补白实施准备与依赖审查.md` | ✅ 已关闭初稿准备缺口 |

准备审查已修订：G2 不再依赖旧 Profile 生成生产包；G4 只比较 TIFF；纯白谓词改为独立可单测模块；UI 扫描从 `ConfigValidator` 移至异步预检服务并绑定 scene/revision/contentHash；不新增闭合掩码；Global 只做回归；15D-01/02 负责建立统一自动 Gate。

## 3. 主线开发进度

| 任务组 | 内容 | 状态 |
|---|---|---|
| 15A | 配置契约（3 字段 + 校验 + 枚举扩展） | ✅ 15A-01..04 完成 |
| 15B | 合成器补白写入 + 统计 | 🟡 15B-01..03 完成；15B-04 待性能 fixture |
| 15C | 切片前预检与错误信息业务化 | ⬜ 未开始 |
| 15D | 验收与证据产出 | 🟡 15D-01、15D-02、15D-04 完成；15D-03 部分完成，15D-05 待工艺侧 |
| 15E | 交付收口 | ⬜ 未开始 |

## 4. 出口门进度

| 门 | 内容 | 状态 |
|---|---|---|
| G1 | 纯白模型切片 PASS | ✅ F-01 自动 Gate PASS；实物仍由 G7 单独约束 |
| G2 | 纯策略/单层组合差异仅落在 W 且仅在命中像素；旧 Profile 负向不变 | ✅ R/G/B/S/V 差异数均为 0，W 差异 4；fail_closed 报 `E_MATERIAL_PROCESS_PROFILE_EMPTY_WHITE` |
| G3 | 默认关闭零漂移 | 🟡 基线 golden SHA-256 PASS；quick CI 在既有 `support bridge report expected bridgedGaps` 处失败，尚未关闭 |
| G4 | 无纯白模型生产 TIFF SHA-256 等价 | ✅ F-02 manifest layer TIFF 逐层 SHA-256 等价，候选补白计数为 0 |
| G5 | 项目内 RIP strict 通过、本仓库 RIP 零改动 | ✅ F-01 strict Reader PASS；本阶段未修改 RIP 源码 |
| G6 | 切片前预检告警 | ⬜ |
| G7 | **实物打样确认** | ⬜ ← 唯一非自动化门 |
| G8 | 注册表翻转与文档同步 | ⬜ |

### 4.1 当前自动验证证据

```text
Release 构建轨道：build-slicesoft/main
texture_white_carrier_policy_unit_tests：PASS
experimental_config_unit_tests：PASS
F-01 小马物语真实模型切片：PASS
F-01 配置：samples/configs/material_process/stage15_f01_xiaoma_white_carrier.json
F-01 package：output/Stage15F01WhiteCarrier
F-01 grid：300 x 623 x 145
unprintableWhiteCarrierPixels：150581
slice_report 逐层和/总计：150581 / 150581
material_process_report 逐层和/总计：150581 / 150581
W printPixels：150581
validationFailures：[]

15D-01/02/04 统一 Gate：PASS（G1/G2/G4/G5）
Gate 入口：scripts/run_stage15_white_carrier_gate.ps1
Gate 摘要：output/benchmarks/stage15/stage15_white_carrier_summary.json
F-03 threshold=0 / 1 补白像素：1134 / 5605
F-04 补白像素：6671
F-04 差异通道计数：R=0, G=0, B=0, W=4, S=0, V=0
F-02 候选补白像素：0；新旧生产 TIFF 逐层 SHA-256 等价
F-01 RIP strict Reader：PASS
基线 golden 文件 SHA-256：PASS
run_ci_quick：FAIL（既有 support bridge schema 断言缺少 bridgedGaps；非 Stage 15 差异）
```

上述证据证明 Legacy 目标路径能够仅在命中纯白纹理像素时写 W，并保持两份报告的逐层/汇总一致；G1、G2、G4、G5 已由统一 Gate 自动关闭。G3 仍需先处理 quick CI 的既有 support bridge 失败，G7 仍只能由实物打样关闭。

## 5. 关键事实索引

| 事实 | 位置 | 等级 |
|---|---|---|
| 闭合判据允许 R/G/B/W 任一非空 | `SceneLayerComposer.cpp:250-291` | A |
| 写 V 必须配 `modelvarnishownership` | 同上 :278-283 | A |
| 故障写入点（只写 RGB） | `slicer.cpp:3059-3072` | A |
| `ShouldApplyModelFill` 对 rgb+表面像素返回 false | `slicer.cpp:2583-2601` | A |
| `NormalizeModelFillTextureContract` 对 rgb 提前返回，无告警 | `EffectiveConfigGenerator.cpp:529-548` | A |
| 同类生产禁令先例 | `config.cpp:803-810` | A |
| underbase 覆盖率检查按精确字符串匹配 | `slicer.cpp:3732` | A |
| `unexpected_overlap_pixels` 硬编码 `constexpr 0U` | `slicer.cpp:3711` | A |
| 实测故障：`pixel=38085 values=255,255,255,255,255,255 ownership=1,0,0` | 用户 2026-08-04 运行 | A |

## 6. 风险状态

| 编号 | 风险 | 等级 | 状态 |
|---|---|---|---|
| R1 | 白墨工艺特性差异 | **高** | 🔴 未缓解 —— 只能由 G7 打样关闭 |
| R2 | 阈值语义争议 | 低 | 🟢 已降级（§5 论证） |
| R3 | `unexpected_overlap_pixels` 恒 0 | 中 | 🟡 已识别，**独立立项，不并入本阶段** |
| R4 | golden 基线漂移 | 中 | 🟡 由 G2/G3/G4 三门共同控制 |
| R5 | 命名不实 | 低 | 🟢 已消解（新增 Profile 而非改造） |

## 7. 待办：独立缺陷卡

**`unexpected_overlap_pixels` 恒 0（R3）尚未建卡。**

`slicer.cpp:3711` 的 `constexpr std::uint64_t unexpected_overlap_pixels{0U}` 使
`maxUnexpectedOverlapPixels` 校验永久空转。修复需先定义「合法材料共写」与「意外重叠」的
区分规则，否则 `13b_07_xiao_ma_legacy.json` 等既有「白墨打底 + 纹理」配置（本就 RGB+W 共存）
会立即报 `E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP`，打破绿基线。

**建议在 Stage 15 关闭后单独立项，不得并入。**

## 8. 与 Stage 14 的衔接

Stage 15 为 Stage 14「RIP 六问 Q2（白区语义）」追加一条显式不透明白 Profile 的实证路径：

> 对明确选择按需补白 Profile 的作业，用真实材料语义（W=0）表达需打印的白，可行且不需要本仓库 RIP 改动。

该结论只做追加，不替换 Q2，不解冻 12G-TCWS 的“同一全 RGB Package 透明/不透明复用”问题。外部目标 RIP 仍需在 G7/集成证据中确认。

## 9. 当前下一任务

1. `15D-03`：隔离并处理 quick CI 的既有 support bridge schema 失败，关闭 G3。
2. `15B-04`：基于已就绪的 F-03/F-04 执行同构 Release 性能基线。
3. `15C-01` / `15C-03`：实现异步纯白预检与既有错误信息的业务解释。

G7 通过前，候选 Profile 必须继续保持 `enabled: false` / `productionSafety: diagnostic`。
