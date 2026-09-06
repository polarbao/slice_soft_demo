# DOC_PREP_16C-06-MEMFLOW MF-03B4B 材料最终重放实施准备

> 状态：**PREPARED / IMPLEMENTATION GO（仅非生产候选）**  
> 日期：2026-08-21  
> 上游：MF-03B4A COMPLETE  
> 下游：MF-04 保持 PENDING，不在本卡授权范围

## 1. 准备结论

MF-03B4B 可以进入实现，但只能建立独立的 non-production bounded material replay。生产继续使用
Retained Dense；不得连接 `run_slicer`、Production Service、Profile、SPI、Worker、TIFF、preview、
report writer 或 Qt。

本准备冻结 retained 等价语义、DTO、identity、缓冲所有权、状态机、异常边界、内存 Gate 和独立
oracle。实现必须遵守 `wrap first, move later, rewrite last`：先提取 retained canonical core 并让
`slicer.cpp` 薄包装复用，再由 B4B scanner 复用；不得以 experimental `MaterialChannelComposer`
代替 retained 语义。

## 2. 当前代码事实

语义权威仍是 `src/slicer_core/slicer.cpp` 匿名作用域：

```text
private DTO/statistics       ChannelStats、LayerSemanticStats、TextureColumnColor、
                             ColumnLayerRange、MaterialPixel、MaterialRoleColumn
material helpers             NonSurfaceRgb、model-fill/material-policy/role helpers
closure helpers              InitializeMaterialClosureSemanticInput、
                             PopulateMaterialClosureEmptyMask、repair values
layer composer               compose_layer
final retained loop          closure exact -> repair -> remaining re-detect -> final totals
```

可直接复用的公共语义能力：

```text
TextureWhiteCarrierPolicy::ApplyUnprintableWhiteCarrier
MaterialClosureSemanticDetector
MaterialClosureRepair
SupportType.h
```

现有 closure API 以 owning `std::vector` 为主，并在 analysis/repair 内分配临时 mask。B4B 首版必须
提供 scanner-owned reusable workspace；旧 owning API 保留并委托相同内核，不能破坏现有调用方。

## 3. Retained 等价顺序

冻结处理顺序如下：

```text
1. validate layer identity, layer order, dimensions, binary masks and aliasing
2. initialize semantic input
   supportRequired = finalSupport OR clearedOuterOverlap
   expectedOccupied = model OR supportRequired OR outerVarnishShell
3. compose RGBWSV and semantic ownership
   pixel priority = Model > OuterVarnishShell > Support > Empty
   model branch = MaterialRoleMapping > MaterialPolicy > Texture > fallback
4. apply Stage 15 only at the retained eligible texture branch
5. apply explicit model fill and surface varnish using retained ordering
6. populate final empty mask from all six channels
7. when exact is enabled, analyze closure
8. when repair_then_report is enabled, build/apply repair plan
9. re-detect remaining gaps after repair
10. scan repair-after RGBWSV channel and semantic totals
11. call synchronous sink using private scratch
12. only after sink success copy RGBWSV and semantic masks to caller buffers
13. commit compact totals and advance expected layer
```

`compose -> Stage 15` 是阶段标签，不得把 Stage 15 扩大为对所有材料分支的统一后处理。当前 Stage 15
只在无 role mapping、无 material policy、texture enabled 且当前层应用纹理的 retained 分支生效。
任何被配置验证禁止的组合也不得借本卡获得新行为。

MaterialPolicy/texture/Stage 15 counters 保留 retained 的 compose-time 口径；closure repair 不回补这些
counter。最终 channel totals、model/support semantic totals 和 closure remaining totals必须读取 repair
后的 RGBWSV/semantic masks。该区分是零漂移合同，不在本卡修正历史统计口径。

## 4. 公共 DTO 与所有权

### 4.1 Immutable facts

```cpp
struct BoundedMaterialColumnRangeFact {
    bool hasModel;
    int lowerLayer;
    int upperLayer;
};

struct BoundedTextureColumnFact {
    bool hasColor;
    std::array<std::uint8_t, 3> rgb;
    bool sampledTexture;
    bool usedFallback;
    bool uvOutOfRange;
};

enum class BoundedMaterialRole {
    Rgb, White, Varnish, Ignore, SupportCandidate, Support
};

struct BoundedMaterialRoleColumnFact {
    bool hasRole;
    BoundedMaterialRole role;
    std::array<std::uint8_t, 3> rgb;
};
```

Facts 在 scanner 构造前生成并在 scanner 生命周期内保持不变。B4B 不解码纹理、不访问文件系统，
也不持有 `ModelReport`、Json report 或 Qt 类型。

### 4.2 Policy snapshot

`BoundedMaterialReplayPolicy` 只复制 composer 实际读取的值：background、material、texture apply/
non-surface/top-surface、material policy、model fill、material process profile、material role mapping enabled、
support/surface-varnish channel values，以及 closure enabled/mode/connectivity/maxGap/repair values。

首版允许从已验证的 `SliceConfig` 构造 snapshot，但 scanner 不保存 `SliceConfig` 引用。snapshot 必须是
值对象；不得把配置字符串或 vector 的外部存活期转嫁给 scanner。

### 4.3 Identity

`BoundedMaterialReplayIdentity` 冻结 digestVersion、width/height/layerCount、B4A upstream replay digest、
BaseProjection policy digest、material policy digest、texture facts digest、material-role facts digest 和
column-range facts digest。

Canonical digest domain：固定 ASCII domain tag + digestVersion；整数为 little-endian 固定宽度；bool 为
`u8 0|1`；enum 为显式 `u8`；字符串为 `u32 byteLength + UTF-8 bytes`；数组按索引顺序；浮点配置先
拒绝非有限值，再使用 IEEE-754 bit pattern。digest version 首版为 1。任一 Grid、policy、facts、
layerCount 或 B4A/upstream identity 变化必须使 identity 不等。

### 4.4 Layer input/output

`BoundedMaterialLayerInput` 借用 layerIndex/z、model、B4A final support/type/cleared overlap、outer shell、
outer/inner surface varnish masks。所有 input span 只在 `ConsumeLayer` 调用期间有效。

`BoundedMaterialLayerBuffers` 借用 caller-owned RGBWSV 和以下 11 个 semantic mask：

```text
textureSurface, modelFill, modelMaterial, supportFill, internalVoidSupport,
surfaceVarnish, outerVarnishShell, modelEnvelope, supportRequired,
expectedOccupiedDomain, layerEmpty
```

所有输入 mask、输出 RGBWSV、输出 semantic masks 之间的任意字节区间重叠均拒绝；所有 mask 必须是
pixelCount 字节，RGBWSV 必须是 `pixelCount * 6` 字节。输出只在 sink 成功后一次性提交；此前 caller
buffer 保持逐字节不变。

`BoundedMaterialLayerEvidenceView` 只在同步 sink 回调期间有效，包含 layer identity、closure initial/
repair/remaining、repair 后 channel/semantic totals 和 retained compose-time material/texture/Stage 15
counters。Result 只保留 identity、completedLayerCount 与 O(1) aggregate totals，不保留逐层 Raster。

## 5. Scanner 状态机

```text
Ready(expected layer N)
  -> validate without mutation
  -> compose/analyze/repair into private reusable scratch
  -> synchronous sink
  -> copy to caller buffers
  -> aggregate totals + expected layer N+1

Ready(final layer consumed) -> Finish -> Finished(Result)
Any fatal digest/sink/internal exception -> Failed(stable error)
```

- 尺寸、层序、非二值、别名等可校验输入错误不推进状态，允许同层修正重试。
- identity/digest 不匹配、sink 异常或内部异常发生在 private scratch 上，caller output 不变，scanner
  永久失败；之后 Consume/Finish 均 fail closed。
- 无显式 Cancel API。调用方停止 Consume 并销毁 scanner 即取消；未 Finish 不产生 Result。
- 未完整消费不得 Finish；Finish 只能调用一次；Finish 后不得 Consume。
- `zMm` 必须有限，层号必须严格从 0 连续到 layerCount-1。

## 6. Workspace 与内存 Gate

scanner 构造期一次分配并复用 RGBWSV、11 个 semantic masks、closure analysis masks、closure repair-plan
masks、detector traversal 和 compact per-layer statistics scratch。compose 热路径要求零堆分配。closure
disabled 时不得触碰 exact/repair workspace；closure exact/repair 只可使用构造期按 pixelCount 固定上限
预分配的 workspace，Consume 热路径不得新增 capacity。

测试记录各 scratch 地址/capacity 跨层稳定。Result、totals 和 sink evidence 均为 O(1)；禁止保存
layerCount x pixelCount RGBWSV 或 semantic mask 栈。

旧 `MaterialClosureSemanticDetector` / `MaterialClosureRepair` owning API 保持源兼容，并委托新的 span+
workspace 内核。若无法在不改变现有语义的前提下完成 workspace 内核，本卡必须停止为 NO-GO，不能
以每层隐式分配宣称通过 bounded Gate。

## 7. 文件与依赖边界

预计实现文件：

```text
src/slicer_core/material/RetainedMaterialLayerComposer.h/.cpp
src/slicer_core/material/BoundedMaterialFinalReplay.h/.cpp
src/slicer_core/diagnostics/MaterialClosureSemanticDetector.h/.cpp
src/slicer_core/material/MaterialClosureRepair.h/.cpp
src/slicer_core/slicer.cpp
tests/stage16/BoundedMaterialFinalReplayTests.cpp
tests/unit/material_closure_semantic_detector/main.cpp
CMakeLists.txt
```

依赖方向：

```text
SupportType / B4A stable evidence
  -> material/BoundedMaterialFinalReplay
  -> RetainedMaterialLayerComposer
  -> TextureWhiteCarrierPolicy + closure detector/repair
```

material 不得反向依赖 support 私有 scanner；不得依赖 output、reports、apps 或 Qt。`slicer.cpp` 仅作
旧 DTO/config 与 canonical composer 的薄转换层，不连接 B4B 路由。

## 8. 实现拆分清单

1. `MF-03B4B-1`：提取 public facts/policy/stats DTO、retained canonical composer 和 closure semantic
   初始化/empty-mask/totals helper；`slicer.cpp` 改为薄包装，生产输出零漂移。
2. `MF-03B4B-2`：为 detector/repair 增加预分配 workspace/span 内核，旧 owning API 委托新内核。
3. `MF-03B4B-3`：实现 identity、move-only scanner、caller-owned output、同步 sink、compact Result 和
   fail-closed 生命周期。
4. `MF-03B4B-4`：独立 oracle、组合/错误/内存 Gate、Release 回归及生产零接线审计。
5. `MF-03B4B-5`：同步 TASKS/DEV/PREP/REPORT/AGENTS/project-profile，按验证事实收口。

上述子项属于同一原子卡；任一零漂移或内存 Gate 失败，B4B 不得标记 COMPLETE。

## 9. 独立 Oracle 与验证矩阵

独立 oracle 不得调用新 canonical composer。优先保留 test-owned pre-extraction reference；同时使用
既有 `run_slicer` owned callback 固化真实 fixture 的逐层 RGBWSV/semantic hash，提取后必须零漂移。

最小矩阵覆盖 support off/on、Base/InternalVoid、outer varnish cleared overlap、outer/inner surface
varnish、texture 三种 apply mode、non-surface policy、MaterialPolicy RGB/W/V、MaterialRoleMapping 全
role、ModelFill material/scope、Stage 15 white_underbase、closure disabled/diagnostic/repair_then_report、
connectivity 4/8、too-wide/rejected 和 repair 后 remaining re-detect。

逐层比较 RGBWSV 全字节、11 semantic masks、channel stats、model/support/texture/material-policy/Stage15
counters、closure initial/repair/remaining/rejected-too-wide 及 aggregate totals。错误矩阵覆盖乱序、跳层、
重复层、错误尺寸、非二值、全部别名、identity mismatch、sink throw、提前/重复 Finish、失败后继续、
销毁取消、caller output 不变、caller/scratch 地址复用和热路径零分配。

Release Gate：

```text
/W4 /WX build: B4B and tests
CTest: B4B, B4A, B3, B2, B1, 03A, 02, closure semantic/repair,
       Stage 15 white carrier, outer varnish, support shape
slicer_cli --version
git diff --check
rg production-reference audit: B4B only tests/CMake/docs; no run_slicer/Service/SPI/Worker/UI
artifact audit: no TIFF/preview/report/package output created by B4B tests
```

## 10. 明确不做

```text
不启动 MF-04
不写 TIFF/manifest/report/preview
不切换生产路由或默认 Profile
不修改 p0.rgbwsv.2、RGBWSV 顺序、uint8、black_is_print
不修改 MaterialPolicy/Stage 15/closure 既有产品语义
不引入第三方依赖，不启用 OpenVDB
不把 slicer.cpp 私有 DTO 暴露到 UI
```

