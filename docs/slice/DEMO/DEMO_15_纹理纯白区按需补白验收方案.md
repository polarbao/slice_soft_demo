# DEMO_15 纹理纯白区按需补白验收方案

> 阶段：Stage 15 ｜ 状态：**ACTIVE / DEVELOPMENT READY** ｜ 版本：v1.2 ｜ 日期：2026-08-04
> 上游：`PRD_15` / `DEV_15` / `DOC_DECISION_15`

---

## 1. 验收原则

```text
① 证据优先：每条用例必须产出可复核的产物（SHA-256 / 差异清单 / 报告 JSON / 打样照片）
② 零漂移优先：默认关闭路径的既有 golden 不变，是其它一切结论的前提
③ 差异必须可解释：新旧 Profile 的每一处字节差异都要能指到具体像素与具体原因
④ fail-closed 不得被绕过：旧 Profile 对纯白仍应失败，这是特性不是缺陷
```

## 2. Fixture 清单

| ID | 模型 | 特征 | 用途 |
|---|---|---|---|
| F-01 | `model/obj/小马物语/小马物语小指/MF_Xiao_ma_Xiaozhi_ty03.obj` + 同目录 MTL/PNG | **大面积纯白 + 红色区** | 主用例（复现原始故障） |
| F-02 | `model/obj/meigui_fudiao/04.obj` | 无纯白纹理 | 零漂移对照 |
| F-03 | 合成 fixture：纯白 / 单通道 254 / 254 灰 / 中间色四值条带 | 阈值边界 | 判定精度 |
| F-04 | 合成 fixture：全纯白纹理 | 极端情形 | 全模型补白 |
| F-05 | 既有 golden 全集 | — | 回归 |

> F-03 需新建。四值条带须包含 `255,255,255`、`254,255,255`、`254,254,254`、`128,128,128` 四类像素，用于精确验证 `ink = 255 - min(R,G,B)` 判据。

## 3. 验收用例

### 3.1 G1 · 主路径可切片

| ID | 步骤 | 期望 |
|---|---|---|
| C-01 | F-01 + 新 Profile，场景生产切片 | PASS，退出码 0，无 `SCENE_PRODUCTION_PACKAGE_INVALID` |
| C-02 | 检查 C-01 报告 | `unprintableWhiteCarrierPixels > 0`；`whitePrintPixels > 0` |
| C-03 | 从 C-01 报告定位首个“模型所有权 + 严格纯白纹理”命中像素并抽检生产 TIFF | 字节为 `255,255,255,0,255,255`；证据同时记录 layerIndex、pixelIndex 与 UV/纹理来源 |

C-03 对应原始 `pixel=38085` 故障语义，但不把一次历史运行中的易漂移像素索引硬编码成长期 Gate。

### 3.2 G2 · 差异范围可证

| ID | 步骤 | 期望 |
|---|---|---|
| C-04 | F-04 的纯策略/单层组合 fixture 分别以策略关闭、开启运行 | 差异**仅**出现在通道索引 3（W） |
| C-05 | 对 C-04 每处差异反查其 R/G/B | **全部**为 `255,255,255` |
| C-06 | 统计 C-04 中 R/G/B/S/V 五通道差异数 | **恒为 0** |
| C-07 | 产出差异清单文件 | 可复核，含像素索引 + 层号 + 前后字节 |

> C-06 是硬门。任何 S 或 V 通道的差异都意味着实现越界，直接判不通过。

> 旧严格 RGB Profile 对 F-04 必须 fail-closed，不能生成第二个生产包。G2 使用内存层/确定性预期值比较，禁止为了获取“旧包”而绕过闭合校验。

### 3.3 G3 · 默认关闭零漂移

| ID | 步骤 | 期望 |
|---|---|---|
| C-08 | F-05 全集在 Stage 15 构建上重跑 | 所有 TIFF SHA-256 与基线**逐字节相同** |
| C-09 | `run_ci_quick.ps1` | 与 Stage 15 前同等结果（已知红基线不新增） |
| C-10 | 旧 Profile 显式配置未出现新字段 | 解析后 policy 为 `fail_closed` |

### 3.4 G4 · 无纯白模型等价

| ID | 步骤 | 期望 |
|---|---|---|
| C-11 | F-02 + 新 Profile vs F-02 + 旧 Profile | manifest layer list 指向的每个 TIFF SHA-256 一致；报告、Profile 名和配置快照允许不同 |

C-11 证明补白能力是**严格增量**：不含纯白的模型完全感知不到该 Profile 的差别。

### 3.5 G5 · RIP 兼容

| ID | 步骤 | 期望 |
|---|---|---|
| C-12 | 项目内 `rip_reader_test` strict 读取 C-01 输出 | 通过，无新增警告 |
| C-13 | RIP 侧代码 / 配置 diff | **空** —— 零改动 |
| C-14 | 通道语义确认 | W=0 被识别为常规白墨，非特殊哨兵 |

G5 仅证明本仓库 Reader/Package 合同兼容，不等同于外部目标 RIP 已完成集成验收；外部链路由 G7 证据记录。

### 3.6 G6 · 预检告警

| ID | 步骤 | 期望 |
|---|---|---|
| C-15 | UI 导入 F-01，选择旧「全实体 RGB」Profile | **切片前**出现告警，指名替代 Profile |
| C-16 | UI 导入 F-02，选择旧 Profile | **不**告警（引用纹理/表面样本无纯白） |
| C-17 | 强制跑到失败，查看错误信息 | 协议措辞后含业务解释句 |

C-16 防误报，与 C-15 同等重要。

### 3.7 G7 · 实物打样（非自动化）

| ID | 步骤 | 期望 |
|---|---|---|
| C-18 | F-01 新 Profile 输出实际打印 | 白区固化正常、无溢墨/附着不良 |
| C-19 | 白区与彩色区交界目视检查 | 无明显色阶断层或高度差 |
| C-20 | 与白墨填充 Profile 打样对照 | 差异在工艺可接受范围 |

> **这是本阶段唯一无法用软件测试关闭的门。** 未完成 C-18..20 之前，Profile 的 `enabled` 不得翻转为 true。

### 3.8 G8 · 交付收口

| ID | 步骤 | 期望 |
|---|---|---|
| C-21 | `slicer_scenarios.json` 中新 Profile `enabled` | `true` |
| C-22 | UI Profile 卡片 | 能力集与说明与实际行为一致 |
| C-23 | `QT_DEBUG_UI_操作手册.md` | 已补充新 Profile 与选型指引 |
| C-24 | 旧 Profile 说明文字 | 已指向新 Profile 作为替代方案 |

### 3.9 阈值精度（补充）

| ID | 步骤 | 期望 |
|---|---|---|
| C-25 | F-03 + threshold=0 | 仅 `255,255,255` 像素补白 |
| C-26 | F-03 + threshold=1 | `255,255,255` 与 `254,254,254` 补白；`254,255,255` 因 min=254→ink=1 也补白 |
| C-27 | F-03 + threshold=0，检查 `254,255,255` | **不**补白，且闭合仍通过（ink=1 > 0，但 R=254≠255 使闭合成立） |

C-27 是对"阈值 0 即充分"这一核心论断的直接验证。

### 3.10 配置与路径负向用例

| ID | 步骤 | 期望 |
|---|---|---|
| C-28 | `unprintableWhitePolicy=white_underbase` 且 value=255 | 配置期 fail-closed，错误信息指出 value 不得等于 emptyValue |
| C-29 | 在 materialPolicy/roleMapping 或非目标 Global 组合中强开新策略 | 配置期 fail-closed；Global 既有白像素行为保持回归 |
| C-30 | 重复导入同一纹理并触发预检；扫描过程中切换 scene/revision/纹理 | 相同内容命中 single-flight/缓存，不重复扫描；旧身份结果被丢弃，UI 不阻塞、不串线、不崩溃；告警明确标注为保守预检 |

### 3.11 性能方法

| ID | 步骤 | 期望 |
|---|---|---|
| C-31 | 相同 Release 构建，F-03/F-04 各预热 1 次、计量 7 次 | 记录切片/组合阶段 p50，不混入 TIFF/preview/report IO |
| C-32 | 开启策略与关闭策略 p50 比较 | 退化不超过 2%；若小于计时噪声则记录噪声区间，不使用单次总耗时下结论 |

## 4. 证据产物清单

| 产物 | 用途 |
|---|---|
| `output/benchmarks/stage15/sha256_baseline_before.txt` / `_after.txt` | G3 |
| `output/benchmarks/stage15/pixel_diff_F04.csv` | G2（C-07，纯策略/单层组合结果） |
| `output/benchmarks/stage15/slice_report_F01_new.json` | G1（C-02） |
| `output/benchmarks/stage15/rip_strict_F01.log` | G5 |
| `output/benchmarks/stage15/proof_print/*.jpg` | G7；工艺侧回填，本地证据默认不提交 |
| `output/benchmarks/stage15/baseline_identity.json` | HEAD、构建轨道、fixture/Profile 哈希和基线身份 |
| `output/benchmarks/stage15/performance_p50.json` | C-31/C-32 |
| `output/benchmarks/stage15/stage15_white_carrier_summary.json` | 自动化 G1..G6 汇总和 G7 外部状态引用 |

## 5. 执行入口

```powershell
.\scripts\run_stage15_white_carrier_gate.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/stage15
```

该脚本由 15D-01/02 落地。实现前命令尚不存在，准备阶段不得宣称已执行。

## 6. 判定

**全部 G1–G8 通过方可关闭 Stage 15。** G7 未完成时，阶段状态最高只能标记为
`INTERNAL COMPLETE / PHYSICAL PROOF PENDING`，且 Profile 保持 `enabled: false`。
