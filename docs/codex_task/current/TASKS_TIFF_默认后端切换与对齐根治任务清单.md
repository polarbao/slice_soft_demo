# TASKS_TIFF 默认后端切换与对齐根治任务清单

> 文档状态：**ACTIVE — T-A-01..03 COMPLETE；T-A-04 外部阻塞；T-A-05A COMPLETE / T-A-05B WAITING**
> 版本：v1.5 ｜ 日期：2026-08-11
> 定位：独立补充专项，不占阶段编号。承接 `03D-LIBTIFF`（COMPLETE / GO_OPTIONAL）与 `03E-02`
> 上游：`DOC_DECISION_03E_TIFF压缩候选与性能Gate.md`、`REPORT_03E_02_*`、打印侧 `CLD_42` §2.1
> 证据等级：A=已核实代码事实，B=目标设计，P=判断

---

## 0. 用户裁决（2026-08-10，两轮）

```text
第一轮
① TIFF 生成【默认采用 libtiff】
② 【启用压缩算法】生成产物
③ TIFF 对齐缺陷若未修复，纳入后续任务处理

第二轮（对 §3 两个待答问题的回复）
④ 【废弃 handwritten】 —— §3.1 选【乙】，T-A-05 由「修对齐」改为「弃用与移除」
⑤ 压缩算法当前只有 PackBits，【后续可添加不同压缩算法】 —— 新增 T-A-06
```

**⑤ 的直接后果**：用户认可「现在只有 PackBits」这一事实，因此
**T-A-04（把默认压缩翻成 PackBits）的外部 RIP 阻塞依然有效**，
不能因为"用户说要压缩"就跳过 RIP 确认 —— 见 §3.2。

## 1. 现状核实（A 级，逐项已看代码）

### 1.1 默认后端已切换为 libtiff

T-A-03 已把 `CMakeLists.txt`、主 CMake Preset、NMake fallback 和 Runtime 部署脚本的
默认 Writer 统一为 `libtiff`。主轨道采用动态 `x64-windows` triplet，并随 Worker/Runtime
分发 `tiff.dll` 与许可证。

```cmake
set(
    SLICESOFT_TIFF_BACKEND
    "libtiff"
    ...
    PROPERTY STRINGS handwritten libtiff
)
```

`handwritten` 仅保留在 `slicesoft-handwritten-legacy` 显式遗留验证轨道中，服务
T-A-05 的观察期与旧字节对照，不再是生产默认。

### 1.2 🔴 对齐缺陷【未修复】，代码里零修补痕迹

| # | 位置 | 现状 |
|---:|---|---|
| 1 | `HandwrittenTiffWriter.cpp:191-192`（tiled）/ `:237`（stripped）| `ifdOffset = kTiffHeaderSize + payload.size()`，**无 padding** |
| 2 | `HandwrittenTiffStructureInternal.h:101-102` `WriteEntry()` | `extraData.insert(extraData.end(), entry.value.begin(), entry.value.end())`，**无对齐补零** |

`HandwrittenTiffWriter.cpp:70,74` 的两个奇数长度 ASCII 字段仍在：

```cpp
{270U, ..., Ascii("RGBWSV")}             // 7 字节（含 NUL），> 4 → 进 extraData
{305U, ..., Ascii("slice_soft_demo p0")} // 19 字节（含 NUL），> 4 → 紧随其后
```

⇒ **打印侧 `CLD_42` §2.1 对 handwritten 的判断成立：该遗留 Writer 的 6 通道输出违反
TIFF 6.0 偶偏移规则，与通道数无关。** 默认 LibTIFF 主轨道已经通过偶偏移门禁；该缺陷
仅在 T-A-05 观察期的显式遗留轨道中保留并 fail-closed 披露。

### 1.3 压缩当前只有 PackBits，且外部互操作未验证

`LibTiffWriter.cpp:267` 与 `HandwrittenTiffStructureInternal.h:135` 均只有
`TiffCompressionMode::PackBits` 一种。**没有 LZW / Deflate。**

`03E-02` 的结论是 `NO_GO_DEFAULT_EXTERNAL_INTEROP_PENDING` / `GO_ON_DEMAND` ——
**阻塞项不在我方代码，而在目标 RIP 与控制软件能否读 PackBits。**

### 1.4 🔴 缺陷位置在 Worker，不在 DLL（A）

`cmake/SliceSoftCoreLayering.cmake` 的分层规则：

```text
basePrefixes      src/slicer_core/{api,importers,layout,model,scene}/
baseExactSources  src/slicer_core/tiff_io.cpp          ← 只有【读取器】进 base
```

`src/slicer_core/output/tiff/HandwrittenTiffWriter.cpp` **不在任何 base 规则内**
→ 落入 `slicer_engine`。而 `CMakeLists.txt`：

```cmake
:615  target_link_libraries(slicer_module PRIVATE slicer_base)      # DLL 只链 base
:1004 target_link_libraries(slicer_worker_slice_runtime PUBLIC ... slicer_engine)
```

⇒ **写入器与对齐缺陷完全在 `slicer_worker.exe` 内，`slicer_module.dll` 里没有 TIFF 写入代码。**
这意味着修它**不触碰 DLL 的 11 个导出与 ABI 面**，只影响 Worker 产物 —— 风险面比预想小得多。

## 2. 任务卡

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **T-A-01** | **补对齐回归测试**（先做，无论后续走哪条路都要）：校验 IFD 偏移与 `extraData` 内每个 `>4` 字节字段偏移的**奇偶性** | 无 | 现有 handwritten 产物**必须 FAIL**（证明测试有效）；`tests/unit/tiff_writer_contract/Main.cpp` 补齐机器断言，并修正便携 fixture 路径 | **COMPLETE / 2026-08-11** |
| **T-A-02** | **libtiff 车道的对齐与压缩实测**：在 `SLICESOFT_TIFF_BACKEND=libtiff` 下产出 stripped/tiled × `none`/`PackBits` 四组，跑 T-A-01 的断言 + 项目内 strict Reader | T-A-01 | libtiff 四组全部**偶偏移 PASS**；strict Reader PASS；给出与 handwritten 的体积/耗时对比 | **COMPLETE / 2026-08-11** |
| **T-A-03** | **默认后端切 libtiff**：改 `CMakeLists.txt:15` 默认值；同步 `runtime_dependencies.json`、`third_party_distribution_manifest.json`、`THIRD_PARTY_NOTICES.txt`、`checksums.sha256` | T-A-02 | 分发包含 libtiff 运行时与许可；`tiff_backend_build_info_unit_tests` 期望值同步；语义 Golden Package 与旧 Writer 做像素/协议等价对照，不冻结跨 Writer 的原始 TIFF 字节哈希 | **COMPLETE / 2026-08-11** |
| **T-A-04** | **压缩默认开启**：`output.tiffCompression.algorithm` 默认由 `none` 改为 `PackBits` | T-A-03；⛔ **外部 RIP 互操作确认** | 目标 RIP 与控制软件均能读；未确认前**不得翻默认值** | **BLOCKED / 需外部证据** |
| **T-A-05** | 🔴 **handwritten 弃用与移除**（用户已裁决 §3.1 = 乙）：分两步 —— ①构建期对 `SLICESOFT_TIFF_BACKEND=handwritten` 发 deprecation 警告并在文档标注弃用；②确认无消费方后移除 `HandwrittenTiffWriter.*` / `HandwrittenTiffStructureInternal.h` / `TiffPackBitsWriteInternal.h` 及其构建分支 | T-A-03 稳定运行一个验收周期 | ⚠️ **只移除写入器**；`tiff_io.cpp` / `TiffReadApi` / `TiffPackBitsReadInternal` 是**读取侧**且在 `slicer_base`（DLL 内），**必须保留**；移除后 `SLICESOFT_TIFF_BACKEND` 选项本身也应一并简化 | **IN_PROGRESS / T-A-05A COMPLETE；T-A-05B WAITING** |
| **T-A-06** | **压缩算法扩展位**：为 `output.tiffCompression.algorithm` 预留 LZW / Deflate(ZIP) 等后续算法的接入点 | T-A-04 | 枚举可扩展且**新增值默认关闭**；每新增一种算法必须各自过 RIP 互操作确认，**不得因 PackBits 已确认就默认放行新算法** | **PROPOSED / 待需求触发** |

### 2.1 T-A-01 / T-A-02 实施证据（2026-08-11）

```text
T-A-01
  handwritten 常规合同：PASS
  handwritten 对齐探针：预期 FAIL，首个命中 tag=273（StripOffsets）奇偏移
  CTest 长期门禁：handwritten 轨道以 WILL_FAIL 冻结已知缺陷；libtiff 轨道以
                  tiff_writer_alignment_conformance_unit_tests 强制偶偏移
  便携配置：obj_mtl_texture_rgb_varnish.json 的相对 modelPath 可解析且文件存在

T-A-02
  LibTIFF Release 定向 CTest：5/5 PASS
  覆盖：合同、对齐、build-info、backend、equivalence
  stripped/tiled × none/PackBits：项目 strict Reader 像素 exact PASS
  性能报告：output/benchmarks/tiff_t_a_02/20260811_115018_205/
              tiff_compression_matrix.json
```

同机 Release Writer-only p50 对比如下；本表只用于迁移风险判断，不改写协议结论：

| 压缩 | Case | handwritten p50 | LibTIFF p50 | LibTIFF 相对变化 | 文件字节差 |
|---|---|---:|---:|---:|---:|
| none | reality_single / stripped | 7.2090 ms | 10.7252 ms | +48.775% | +1 B |
| none | multi_model / stripped | 11.4821 ms | 13.4415 ms | +17.065% | +1 B |
| none | non_integral_tile / tiled | 8.7066 ms | 10.4920 ms | +20.506% | +1 B |
| PackBits | reality_single / stripped | 9.3119 ms | 11.3285 ms | +21.656% | +241 B |
| PackBits | multi_model / stripped | 20.8648 ms | 21.0913 ms | +1.086% | +2307 B |
| PackBits | non_integral_tile / tiled | 9.7516 ms | 12.7965 ms | +31.225% | +283 B |

PackBits 在两种 Writer 上均减少约 `58.740%..66.930%` 文件体积；本轮矩阵总体判定仍为
`NO_GO_DEFAULT`。该性能结论不阻塞 T-A-03 的**默认 Writer**切换，因为 T-A-03 的首要目标
是根治 TIFF 对齐违规；它仍然阻塞在 T-A-04 中把 PackBits 翻成默认压缩。

### 2.2 T-A-03 实施证据（2026-08-11）

```text
默认主轨道
  CMakeCache：SLICESOFT_TIFF_BACKEND=libtiff，VCPKG_TARGET_TRIPLET=x64-windows
  后端查询：configuredBackend=libtiff，LibTIFF 4.7.1，stripped/tiled=true
  Release 定向 CTest：contract/alignment/build-info/backend/equivalence 5/5 PASS

生产语义与 Golden 对照
  material_process_top2_fixture：默认主轨道生成 20 层 p0.rgbwsv.2 Package
  manifest compression=none，RGBWSV/uint8/black_is_print 保持不变
  rip_reader_test --quiet：PASS
  仓库无生产 TIFF 字节哈希 Golden；双 Writer equivalence 继续作为旧/新语义对照

分发
  Runtime staging：tiff.dll、licenses/libtiff.txt、runtime_manifest 中 DLL SHA-256 PASS
  Stage 14 模块包：runtime_dependencies.json 声明 slicer_worker.exe -> tiff.dll
  checksums.sha256 覆盖 tiff.dll、许可证、notice 和第三方分发声明
  TestSlicerModulePackage.ps1：PASS

遗留轨道
  slicesoft-handwritten-legacy：独立 build/vcpkg 根配置成功
  contract + build-info：PASS
  对齐已知失败：WILL_FAIL 门禁按预期 PASS
```

### 2.3 T-A-05A 弃用告警与消费方审计（2026-08-11）

```text
验收周期
  默认 LibTIFF 主轨道的 Release 5/5 TIFF Gate、语义 Package/RIP strict、Runtime staging、
  Stage 14 能力包和遗留轨道 3/3 已完成一次完整验收周期。

T-A-05A
  CMake 选择 SLICESOFT_TIFF_BACKEND=handwritten 时输出 DEPRECATION 告警；
  默认 libtiff 配置不输出该告警；handwritten 仍可用于迁移证据，不得用于生产。

T-A-05B 尚未准入
  TiffWriterFactory 仍把 8x4 等非 16 倍数 tiled 请求回退到 handwritten；
  tiff_writer_backend/equivalence/build_info 测试仍直接消费 handwritten；
  03D/03E 历史脚本仍依赖 handwritten 对照车道；
  删除源文件属于破坏性操作，必须在迁移上述消费方并取得明确删除确认后执行。
```

详细准备与迁移边界见
`docs/slice/DOC/DOC_PREP_TIFF_T_A_05_HandwrittenWriter弃用与移除准备.md`。

## 3. 两个问题的处置

### 3.1 ✅ handwritten 车道 —— 已裁决【乙：废弃】

用户 2026-08-10 裁决：**废弃 handwritten**。因此：

```text
T-A-05 由「修对齐」改为「弃用与移除」
handwritten 的对齐缺陷【不再修】—— 修一个即将删除的实现是纯成本
```

⚠️ **但 T-A-01 的对齐回归测试仍然必须先做**，原因有两个：

```text
① 它是【验证 libtiff 确实没有这个问题】的手段。没有断言，
   "切到 libtiff 就好了"只是假设，不是证据。
② 弃用期间 handwritten 仍可被显式选用，需要一个能说明
   「选它会产生违规产物」的机器证据，而不是只在文档里写一句。
```

🔴 **移除时的边界（易错）**：只移除**写入器**。
`tiff_io.cpp` / `TiffReadApi` / `TiffPackBitsReadInternal` 是**读取侧**，
按 `SliceSoftCoreLayering.cmake` 的 `baseExactSources` / `baseExactStems` 进了 `slicer_base`，
**在 `slicer_module.dll` 内**，被层预览与 strict Reader 使用 —— **删了会直接破坏 DLL 能力面。**

### 3.2 ⚠️ 压缩默认开启的真实阻塞不在我方（用户裁决未解除此项）

`03E-02` 的结论是外部互操作待验证。**T-A-04 若在未确认前翻默认值，
风险是产出目标 RIP 读不了的生产包** —— 这比对齐缺陷严重得多（对齐是 UB，压缩读不了是硬失败）。

**建议拆成两步交付**：

```text
T-A-03 先落地   默认 libtiff + 压缩仍为 none
                → 立即根治对齐 UB，零外部依赖，可马上做
T-A-04 后落地   压缩默认 PackBits
                → 等打印侧 M0-11 / RIP 确认后再翻
```

这样用户要的两件事都能拿到，但**不把"根治对齐"这件确定的事，绑死在"RIP 能不能读压缩"这件不确定的事上**。

## 3.5 开工顺序与当前入口

```text
✅ T-A-01   补对齐回归测试            ← handwritten 已按预期命中奇偏移
✅ T-A-02   libtiff 四组实测          ← 对齐与 strict Reader 全部 PASS
✅ T-A-03   默认后端切 libtiff        ← 主轨道、Runtime、能力包与 RIP strict PASS
🟡 T-A-05A  handwritten 弃用告警       ← COMPLETE
⏳ T-A-05B  handwritten Writer 移除     ← 等待回退/测试/历史脚本迁移与删除确认
  ─────────────────────────────────
⛔ T-A-04   压缩默认 PackBits         ← 等外部目标 RIP/控制软件互操作证据
⏸ T-A-06   扩展 LZW / Deflate        ← 待需求触发
```

当前没有可立即继续的无破坏性 T-A 实现卡：T-A-04 是外部阻塞，T-A-05B 需要先迁移
非标准 tiled 回退、测试和历史脚本并取得删除确认，T-A-06 需要新增算法需求。默认压缩继续
保持 `none`。

**顺带前置**：`samples/configs/material_process/obj_mtl_texture_rgb_varnish.json:22`
的绝对路径应在 T-A-01 一并修掉（原为 `TASKS_CI` 的 `C-A-01`；CI 已暂缓，
但该缺陷与 CI 无关 —— 换机器就失败，且会干扰 T-A-02 的实测）。

## 4. 边界

```text
✅ 不改 p0.rgbwsv.2 协议 / RGBWSV 通道顺序 / uint8 位深 / black_is_print 极性
✅ 不改 PM_SPI_VERSION、11 个 pm_* 导出、15 项能力
✅ 写入器在 slicer_engine（Worker）内，本专项【不触碰 slicer_module.dll 的 ABI 面】
⚠️ 但产物字节会变 → 语义 Golden 必须重固化；不得把不同 Writer 的原始 TIFF 字节哈希当作等价标准；打印侧若已固化包哈希，需同步回签
```

## 5. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-11 | v1.5 | 完成 T-A-05A：默认轨道完整验收周期通过；handwritten 配置新增 CMake DEPRECATION 告警。审计确认 T-A-05B 仍被非标准 tiled 回退、直接测试、历史脚本及破坏性删除确认阻塞。 |
| 2026-08-11 | v1.4 | 完成 T-A-03：默认主轨道切换为 LibTIFF 4.7.1 与动态 x64-windows triplet；主轨道 5/5 TIFF Gate、语义 Package/RIP strict、Runtime staging、能力包依赖/哈希和 handwritten 遗留车道全部 PASS。T-A-04 保持外部阻塞，T-A-05 等待稳定周期，默认压缩保持 none。 |
| 2026-08-11 | v1.3 | 完成 T-A-01 / T-A-02：增加 IFD 与外置字段偶偏移断言；handwritten 机器证据按预期命中 tag 273 奇偏移；LibTIFF stripped/tiled × none/PackBits 对齐、严格 Reader 与 backend/equivalence 5/5 PASS；补录同机 Release 体积/耗时矩阵并把下一张卡推进到 T-A-03。 |
| 2026-08-10 | v1.1 | 回填用户第二轮裁决：**废弃 handwritten**（§3.1 = 乙，T-A-05 由「修对齐」改为「弃用与移除」，并标注**只删写入器、读取侧在 DLL 内必须保留**）；**压缩后续可扩展算法**（新增 T-A-06，要求每种新算法各自过 RIP 确认）。新增 §3.5 开工顺序，并把 `obj_mtl_texture_rgb_varnish.json:22` 的绝对路径修正并入 T-A-01（CI 已暂缓但该缺陷与 CI 无关）。 |
| 2026-08-10 | v1.0 | 首版。核实默认后端仍为 `handwritten`、对齐缺陷两处**均未修复**、压缩只有 PackBits 且外部互操作未验证；核实写入器落在 `slicer_engine`（Worker）而非 DLL，修复不触碰 ABI 面。立 T-A-01..05 五卡，并提出两个待裁决问题（handwritten 车道存废、压缩默认与对齐根治应否解耦） |
