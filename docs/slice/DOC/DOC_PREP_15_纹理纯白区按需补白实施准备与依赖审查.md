# DOC_PREP_15 纹理纯白区按需补白实施准备与依赖审查

> 文档状态：READY FOR DEVELOPMENT
> 版本：v1.1
> 审查日期：2026-08-04
> 审查范围：Stage 15 决策、PRD、DEV、DEMO、任务、执行指令、Profile 与场景注册
> 代码状态：尚未开始 Stage 15 实现；本文只关闭实施准备缺口

## 1. 准备结论

Stage 15 的问题边界、材料语义和最小实现方向成立：

```text
black_is_print 下，模型所有权像素若为 RGBWSV=(255,255,255,255,255,255)，
则与真实空白字节相同，无法通过生产材料闭合校验。

目标 Profile 仅在纹理判定为不可打印白时，同层、同 XY 像素补写 W；
不写 S/V，不修改 ownership，不修改协议、Writer、TIFF 和闭合判据。
```

初稿存在验收基线、模块归属、UI 预检、Global 路径和 Stage 14 关系等准备缺口。本文已给出约束，配套文档按本文修订后，Stage 15 可进入 `15A-01`。`15A-01` 必须先冻结可复核基线，再进行第一处代码编辑。外部实物打样 G7 不是开工依赖，但在 G7 通过前 Profile 必须保持禁用和诊断级状态。

## 2. 术语冻结

| 术语 | 本阶段唯一含义 |
|---|---|
| 不可打印白 | 纹理 RGB 满足策略阈值，默认严格等于 `255,255,255`，仅靠 RGB 无法产生打印通道 |
| 按需补白 | 在同一 `layerIndex`、同一 XY 像素写入 W 通道，不新增 Z 层 |
| `white_underbase` | 配置策略名；表示纹理白的材料载体，不等于 Stage 13G 支撑铺底，也不等于模型全覆盖白墨底层 |
| 旧 Profile | `textured_nail_rgb_only_lower_support`，保持严格 RGB 和纯白 fail-closed |
| 新 Profile | `textured_nail_rgb_white_ondemand_lower_support`，仅对不可打印白补写 W |

## 3. 实现范围矩阵

| 路径 | 当前事实 | Stage 15 处置 |
|---|---|---|
| Legacy，`materialPolicy=false`，无 `materialRoleMapping` | 当前纯白可能形成六通道全空 | **目标实现路径** |
| Legacy 旧严格 RGB Profile | 纯白 fail-closed | 保持不变，作为负向兼容基线 |
| Global Surface Shell | 已有精确白纹理写 W 的独立逻辑 | 不改行为，只做回归 |
| `materialPolicy` / `materialRoleMapping` 分支 | 具有独立材料映射语义 | 本阶段不扩展；不支持的组合必须配置期 fail-closed |
| Writer / TIFF / manifest / Reader | 已支持 RGBWSV 中 W 的合法生产值 | 不改协议和存储结构 |
| 外部 RIP | W=0 是常规白墨材料值 | Stage 15 不改 RIP；内部 strict Reader 通过不等于外部 RIP 已验收 |

## 4. 已关闭的准备缺口

### 4.1 可单测的策略模块

纯白判定不得只放在 `slicer.cpp` 匿名命名空间，否则 `U15-01..04` 无法直接测试。新增 STL-only 小模块：

```text
src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h
src/slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.cpp
tests/unit/texture_white_carrier_policy/Main.cpp
```

模块公开 API 使用 Doxygen 注释，函数使用 PascalCase，局部变量使用 camelCase。Legacy 调用该模块；Global 本阶段不迁移到新模块，以避免扩大行为变化。

### 4.2 配置校验

```text
unprintableWhitePolicy: fail_closed | white_underbase
unprintableWhiteInkThreshold: 0..255
unprintableWhiteValue: 0..254（策略启用时不得等于 emptyValue=255）
```

还必须拒绝 Stage 15 策略与本阶段未支持路径的组合。新 Profile 固定：

```text
Legacy
materialPolicy.enabled=false
materialRoleMapping.enabled=false
```

### 4.3 UI 预检职责

`ConfigValidator` 只持有 `QJsonObject`，没有模型、MTL、贴图或 UV 使用范围上下文，因此只负责字段结构与取值校验，不负责扫描贴图。

纯白预检由独立 `TextureWhitePreflightService` 负责，并复用导入阶段已得到的纹理/`SceneViewSurfacePreview.contenthash`。缓存键至少包含规范化资产路径、文件大小、修改时间和内容哈希。扫描通过现有异步 worker 模式执行，使用 single-flight 避免重复解码；结果绑定 scene/revision/contentHash，过期结果丢弃。扫描整个源贴图时只能给出“保守告警”，因为未被 UV 使用的纯白 texel 也可能被统计；告警不得成为硬阻断，核心材料闭合仍是最终真源。

### 4.4 报告证据而非闭合掩码

Stage 15 不新增 `MaterialClosureSemanticLayerInput` 掩码。该掩码不是闭合所需，并会扩大 Legacy/Global adapter、诊断和修复模块的改动面。

证据写入既有报告：

```text
reports/slice_report.json
  layers[].unprintableWhiteCarrierPixels
  totals.unprintableWhiteCarrierPixels

reports/material_process_report.json
  layers[].unprintableWhiteCarrierPixels
  white.unprintableWhiteCarrierPixels
```

字段为向后兼容新增；`whitePrintPixels` 仍表示 W 通道真实生产像素总数。

### 4.5 G2 与 G4 的可执行口径

旧 Profile 对全白 fixture 必须失败，不会产生可用于比较的生产包，因此 G2 不再要求“新旧两个生产包”比较。正确口径是：

```text
1. 用纯函数/单层组合 fixture 生成关闭策略与开启策略的内存层结果；
2. 证明 R/G/B/S/V 不变；
3. 证明 W 仅在命中判据的像素从 emptyValue 变为配置值；
4. 旧 Profile 另做负向测试，确认仍 fail-closed。
```

G4 只比较 manifest layer list 指向的生产 TIFF 文件 SHA-256。Profile 名、配置快照和报告元数据允许不同，不能要求整个 package 目录哈希相同。

### 4.6 性能 Gate

使用相同 Release 构建、机器和存储，隔离切片/组合计时，不把 TIFF/preview/report IO 纳入策略开销：

```text
fixture：F-03 与 F-04
预热：1 次
计量：7 次
统计：p50
Gate：相对 fail_closed 基线退化不超过 2%
```

如 2% 小于当前计时噪声，先记录噪声区间，不得用单次总耗时下结论。

### 4.7 基线身份

Stage 15 的 `15A-01` 必须在第一处代码修改前记录：

```text
git rev-parse HEAD
构建目录/生成器/配置
Profile 与 fixture SHA-256
既有 golden TIFF SHA-256
```

当前工作树存在其它阶段改动，基线必须绑定到明确 HEAD 和文件哈希，不能用“当前工作树”作为模糊基线。

如果开始 15A-01 时仍存在非 Stage 15 代码改动，应先由对应任务提交，或在独立工作树/分支中隔离。该操作 Gate 不影响“设计准备完成”的结论，但会阻止在当前脏工作树上直接开始编码。

### 4.8 Stage 14 / 12G 边界

Stage 15 证明的是“显式选择不透明白 Profile 时，可用 W 承载不可打印白”。它不解决 12G 冻结问题：同一个全 RGB Package 是否还能由 RIP 在透明与不透明白之间切换。

因此 Stage 15 只向 Stage 14 Q2 **追加实证和可选方案**，不得替换 Q2，也不得宣称 12G 已解冻。

### 4.9 构建与第三方依赖

Stage 15 不新增第三方库。核心判定模块仅依赖 C++ 标准库；UI 预检复用项目现有 Qt 5.15 `QImage`/worker 能力。新增 UI 预检单测 target 需链接 `Qt5::Core` 与 `Qt5::Gui`，不引入 OpenVDB、LibTIFF 或新的 vcpkg 包。

自动 Gate 固定使用默认 OpenVDB OFF 的 `build-slicesoft/main` Release 轨道。Global 只作为回归样本，不要求 OpenVDB ON 构建。

## 5. Fixture 与基线事实

准备审查已确认：

| Fixture | 纹理事实 | 用途 |
|---|---|---|
| `model/obj/小马物语/小马物语小指/MF_Xiao_ma_Xiaozhi_ty03.obj` | 纹理 1024x1024；严格纯白 34,183 像素；无透明像素 | G1 主路径 |
| 玫瑰 `04.obj` 所用 `zhongzhi1(4).png` | 严格纯白 0；近白 0；无透明像素 | G4 无纯白对照 |
| F-03 合成四值条带 | 待新增 | 阈值单测/组合测试 |
| F-04 全纯白 | 待新增 | 极端白载体正向与旧 Profile 负向 |

新 Profile JSON 是通用模板。G1 测试必须生成指向小马物语小指的专用测试配置，不得为了验收修改通用模板的默认模型路径。

## 6. Profile 生命周期

| 阶段 | `enabled` | `productionSafety` | 含义 |
|---|---:|---|---|
| 代码未完成或仅内部测试 | `false` | `diagnostic` | 不向普通用户暴露 |
| G1..G6 通过、G7 未完成 | `false` | `diagnostic` | `INTERNAL COMPLETE / PHYSICAL PROOF PENDING` |
| G7 通过且文档收口 | `true` | `production` | 可进入生产 Profile 列表 |

## 7. 开工 Gate

| Gate | 状态 | 说明 |
|---|---|---|
| 问题与协议边界 | PASS | W 可满足模型材料闭合，不改协议 |
| Legacy / Global 责任边界 | PASS | Legacy 实现，Global 回归 |
| 配置 schema 与非法组合 | PASS | 已冻结 |
| 单测入口 | PASS | 独立纯策略模块 |
| UI 预检职责 | PASS | 独立服务，ConfigValidator 只做结构校验 |
| 验收比较基线 | PASS | G2/G4 改为可执行口径 |
| 报告 schema | PASS | 仅向后兼容字段，不加闭合掩码 |
| Fixture | PREPARED | F-03/F-04 由 15D-01 落地 |
| 自动化入口 | PREPARED | 15D-01/02 落地 `run_stage15_white_carrier_gate.ps1` 和 summary schema |
| 实物打样 | NOT STARTED | 只阻断启用，不阻断开发 |

**结论：准备工作已完成，Stage 15 可从 `15A-01` 开始开发。**

当前仓库仍有其它阶段未提交代码，实际开工时先执行 15A-01 的基线隔离动作；在此之前不得生成或声称存在 Stage 15 before golden。

## 8. 验证入口

准备阶段只执行文档与配置校验：

```powershell
Get-Content samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json -Raw | ConvertFrom-Json | Out-Null
Get-Content samples/scenarios/slicer_scenarios.json -Raw | ConvertFrom-Json | Out-Null
git diff --check
```

代码阶段的构建、单测、场景切片、Reader、性能和实物 Gate 以 `TASKS_15` 与 `DEMO_15` 为准。
