# TASKS_15 纹理纯白区按需补白任务清单

> 阶段：Stage 15 ｜ 状态：**ACTIVE / EXTERNAL GATE** ｜ 版本：v1.8 ｜ 日期：2026-08-04
> 上游：`DOC_DECISION_15` / `PRD_15` / `DEV_15` / `DEMO_15`
> 优先级：**高于 Stage 14**（14 为集成工程；15 阻断实际生产使用）

---

## 0. 开工须知

```text
① 本阶段不修改协议、不修改闭合校验、不要求 RIP 改动 —— 若发现需要改，先停下来复核设计
② 默认值 fail_closed 必须保证既有行为逐字节不变，这是所有其它结论的前提
③ 只写 W(idx 3)；触碰 S(idx 4) 或 V(idx 5) 即为越界
④ 配置产物已由 Claude 侧交付（见 15E-02 前置），无需重建
⑤ slicer.cpp:3711 unexpected_overlap_pixels 缺陷【不属于本阶段】，不要顺手改
⑥ 开工前记录 HEAD、构建轨道、Profile/fixture/golden 哈希；当前脏工作树不能作为模糊基线
⑦ Legacy 是目标实现路径；Global 已有精确白写 W 逻辑，本阶段只回归
```

## 1. 任务总览

| 组 | 主题 | 卡数 | 可并行 |
|---|---|---|---|
| 15A | 配置契约 | 4 | 内部串行 |
| 15B | 合成器补白与统计 | 4 | 依赖 15A |
| 15C | 预检与错误信息 | 3 | 可与 15B 并行 |
| 15D | 验收与证据 | 5 | 依赖 15B |
| 15E | 交付收口 | 3 | 依赖 15D |

### 1.1 当前执行状态

| 任务 | 状态 | 证据摘要 |
|---|---|---|
| 15A-01 | ✅ COMPLETE | 基线身份已冻结；`TextureConfig` 三字段已加入 |
| 15A-02 | ✅ COMPLETE | 解析、默认值、fail-closed 组合校验和 U15-05/06/08/09 通过 |
| 15A-03 | ✅ COMPLETE | 新 white mode/coverage 枚举可加载 |
| 15A-04 | ✅ COMPLETE | underbase 精确语义由公共谓词和 U15-07 锁定 |
| 15B-01 | ✅ COMPLETE | STL-only 纯白谓词、Legacy W 写入和 U15-01..04 通过 |
| 15B-02 | ✅ COMPLETE | 核心语义计数与 W channel stats 一致 |
| 15B-03 | ✅ COMPLETE | 两份报告逐层和/总计一致，F-01 实测均为 150581 |
| 15B-04 | ✅ COMPLETE | Release 预热 1 次、计量 7 次；F-03/F-04 `sliceProcessingMs` p50 退化均 ≤ 2% |
| 15C-01 | ✅ COMPLETE | 异步严格纯白扫描、完整资产身份缓存、single-flight、stale 丢弃和 C-15/C-16 服务单测通过 |
| 15C-02 | ✅ COMPLETE | 异步结果已绑定 scene/revision/contentHash/Profile，Legacy RGB 路径在切片前显示保守告警；能力 Profile 与 stale 结果不告警 |
| 15C-03 | ✅ COMPLETE | 保留既有协议错误前缀与错误码，仅在 model ownership + 六通道 Empty 条件下追加纯白纹理业务解释 |
| 15D-01 | ✅ COMPLETE | F-01..F-05 manifest、F-03/F-04 合成资产和统一 Gate 骨架已落地 |
| 15D-02 | ✅ COMPLETE | `pixel_diff_F04.csv` 仅 W 有差异；F-02 新旧 TIFF SHA-256 等价 |
| 15D-03 | ✅ COMPLETE | 28 个 golden SHA-256 零漂移；历史 Fixture 固定源姿态后 Quick CI PASS；统一 Gate 关闭 G3 |
| 15D-04 | ✅ COMPLETE | F-01 新包经项目内 `rip_reader_test --quiet` strict 读取通过，RIP 源码零改动 |
| 15D-05 | ⬜ PENDING | 等待工艺侧实物打样；继续阻断 Profile 启用 |
| 15E-01 | ✅ COMPLETE | UI 操作手册已补候选 Profile、选型原则与 G7 边界；PRD_12A 已回填 Stage 15 窄增量关系 |
| 15E-02 | ⛔ BLOCKED | 等待 G7 实物打样；候选继续保持 disabled/diagnostic |
| 15E-03 | ✅ COMPLETE | Stage 14 Q2 已回填 W 载体自动 Gate、strict Reader 与零漂移证据；外部 RIP、G7 和 12G 边界保留 |

---

## 15A · 配置契约

### 15A-01 冻结基线并扩展 TextureConfig
- **文件**：`output/benchmarks/stage15/baseline_identity.json`、`src/slicer_core/config.h`（`TextureConfig`，:98-109）
- **内容**：在第一次代码编辑前记录 HEAD、`git status --short`、构建轨道、编译器/配置、Profile/fixture/golden 哈希；若工作树含非 Stage 15 代码改动，必须先提交或隔离，不能把脏工作树笼统当作基线。随后新增 `unprintable_white_policy{"fail_closed"}`、`unprintable_white_ink_threshold{0}`、`unprintable_white_value{0}`
- **出口**：基线身份可复核；编译通过；默认值与既有行为一致
- **依赖**：无

### 15A-02 解析与校验
- **文件**：`src/slicer_core/config.cpp`
- **内容**：紧邻 `nonSurfaceRgbPolicy` 解析三字段（`read_u8` 用于两个 uint8）；新增 policy 枚举校验；策略启用时 value 必须为 0..254；拒绝 materialPolicy/roleMapping/非目标路径的未支持组合
- **出口**：U15-05、U15-06、U15-08、U15-09 通过
- **依赖**：15A-01

### 15A-03 扩展 materialProcessProfile 枚举
- **文件**：`src/slicer_core/config.cpp`
- **内容**：`white.mode` 允许 `unprintable_white_underbase`；`white.coverage` 允许 `texture_unprintable_white`
- **注意**：先确认现有是否存在 allowlist；若无则本卡仅需确认并记录，不得凭空新增限制
- **出口**：新 Profile 配置能正常加载
- **依赖**：15A-01

### 15A-04 锁定 underbase 检查隔离
- **文件**：`src/slicer_core/slicer.cpp:3732` 附近 + 单测
- **内容**：确认 `profile.white.mode == "underbase"` 为**精确**字符串匹配，新枚举值不触发 `E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW`；**补单测锁定**，防后续改为模糊匹配引入回归
- **出口**：U15-07 通过
- **依赖**：15A-03

---

## 15B · 合成器补白与统计

### 15B-01 判定谓词与写入
- **文件**：`src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.*`、`src/slicer_core/slicer.cpp`（写入点 :3059-3072）、对应 CMake target
- **内容**：新增可直接单测的 STL-only `IsUnprintableWhiteTexel()`；在 `update_texture_report_for_color` 之后按判据写 `pixels.at(base + 3U)`；公共 API 含 Doxygen
- **硬约束**：不触碰 `base+4`/`base+5`；不改 `base+0/1/2`；不改 ownership 掩码
- **出口**：U15-01..U15-04 通过；I15-01 切片 PASS
- **依赖**：15A-02

### 15B-02 核心统计累加
- **文件**：`slicer.cpp` 的 `LayerSemanticStats`、逐层/汇总累加路径
- **内容**：新增核心计数 `unprintable_white_carrier_pixels`；确保 W channel stats 与 `white_print_pixels` 覆盖新路径（**注意** legacy texture 分支不经过 `write_material_pixel()`，其内的累加不生效）
- **出口**：计数与实际写 W 像素一一对应
- **依赖**：15B-01

### 15B-03 报告证据与层级一致性
- **文件**：`slicer.cpp` 的 `LayerSemanticStats`、`layer_diagnostics_to_json()`、`material_process_report_to_json()`、`slice_report`
- **内容**：新增 `unprintableWhiteCarrierPixels` 的逐层和汇总证据；不扩展 `MaterialClosureSemanticLayerInput`，不新增跨 adapter 掩码
- **出口**：C-02；`slice_report.json` 与 `material_process_report.json` 按真实 layerIndex 给出一致计数，且总计等于逐层求和
- **依赖**：15B-02

### 15B-04 性能校验
- **内容**：相同 Release 构建、F-03/F-04 各预热 1 次并计量 7 次；基线与候选交替运行，只比较 `sliceProcessingMs` p50，不含 TIFF/preview/report IO。为降低微型 fixture 的计时噪声，基线与候选统一使用 2400 DPI、XY 4 倍的基准专用栅格放大，不改动生产配置
- **出口**：退化 ≤ 2%（NFR-02）
- **依赖**：15B-01

---

## 15C · 预检与错误信息（可与 15B 并行）

### 15C-01 纹理纯白扫描与 Profile 匹配告警
- **文件**：`apps/slicer_debug_ui/services/TextureWhitePreflightService.*`、对应 CMake target/单测；复用导入资产或 `SceneViewSurfacePreview.contenthash`
- **内容**：纹理加载后扫描严格 `255,255,255` 并按规范化路径、大小、mtime、内容哈希缓存；若含纯白且 Profile 能力集不含 `unprintable_white_underbase`，切片前给出保守告警并指名替代 Profile。扫描必须走现有异步任务/worker 机制，不得阻塞 UI 线程；结果必须绑定 scene/revision/contentHash，过期结果丢弃
- **硬约束**：不得重复扫描；整图扫描结果不得作为硬阻断，须说明未使用 UV texel 可能造成保守告警
- **出口**：C-15 告警、C-16 不误报
- **依赖**：无

### 15C-02 EffectiveConfigGenerator 告警分支
- **文件**：`apps/slicer_debug_ui/services/EffectiveConfigGenerator.cpp`（:529-548 附近）
- **内容**：`NormalizeModelFillTextureContract` 的纠正逻辑**保持不变**；消费 15C-01 预检结果并映射到当前 Profile 能力，覆盖 `fillMaterial == "rgb"` 的静默路径；`ConfigValidator` 只做 JSON 字段校验
- **出口**：rgb 路径不再静默，警告身份绑定当前 scene/revision/Profile
- **依赖**：15C-01
- **状态**：✅ COMPLETE（2026-08-04）
- **证据**：`production_effective_config_unit_tests` 覆盖 RGB 静默路径、能力抑制与 stale identity 丢弃；`texture_white_preflight_service_unit_tests` 锁定 Profile 身份；Release Qt self-test PASS

### 15C-03 场景错误信息业务化
- **文件**：`src/slicer_core/pipeline/SceneLayerComposer.cpp` 错误构造处（:468-490）
- **内容**：当失败像素六通道全为 `empty_value` 且 `ownership.model=1` 时，在既有协议措辞**之后**追加业务解释句
- **硬约束**：只增不改 —— 既有错误码与前半段措辞不得变更（下游可能已在匹配）
- **出口**：C-17
- **依赖**：无
- **状态**：✅ COMPLETE（2026-08-04）
- **证据**：`multi_model_layer_composer_unit_tests` 验证既有前缀稳定、纯白失败包含按需补白建议，普通非空闭合失败不误标为纯白纹理

---

## 15D · 验收与证据

### 15D-01 Fixture 准备
- **内容**：新建 F-03 四值条带 fixture（须含 `255,255,255` / `254,255,255` / `254,254,254` / `128,128,128`）；F-04 全纯白 fixture；建立 `scripts/run_stage15_white_carrier_gate.ps1` 的参数、输出目录和 summary schema 骨架
- **出口**：fixture 可复现并纳入版本管理；自动 Gate 入口可发现全部 fixture
- **依赖**：无

### 15D-02 差异证据产出
- **内容**：实现 `scripts/run_stage15_white_carrier_gate.ps1` 的自动 Gate；F-04 纯策略/单层组合 fixture 在策略关闭/开启下逐像素 diff，产出 `output/benchmarks/stage15/pixel_diff_F04.csv`；同时比较 F-02 新旧 Profile 的 manifest layer TIFF SHA-256；旧 Profile 对 F-04 另做 fail-closed 负向测试，不绕过闭合生成旧生产包
- **出口**：**G2 硬门** —— 差异仅在通道 3；R/G/B/S/V 差异数恒为 0；每处差异反查 RGB 均为 255,255,255
- **依赖**：15B-01

### 15D-03 零漂移回归
- **内容**：复用 15A-01 在代码修改前产出的 `baseline_identity.json` 与 golden before SHA-256；实现后重跑并产出 after 清单。历史 Golden/Stage 10 Fixture 通过运行时配置显式关闭自动定向，固定源姿态且不修改生产 Profile。G4 只比较 manifest layer list 指向的 TIFF，不比较 Profile/报告元数据
- **出口**：**G3 硬门** —— 逐字节相同；`run_ci_quick.ps1` 不新增红项
- **依赖**：15B-01

### 15D-04 RIP 兼容验证
- **内容**：`rip_reader_test` strict 读取新包
- **出口**：G5 —— 项目内 strict Reader 通过且本仓库 RIP 侧 diff 为空；不得据此宣称外部目标 RIP 已验收
- **依赖**：15B-01

### 15D-05 实物打样
- **内容**：F-01 新 Profile 输出实际打印，检查白区固化、交界过渡，与白墨填充 Profile 对照
- **出口**：**G7 —— 唯一非自动化门**。未完成前 Profile 不得启用
- **依赖**：15D-02
- **责任方**：工艺侧（非开发）

---

## 15E · 交付收口

### 15E-01 文档同步
- **内容**：更新 `docs/user_guides/QT_DEBUG_UI_操作手册.md` 新增 Profile 与选型指引；`PRD_12A` 补充本阶段增量引用
- **依赖**：15D-02
- **状态**：✅ COMPLETE（2026-08-04）
- **证据**：操作手册已区分全实体 RGB 兼容、白墨填充和按需补白候选；PRD_12A 明确 Stage 15 不改变默认填充、支撑、光油和协议语义

### 15E-02 启用 Profile
- **文件**：`samples/scenarios/slicer_scenarios.json`
- **内容**：将 `enabled` 由 `false` 翻转为 `true`，同时将 `productionSafety` 从 `diagnostic` 翻转为 `production`
- **前置**：**G7 已通过**
- **依赖**：15D-05

### 15E-03 回填 Stage 14 Q2
- **文件**：`docs/slice/DOC/DOC_ANALYSIS_14_Q2_RIP白区带内信号与配置冲突审查.md`、`docs/slice/DOC/DOC_CHECKLIST_14_对RIP侧技术确认清单.md`
- **内容**：向 Q2 追加“显式不透明白 Profile 可用 W 承载”的实证，不替换 Q2，不解冻“同一全 RGB Package 透明/不透明复用”的 12G 问题
- **依赖**：15D-04
- **状态**：✅ COMPLETE（2026-08-04）
- **证据**：F-01/F-02/F-04、项目内 strict Reader、28/28 Golden 与 Quick CI 结果已回填；明确外部目标 RIP 与 G7 尚未验收

---

## 2. 依赖图

```text
15A-01 → 15A-02 → 15B-01 → ┬─ 15B-02 → 15B-03
                            ├─ 15B-04
                            ├─ 15D-02 → 15D-05 → 15E-02
                            ├─ 15D-03
                            └─ 15D-04 → 15E-03
15A-01 → 15A-03 → 15A-04
15C-01 → 15C-02
15C-03                       （独立，可全程并行）
15D-01                       （独立，越早越好）
15D-02 → 15E-01
```

## 3. 建议执行顺序

```text
第 1 批（并行）：15A-01、15D-01、15C-01、15C-03
第 2 批（并行）：15A-02、15A-03、15C-02；15A-03 → 15A-04
第 3 批：15B-01 → 15B-02 → 15B-03；15B-04 可在 15B-01 后并行
第 4 批（并行）：15D-02、15D-03、15D-04
第 5 批：15E-01、15E-03；15D-05（工艺侧）通过后再执行 15E-02
```

## 4. 自动验证入口

```powershell
cmake --build build-slicesoft/main --config Release --target `
  texture_white_carrier_policy_unit_tests `
  texture_white_preflight_service_unit_tests `
  experimental_config_unit_tests `
  production_effective_config_unit_tests `
  slicer_cli `
  rip_reader_test
ctest --test-dir build-slicesoft/main -C Release `
  -R "^(texture_white_carrier_policy_unit_tests|texture_white_preflight_service_unit_tests|experimental_config_unit_tests|production_effective_config_unit_tests)$" `
  --output-on-failure
.\scripts\run_stage15_white_carrier_gate.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/stage15
```

该脚本及前两个测试 target 由 15B-01、15C-01、15D-01/02 创建；在对应任务落地前命令不存在，不得虚构执行结果。

## 5. 停止条件

出现以下任一情况**立即停止并回报**，不得自行绕过：

```text
✗ 发现必须修改 p0.rgbwsv.2 协议或 SourcePixelHasClosure 才能实现
✗ 15D-03 显示既有 golden 出现任何字节变化
✗ 15D-02 显示 S 或 V 通道存在差异
✗ 发现需要修改 RIP 侧才能通过 strict 读取
✗ 需要修改 obj_mtl_texture_rgb_only.json 才能达成目标
```
