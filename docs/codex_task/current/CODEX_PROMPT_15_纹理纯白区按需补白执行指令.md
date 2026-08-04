# CODEX_PROMPT_15 纹理纯白区按需补白执行指令

> 阶段：Stage 15 ｜ 状态：**ACTIVE / DEVELOPMENT READY** ｜ 版本：v1.2 ｜ 日期：2026-08-04
> 配套：`TASKS_15` / `DEV_15` / `DEMO_15` / `PRD_15` / `DOC_DECISION_15`

---

## 一、任务背景（先读这段）

在 `black_is_print` 语义下 `255 = 无墨`。「全实体 RGB」Profile 只写 R/G/B 三通道，
因此纹理中的**纯白像素**（`R=G=B=255`）产生的六通道字节是
`255,255,255,255,255,255` —— **与背景像素完全相同**。

场景合成的材料闭合校验因此判定"该像素属于模型却没有任何材料"，拒绝生产包：

```text
SCENE_PRODUCTION_PACKAGE_INVALID: instance RGBWSV bytes do not close against
material ownership; pixel=38085 values=255,255,255,255,255,255 ownership=1,0,0
```

**这是 fail-closed，现有保护是对的。** 你要做的不是修 bug，而是**补一项当前物理上无法表达的能力**：
对这类像素按需补写白墨（W 通道）。

## 二、最重要的一条事实（决定了工作量）

`src/slicer_core/pipeline/SceneLayerComposer.cpp:250-291` 的闭合判据：

```cpp
if (channel < kSupportChannel && value != protocol.empty_value) {
    modelMaterialPresent = true;
}
return modelMaterialPresent;
```

`kSupportChannel = 4`，所以 `channel < 4` 覆盖 **R、G、B、W 四个通道**。
规则是"**至少一种材料非空**"，不是"恰好一种"，**代码中不存在 RGB/W 互斥判断**。

结论：

```text
✅ 255,255,255,  0,255,255  ownership=1,0,0   →  闭合通过
```

**所以协议层完全不需要改动。** 你不需要动 `p0.rgbwsv.2`、不需要动 `SourcePixelHasClosure`、
不需要动 writer、不需要动 TIFF 结构、**不需要 RIP 侧任何配合**。

阻碍只在合成器的写入路径与配置 schema 上。

## 三、为什么只做 W、不做 V

同一函数的 V 通道分支（:266-285）要求：写非空值时**必须**有
`modelvarnishownership != 0` 或 `outervarnishownership != 0`，否则 `return false`。

即写 V 要跨模块同步维护 ownership 掩码（合成器 + 掩码生成 + 场景传递三处）。
而 V 对本问题**无必要** —— 白墨才是打白的材料，光油是表面处理。

**只做 W。看到任何"顺便把 V 也支持一下"的冲动，请停下。**

## 四、阈值为什么默认取 0

因为闭合判据是 `value != 255`，所以 `254,254,254` 已经能通过闭合
（`254 != 255` → `modelMaterialPresent = true`）。**只有严格 `255,255,255` 会失败。**

所以 `unprintableWhiteInkThreshold = 0`（严格相等）对消除闭合失败**充分且必要**。
大于 0 的阈值属于印刷质量调优选项，不是正确性要求。

## 五、执行范围

### 要做

```text
15A  配置契约：TextureConfig 新增 3 字段 + 解析 + 校验 + profile 枚举扩展
15B  合成器：新增可单测的 TextureWhiteCarrierPolicy，在 slicer.cpp:3059-3072 分支内按判据写 base+3
15C  预检：独立异步服务扫描纹理纯白并匹配 Profile 能力，给出保守告警；错误信息补业务解释
15D  验收：fixture、差异证据、零漂移回归、RIP 验证、实物打样
15E  收口：文档同步、翻转 Profile enabled、回填 Stage 14 Q2
```

### 明确不要做

```text
✗ 不修改 p0.rgbwsv.2 协议定义
✗ 不修改 SourcePixelHasClosure 的判定规则
✗ 不修改 RgbwsvPackageWriter / TIFF 结构 / 通道顺序 / 位深 / 极性
✗ 不修改 samples/configs/material_process/obj_mtl_texture_rgb_only.json
✗ 不实现 V 通道共写
✗ 不修改 slicer.cpp:3711 的 unexpected_overlap_pixels（已知缺陷，独立立项）
✗ 不重建 Profile 配置文件（已交付，见第七节）
```

## 六、三条硬性不变量

```text
① 只写 W(idx 3)。触碰 S(idx 4) 或 V(idx 5) 即为越界，验收直接判不通过。
② 不改变 R/G/B 的既有取值 —— 纯白像素仍然是 255,255,255。
   补白的语义是"在保持颜色不变的前提下补上承载它的白墨"，与真实 UV 打印一致，
   不是"把白色改成别的颜色"。
③ 不改变任何 ownership 掩码 —— 该像素本来就是 modelownership=1。
```

## 七、已交付的配置产物（不要重建）

| 文件 | 状态 |
|---|---|
| `samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json` | ✅ 已创建 |
| `samples/scenarios/slicer_scenarios.json` 新条目 | ✅ 已注册，**`enabled: false`** |

`enabled: false` 是刻意的：代码落地前不让用户在 UI 选到会报错的条目。
**任务 15E-02 在 G7（实物打样）通过后才把 `enabled` 翻转为 `true`，并把 `productionSafety` 从 `diagnostic` 翻转为 `production`。**

## 八、核心实现合同

### 判定谓词（独立 STL-only 模块，可直接单测）

文件：`src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.*`

```cpp
bool IsUnprintableWhiteTexel(
    std::string_view policy,
    std::uint8_t inkThreshold,
    const std::array<std::uint8_t, 3>& rgb) noexcept
{
    if (policy != "white_underbase") {
        return false;
    }
    const std::uint8_t minChannel = std::min({rgb.at(0), rgb.at(1), rgb.at(2)});
    const int ink = 255 - static_cast<int>(minChannel);
    return ink <= static_cast<int>(inkThreshold);
}
```

### 写入点（`slicer.cpp:3059-3072`，`update_texture_report_for_color` 之后）

```cpp
if (IsUnprintableWhiteTexel(
        config.texture.unprintable_white_policy,
        config.texture.unprintable_white_ink_threshold,
        color.rgb)) {
    pixels.at(base + 3U) = config.texture.unprintable_white_value;
    ++semantic_stats.unprintable_white_carrier_pixels;
}
```

不得新增 `MaterialClosureSemanticLayerInput` 掩码；诊断证据写入 `slice_report.json` 与 `material_process_report.json` 的 `unprintableWhiteCarrierPixels` 字段。

## 九、五个容易踩的坑

### 坑 1 · `white_print_pixels` 不会自动累加

现有累加只在 `write_material_pixel()` 内（`slicer.cpp:2807-2809`），
而 legacy texture 分支**不经过该函数**。补白像素必须单独累加，否则报告白墨数为 0。

### 坑 2 · 不要复用 `mode: "underbase"`

`slicer.cpp:3732`：

```cpp
if (profile.white.enabled && profile.white.mode == "underbase" && missing_underbase_pixels > 0U)
    validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW");
```

其中 `missing_underbase_pixels = rgb_print_pixels - white_print_pixels`。
本方案白墨像素数**远小于** RGB 像素数，复用 `"underbase"` 会**必然误报**。

新枚举值 `"unprintable_white_underbase"` 与 `"underbase"` 字符串不等，天然不触发 ——
**但必须补单测锁定**（15A-04），防止后续有人改成 `find("underbase") != npos` 引入回归。

### 坑 3 · `applyMode = solid_volume_from_top_surface` 的放大效应

该模式下颜色沿整列投影，白色区域的**整个体积**都是 `255,255,255`，
所以补白也会作用于整列。这是**预期行为**（白色物体整体由白墨承载），
不要试图"优化"成只补表面一层 —— 那会让实体内部重新出现空像素，闭合再次失败。

### 坑 4 · ConfigValidator 没有资产上下文

`ConfigValidator` 只能校验 JSON 结构、枚举和值域，不能负责读取 OBJ/MTL/纹理。纯白扫描必须放在独立预检服务，复用导入后的纹理/`SceneViewSurfacePreview.contenthash` 并缓存。整张源贴图扫描只能产生保守告警，不能替代核心 fail-closed。

### 坑 5 · Global 已有独立白像素行为

Stage 15 只改 Legacy 新 Profile。Global Surface Shell 已有精确白写 W 路径，本阶段只做 golden 回归；不要借机重构或迁移 Global 行为。

### 坑 6 · 基线记录不能晚于第一次代码编辑

`baseline_identity.json` 不是 15D 才补写的文档。必须在 15A-01 的第一处代码改动前记录 HEAD、工作树状态、构建轨道与 Profile/fixture/golden 哈希。若存在非 Stage 15 代码改动，先提交或隔离；不得将脏工作树描述为稳定基线。

### 坑 7 · 预检结果不能跨场景串线

纹理扫描不得阻塞 Qt UI 线程。结果必须携带 scene/revision/contentHash，回调时重新核对当前身份；用户切换场景、替换纹理或重新导入后，旧结果只能丢弃，不能显示到新场景。

## 十、验收硬门

| 门 | 标准 | 对应任务 |
|---|---|---|
| G2 | 纯策略/单层组合 fixture 在策略关闭/开启下逐像素 diff：差异**仅**在通道 3；R/G/B/S/V 差异数恒为 0；旧 Profile 另行保持 fail-closed | 15D-02 |
| G3 | `policy=fail_closed`（默认）时，既有 golden TIFF SHA-256 **逐字节不变** | 15D-03 |
| G4 | 无纯白纹理模型的新旧 Profile，其 manifest layer list 对应 TIFF SHA-256 一致；元数据允许不同 | 15D-02 |
| G5 | 项目内 RIP strict Reader 通过，且本仓库 RIP 侧 diff 为空；不冒充外部 RIP 验收 | 15D-04 |
| G7 | **实物打样确认**（唯一非自动化门，未过不得启用 Profile） | 15D-05 |

## 十一、停止条件

出现以下任一情况**立即停止并回报**，不得自行绕过：

```text
✗ 发现必须修改协议或闭合校验才能实现   →  设计有误，回来复核
✗ 既有 golden 出现任何字节变化         →  默认值路径被污染
✗ diff 中出现 S 或 V 通道差异          →  实现越界
✗ 需要修改 RIP 侧才能通过 strict       →  方案前提被破坏
✗ 需要修改 obj_mtl_texture_rgb_only    →  违反"新增而非改造"决策
```

## 十二、开工顺序

```text
第 1 批（并行）：15A-01、15D-01、15C-01、15C-03
第 2 批（并行）：15A-02、15A-03、15C-02；15A-03 → 15A-04
第 3 批：15B-01 → 15B-02 → 15B-03；15B-04 可在 15B-01 后并行
第 4 批（并行）：15D-02、15D-03、15D-04
第 5 批：15E-01、15E-03；15D-05（工艺侧）通过后再执行 15E-02
```

自动化总入口由 15D-01/02 落地：

```powershell
.\scripts\run_stage15_white_carrier_gate.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/stage15
```

完整任务卡见 `docs/codex_task/current/TASKS_15_纹理纯白区按需补白任务清单.md`。
实施前置与依赖审查见 `docs/slice/DOC/DOC_PREP_15_纹理纯白区按需补白实施准备与依赖审查.md`。
