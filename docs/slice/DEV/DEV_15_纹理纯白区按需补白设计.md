# DEV_15 纹理纯白区按需补白设计

> 阶段：Stage 15 ｜ 状态：**ACTIVE / DEVELOPMENT READY** ｜ 版本：v1.3 ｜ 日期：2026-08-04
> 上游：`PRD_15` / `DOC_DECISION_15`
> 证据等级：A=已核实代码事实（含行号），B=目标设计

---

## 1. 现状代码事实（A）

### 1.1 闭合校验 —— 不需要改动

`src/slicer_core/pipeline/SceneLayerComposer.cpp:250-291`，Model 所有权分支：

```cpp
bool modelMaterialPresent{false};
for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
{
    const std::uint8_t value = layer.output.channels.at(base + channel);
    if (channel == kSupportChannel) {            // idx 4：由 supportownership 决定，严格相等
        const std::uint8_t expected = layer.supportownership.at(pixelIndex) != 0U
            ? protocol.print_value : protocol.empty_value;
        if (value != expected) { return false; }
        continue;
    }
    if (channel == kVarnishChannel) {            // idx 5：写非空值必须有 varnish ownership
        const bool modelVarnish = layer.modelvarnishownership.at(pixelIndex) != 0U;
        if (modelVarnish) {
            if (value == protocol.empty_value) { return false; }
            modelMaterialPresent = true;
        } else if (value != protocol.empty_value
                   && layer.outervarnishownership.at(pixelIndex) == 0U) {
            return false;
        }
        continue;
    }
    if (channel < kSupportChannel && value != protocol.empty_value) {   // idx 0,1,2,3 = R,G,B,W
        modelMaterialPresent = true;
    }
}
return modelMaterialPresent;
```

**关键**：`channel < kSupportChannel(4)` 覆盖 R/G/B/**W**，规则是"至少一种非空"，无互斥判断。写 W 天然合法，**本文件一行都不需要改**。

同时可见 **V 通道的额外代价**：写 V 必须同步设置 `modelvarnishownership`，属跨模块改动 —— 这是本方案排除 V 的直接依据。

### 1.2 故障写入点

`src/slicer_core/slicer.cpp:3059-3072`，legacy texture 分支：

```cpp
} else if (config.texture.enabled
           && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index)) {
    const TextureColumnColor color = resolve_texture_color(config, texture_columns, pixel_index);
    pixels.at(base + 0U) = color.rgb.at(0);
    pixels.at(base + 1U) = color.rgb.at(1);
    pixels.at(base + 2U) = color.rgb.at(2);
    update_texture_report_for_color(color, texture_report);
    counted_model_pixel = true;
    texture_surface_pixel = true;
    const ModelFillMaterial fill_material = ResolveModelFillMaterial(config, nullptr);
    if (ShouldApplyModelFill(config, texture_surface_pixel, fill_material)     // rgb → false
        && WriteModelFillPixel(pixels, base, config, nullptr)) {
        model_fill_pixel = true;
    }
}
```

`ShouldApplyModelFill`（`slicer.cpp:2583-2601`）在 `material == Rgb && textureSurfacePixel` 时返回 false —— 纹理表面像素得不到任何补充材料。

> **注意 `applyMode = solid_volume_from_top_surface` 的放大效应**：颜色沿整列投影，`ShouldApplyTextureToLayer` 对整个模型列返回 true。因此白色区域的**整个体积**都是 `255,255,255`，故障是体量级的而非仅表面一层。补白同样作用于整列 —— 白色物体整体由白墨承载，物理上正确。

### 1.3 既有同类校验先例

`src/slicer_core/config.cpp:803-810`：

```cpp
if (config.model_fill.enabled
    && !config.model_fill.empty_allowed_in_production
    && !config.model_fill.legacy_rgb_fallback
    && config.model_fill.material == "rgb"
    && config.texture.enabled
    && config.texture.non_surface_rgb_policy == "empty") {
    throw std::runtime_error(
        "modelFill production profile cannot use rgb fill with texture.nonSurfaceRgbPolicy=empty");
}
```

已识别"rgb fill 产生空像素"问题族，仅覆盖 `nonSurfaceRgbPolicy=empty` 分支。本阶段补齐"纹理自身为纯白"分支。

### 1.4 现有 TextureConfig（A）

`src/slicer_core/config.h:98-109`：

```cpp
struct TextureConfig {
    bool enabled{false};
    std::string apply_mode{"solid_volume_from_top_surface"};
    int top_surface_layers{1};
    std::string sampler{"bilinear"};
    std::string uv_address_mode{"clamp"};
    bool flip_v{true};
    std::array<std::uint8_t, 3> fallback_rgb{0, 0, 0};
    std::string missing_texture_policy{"warn_and_fallback"};
    std::string non_surface_rgb_policy{"model_material"};
    TextureSurfaceShellConfig surface_shell;
};
```

新字段采用同族扁平字符串策略风格。

## 2. 目标设计（B）

### 2.1 配置结构扩展

```cpp
struct TextureConfig {
    // ... 既有字段不变 ...
    std::string   unprintable_white_policy{"fail_closed"};   // fail_closed | white_underbase
    std::uint8_t  unprintable_white_ink_threshold{0};
    std::uint8_t  unprintable_white_value{0};
};
```

JSON 映射（`config.cpp` 解析段，紧邻 `nonSurfaceRgbPolicy`）：

```cpp
config.texture.unprintable_white_policy =
    texture.value("unprintableWhitePolicy", config.texture.unprintable_white_policy);
config.texture.unprintable_white_ink_threshold =
    read_u8(texture, "unprintableWhiteInkThreshold", config.texture.unprintable_white_ink_threshold);
config.texture.unprintable_white_value =
    read_u8(texture, "unprintableWhiteValue", config.texture.unprintable_white_value);
```

校验（`config.cpp` 校验段）：

```cpp
if (config.texture.unprintable_white_policy != "fail_closed"
    && config.texture.unprintable_white_policy != "white_underbase") {
    throw std::runtime_error(
        "texture.unprintableWhitePolicy must be fail_closed or white_underbase");
}
```

策略启用时还必须校验 `unprintableWhiteValue != output.emptyValue`。当前协议空值固定为 255，故有效范围为 `0..254`；配置 255 必须 fail-closed。首版只允许 Legacy、`materialPolicy.enabled=false` 且未启用 `materialRoleMapping`，其它组合不得静默忽略策略。

### 2.2 判定谓词

为保证谓词可直接单测，新增 STL-only 小模块，而不是把函数藏在 `slicer.cpp` 匿名命名空间：

```text
src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h
src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.cpp
tests/unit/texture_white_carrier_policy/Main.cpp
```

公开 API 必须含 Doxygen 注释。建议接口：

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

核心模块不依赖 Qt、JSON 或 `SliceConfig`，由调用方传入已验证的策略值。

### 2.3 写入点改造

`slicer.cpp:3059-3072` 分支内，`update_texture_report_for_color` 之后插入：

```cpp
if (IsUnprintableWhiteTexel(
        config.texture.unprintable_white_policy,
        config.texture.unprintable_white_ink_threshold,
        color.rgb)) {
    pixels.at(base + 3U) = config.texture.unprintable_white_value;   // W 通道，仅此一处
    ++semantic_stats.unprintable_white_carrier_pixels;
}
```

**不得触碰** `base + 4`(S) 与 `base + 5`(V)；**不得**修改 `base + 0/1/2`；**不得**改动 ownership 掩码。

> 若 `materialPolicy` / `materialRoleMapping` 分支（`slicer.cpp:3031-3058` / 之前分支）后续也需要该能力，应复用同一谓词而非复制逻辑。**本阶段仅改造 legacy texture 分支**，因为新 Profile 的 `materialPolicy.enabled=false` 且未配置 `materialRoleMapping`，只会走该分支。

### 2.4 统计与报告

| 项 | 处理 |
|---|---|
| `semantic_stats.unprintable_white_carrier_pixels` | 新增，逐层累计 |
| `white_print_pixels` | 必须覆盖新路径。注意现有累加仅在 `write_material_pixel()` 内（`slicer.cpp:2807-2809`），legacy texture 分支不经过该函数，需单独累加 |
| `reports/slice_report.json` | `layers[].unprintableWhiteCarrierPixels`、`totals.unprintableWhiteCarrierPixels` |
| `reports/material_process_report.json` | `layers[].unprintableWhiteCarrierPixels`、`white.unprintableWhiteCarrierPixels` |

不新增 `MaterialClosureSemanticLayerInput` 掩码。补白后的生产 W 值已能参与闭合，额外掩码会无必要地扩大 Legacy/Global adapter、诊断和修复模块改动面。

### 2.5 materialProcessProfile 枚举扩展（report-only）

```text
white.mode      新增合法值：unprintable_white_underbase
white.coverage  新增合法值：texture_unprintable_white
```

**必须同时确认**：`slicer.cpp:3732` 的 underbase 覆盖率检查

```cpp
if (profile.white.enabled && profile.white.mode == "underbase" && missing_underbase_pixels > 0U)
    validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW");
```

该检查按字符串精确匹配 `"underbase"`，新枚举值 `"unprintable_white_underbase"` **不等于** `"underbase"`，故天然不触发。**但必须写单测锁定这一点**，防止后续有人改成 `find("underbase") != npos` 之类的模糊匹配而引入回归。

### 2.6 预检与告警（15C）

| 位置 | 改动 |
|---|---|
| `apps/slicer_debug_ui/services/EffectiveConfigGenerator.cpp` | 现有 `NormalizeModelFillTextureContract`（:529-548）在 `fillMaterial == "rgb"` 时提前 return false。**不改其纠正逻辑**，另加独立告警分支 |
| `apps/slicer_debug_ui/services/ConfigValidator.cpp` | 只校验字段类型、枚举、数值范围与不支持组合，不读取模型或贴图 |
| `apps/slicer_debug_ui/services/TextureWhitePreflightService.*` | 复用导入后的纹理或 `SceneViewSurfacePreview.contenthash` 扫描并缓存；发现严格纯白且 Profile 不支持时给出保守告警 |
| 场景错误信息 | `SCENE_PRODUCTION_PACKAGE_INVALID` 追加业务解释（条件：六通道全 `empty_value` 且 `ownership.model=1`） |

纹理纯白扫描应在纹理加载后一次性完成并缓存，不得逐像素重复扫描。缓存键至少包含规范化资产路径、文件大小、修改时间和内容哈希。若扫描的是整张源贴图而非实际 UV 命中 texel，告警必须标注为保守判断且不得阻断切片。

扫描通过现有 worker/异步任务模式执行，不在 Qt UI 线程中解码或全图遍历。请求和结果至少携带 `sceneId`、`revision`、`contentHash`；完成回调必须再次核对当前身份，过期结果直接丢弃。相同缓存键的并发请求采用 single-flight，避免重复解码。销毁窗口或切换场景时允许任务自然完成，但禁止回写已销毁对象或新场景。

### 2.7 路径责任矩阵

| 路径 | Stage 15 行为 |
|---|---|
| Legacy 新按需补白 Profile | 实现并验证 |
| Legacy 旧严格 RGB Profile | 保持纯白 fail-closed |
| Global Surface Shell | 已有精确白写 W，代码不改，只回归 |
| materialPolicy / materialRoleMapping | 本阶段不支持新策略，配置期 fail-closed |

## 3. 不改动清单（硬约束）

```text
✗ src/slicer_core/pipeline/SceneLayerComposer.cpp      闭合规则原样
✗ RgbwsvProtocol / p0.rgbwsv.2                          协议原样
✗ RgbwsvPackageWriter / TIFF 结构                       原样
✗ 通道顺序 R G B W S V / uint8 / black_is_print         原样
✗ samples/configs/material_process/obj_mtl_texture_rgb_only.json   原样
✗ RIP 侧                                                零改动
✗ slicer.cpp:3711 unexpected_overlap_pixels             本阶段不动（独立立项）
```

## 4. 测试设计

### 4.1 单元测试

| ID | 用例 | 断言 |
|---|---|---|
| U15-01 | `IsUnprintableWhiteTexel` policy=fail_closed | 任意 RGB 均返回 false |
| U15-02 | policy=white_underbase, threshold=0, rgb=255,255,255 | true |
| U15-03 | policy=white_underbase, threshold=0, rgb=254,255,255 | false |
| U15-04 | policy=white_underbase, threshold=1, rgb=254,254,254 | true |
| U15-05 | 配置解析：缺省字段 | policy=fail_closed, threshold=0, value=0 |
| U15-06 | 配置校验：非法 policy 值 | 抛出且信息含字段名 |
| U15-07 | **枚举隔离**：`white.mode="unprintable_white_underbase"` + white≪rgb | **不**产生 `E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW` |
| U15-08 | policy=white_underbase, value=255 | 配置期拒绝，信息说明不得等于 emptyValue |
| U15-09 | policy 与 Global/materialPolicy/roleMapping 非支持组合 | 配置期 fail-closed |

### 4.2 集成测试

| ID | 用例 | 断言 |
|---|---|---|
| I15-01 | 纯白纹理 fixture + 新 Profile | 切片 PASS，闭合校验通过 |
| I15-02 | 同 fixture + 旧 Profile | 仍 `SCENE_PRODUCTION_PACKAGE_INVALID`（兼容基线行为保持） |
| I15-03 | 无纯白纹理 fixture，新旧 Profile | manifest layer list 对应 TIFF SHA-256 一致；元数据允许不同 |
| I15-04 | 全部既有 golden | SHA-256 逐字节不变 |
| I15-05 | 纯策略/单层组合 fixture，开启 vs 关闭 | 差异**仅**在 W 通道，且**仅**在命中判据处；不要求旧 Profile 生成生产包 |
| I15-06 | 项目内 RIP strict Reader 读取新包 | 通过，无警告升级 |
| I15-07 | Global 既有精确白场景 | 生产结果和 golden 不变 |
| I15-08 | 预检任务执行中切换场景/替换纹理 | 旧 scene/revision/contentHash 结果被丢弃，UI 不串线且不崩溃 |

I15-05 是 G2 的直接证据，必须产出可复核的差异清单而非仅断言布尔值。

## 5. 性能

补白策略字符串在进入像素循环前解析一次；热循环只执行阈值下界与 RGB 三通道比较，命中时写 W，无额外遍历。Release 下固定机器与存储，预热 1 次、计量 7 次，基线/候选交替运行，以 `sliceProcessingMs` p50 比较，不把 TIFF/preview/report IO 纳入策略开销；要求退化 ≤ 2%（NFR-02）。微型 F-03/F-04 对两组策略统一使用 2400 DPI、XY 4 倍的基准专用放大以降低计时噪声，生产配置保持不变。

## 6. 基线与回滚

15A-01 的第一步、且必须早于任何 Stage 15 代码编辑：记录 HEAD、`git status --short`、构建轨道、编译器/配置、Profile/fixture 哈希和既有 golden TIFF 哈希。若工作树包含非 Stage 15 代码改动，先提交或隔离；不得以脏工作树作为模糊基线。15D-03 复用该 before 证据并生成 after 清单。历史 Golden 与 Stage 10 输出契约必须通过 `output/golden_runtime_configs` 下的运行时副本显式关闭自动定向，以固定合成资产源姿态；禁止为通过测试而修改生产 Profile 或 golden 期望值。

回滚只需移除新配置字段、策略模块、Legacy 调用、报告新增字段和 UI 预检服务；旧 Profile、协议、Writer、Reader 与 Global 路径均不修改。

## 7. 自动化入口

Stage 15 新增单一 Release Gate，统一调用单测、fixture、场景切片、TIFF 差异、strict Reader 和性能测量：

```powershell
.\scripts\run_stage15_white_carrier_gate.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/stage15 `
  -VerifyZeroDrift `
  -VerifyPerformance
```

脚本输出 `stage15_white_carrier_summary.json`，至少包含构建身份、fixture/Profile 哈希、G1..G6 状态、TIFF SHA-256、通道差异计数、Reader 结果和 p50 性能数据。`-VerifyZeroDrift` 负责 G3 的 golden 哈希与 Quick CI 硬门，`-VerifyPerformance` 调用独立性能脚本并关闭 NFR-02；G7 实物证据不由脚本伪造，只在 summary 中记录 `physicalProof=pending|passed|failed`。
