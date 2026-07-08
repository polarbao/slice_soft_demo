# DEV_12B_R1 Legacy 与 Heightfield 优化原型设计

> 文档版本：v0.1
> 文档状态：DEV / Stage 12B-R1
> 生成日期：2026-07-08
> 前置报告：`docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md`

## 1. 阶段目标

12B-R1 的目标不是重写切片引擎，而是在 R0 已建立的 core-only benchmark 契约上，找出 legacy production path 的主要耗时来源，并验证一个低风险优化方向。

本阶段保持以下边界：

```text
1. legacy slicer_cli 仍是 production path；
2. OpenVDB 不作为默认生产引擎；
3. 不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
4. 不改变 12A/12D 材料语义；
5. 所有性能结论必须基于 Release core-only benchmark。
```

## 2. R0 输入结论

R0 已确认：

```text
nai_you_new legacy Release coreComputeMs ≈ 4862.987
aishen_fudiao legacy Release coreComputeMs ≈ 6564.161
meigui_fudiao legacy Release coreComputeMs ≈ 6409.744
OpenVDB Release CLI 缺失；
OpenVDB Debug candidate 可运行但 outputSemanticsComparable=false；
replacementPass=false。
```

因此 R1 优先方向是：

```text
legacy core hotspot profile
legacy 低风险优化
2.5D heightfield fast path 可行性小原型
```

## 3. R1 执行顺序

### 3.1 R1-01 Hotspot Profile

先增强 core-only benchmark 的观测能力，输出 legacy 内部阶段耗时。

落地字段：

```json
{
  "profile": {
    "available": true,
    "profileLevel": "coarse",
    "configLoadMs": 0,
    "modelLoadMs": 0,
    "gridSetupMs": 0,
    "maskSamplingMs": 0,
    "texturePrepareMs": 0,
    "supportGenerationMs": 0,
    "layerComposeMs": 0,
    "reportBuildMs": 0,
    "reportWriteMs": 0,
    "totalMs": 0
  }
}
```

说明：

```text
如果当前代码无法无侵入拆到 geometry/material/support，先输出 coarse profile；
不要为了计时重构 slicer.cpp 大块逻辑；
后续每拆出一个稳定阶段，再升级 profileLevel 或新增兼容字段。
```

### 3.2 R1-02 Release Profile Baseline

使用 R0 的三个真实模型重复跑 Release core-only profile。

输出：

```text
coreComputeMs
profile.modelLoadMs / maskSamplingMs / texturePrepareMs / supportGenerationMs / layerComposeMs
grid
modelPixels
supportPixels
peakWorkingSetBytes
```

### 3.3 R1-03 低风险优化候选选择

只有在 profile 指向明确热点后，才选择优化候选。

候选优先级：

```text
1. support projection cache：如果 supportPixels 与层数导致重复列投影；
2. z-bucket / active triangle filter：如果逐层三角扫描是主要热点；
3. texture sampling cache：如果纹理采样是主要热点；
4. tile/layer parallel：如果单线程层循环成为主要瓶颈。
```

选择规则：

```text
优先选择影响面最小、可开关、可回滚、可用 R0 benchmark 直接比较的优化。
```

### 3.4 R1-04 首个低风险优化原型

根据 R1-02 profile，优先选择 support generation path 中影响面最小的优化点。

已验证优化：

```text
support.shape.enabled=false 时跳过 support mask 深拷贝；
support.shape.enabled=false 时跳过 support shape pipeline；
support.shape.enabled=true 时保留原行为并通过 support_shape_smoke guard。
```

验收：

```text
同一真实模型 before/after 的 grid/modelPixels/supportPixels 不变；
Release coreComputeMs 有可解释下降；
support.shape.enabled=true 的路径仍可运行。
```

### 3.5 R1-05 2.5D Heightfield Fast Path 设计检查

不直接替换 production path，只做 admission 和 mask 差异评估。

原型目标：

```text
1. 判断模型是否近似 2.5D；
2. 生成 topHeight / bottomHeight；
3. 通过 z 与高度范围比较得到 model mask；
4. 与 legacy mask 做统计差异；
5. 输出是否值得进入后续正式实现。
```

## 4. 数据契约扩展

`slicesoft.benchmark.12b.1` 可兼容新增 engine profile 字段：

```json
{
  "engines": [
    {
      "engine": "legacy",
      "profile": {
        "available": true,
        "profileLevel": "coarse",
        "configLoadMs": 0.0,
        "modelLoadMs": 554.044,
        "gridSetupMs": 0.0,
        "maskSamplingMs": 80.436,
        "texturePrepareMs": 42.698,
        "supportGenerationMs": 2555.337,
        "layerComposeMs": 1302.770,
        "reportBuildMs": 64.456,
        "reportWriteMs": 0.001,
        "totalMs": 4701.410
      }
    }
  ]
}
```

规则：

```text
1. profile 字段不参与 RGBWSV package 协议；
2. profile 缺失不影响旧报告读取；
3. 无法稳定拆分的字段可以省略或置 null，不得伪造；
4. profileLevel=coarse/fine，用于区分粗粒度和细粒度计时。
```

## 5. 验证策略

文档/脚本：

```powershell
git diff --check
占位标记扫描：无命中
```

代码观测：

```powershell
cmake --build build --config Release --target slicer_cli
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_r1_profile.json
```

验收：

```text
1. 报告 schema 仍为 slicesoft.benchmark.12b.1；
2. legacy available=true；
3. profile.available=true；
4. coreComputeMs 与 profile.totalMs 语义一致；
5. writeTiff/writePreview 仍为 false；
6. 生产切片输出不受影响。
```

## 6. 阶段退出标准

R1 完成至少需要：

```text
1. R1 profile 字段可用；
2. 3 个真实模型有 Release profile baseline；
3. 至少一个低风险 legacy 优化经过 before/after benchmark；
4. 如果不做优化，必须有明确证据说明瓶颈不适合当前阶段处理；
5. 形成 REPORT_12B_R1。
```
