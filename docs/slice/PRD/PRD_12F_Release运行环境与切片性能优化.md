# PRD_12F Release 运行环境与切片性能优化

> 文档状态：PRD / R0 Implemented / Performance Work Planned
> 日期：2026-07-16

## 1. 产品问题

当前 UI 展示的切片时间来自 Debug CLI，不能代表用户实际可获得的运行性能；开发者还需要在多个构建目录和调试入口之间判断应该使用哪个二进制。与此同时，Release profile 已证明当前纯切片热点集中在支撑生成和逐层材料合成，而不是 OpenVDB 或几何采样。

## 2. 产品目标

```text
G1：提供一键生成、可直接运行的 Debug/Release Qt Runtime；
G2：日常只保留一个 Qt Debug 入口，并新增明确的 Release 运行入口；
G3：UI 与 CLI 必须使用相同构建类型；
G4：性能结论统一使用 Release core-only benchmark；
G5：以可验证方式降低 supportGenerationMs 和 layerComposeMs；
G6：降低 relief 模式的稠密 mask 内存和重复计算；
G7：不改变生产 RGBWSV 语义。
```

## 3. 非目标

```text
不实现安装器、自动更新和生产作业队列；
不实现 RIP 半色调或设备通信；
不把 OpenVDB 设为默认切片引擎；
不在没有 benchmark 的情况下承诺具体加速比例；
不把 preview 保存时间混入纯切片 KPI。
```

## 4. 运行环境需求

| ID | 需求 | 优先级 |
|---|---|---|
| RUNTIME-01 | 一条命令构建 UI、CLI、RIP reader | P0 |
| RUNTIME-02 | 同时支持 Debug、Release，目录不可混用 | P0 |
| RUNTIME-03 | Runtime 包含 Qt DLL、platform plugin 和编译器 runtime | P0 |
| RUNTIME-04 | UI 优先调用同目录 CLI/RIP reader | P0 |
| RUNTIME-05 | 生成可审计 runtime manifest | P1 |
| RUNTIME-06 | OpenVDB 默认关闭且不进入默认 Runtime | P0 |
| RUNTIME-07 | Runtime 更新使用 staging 发布，不留下半成品目录 | P1 |

## 5. VS Code 需求

```text
SliceSoft: Debug Qt UI
  构建并部署 Debug Runtime，然后进入调试器；

SliceSoft: Run Release Qt UI
  构建并部署 Release Runtime，以 noDebug 方式启动；

SliceSoft: Prepare Release Runtime
  只准备 Release 运行目录；

SliceSoft: Run Release Runtime self-test
  验证部署后的 UI 能独立加载 Qt runtime。
```

12C Fresh Qt UI 不再作为第二个日常 launch，历史脚本继续保留。

## 6. 性能 KPI

### 6.1 KPI 口径

```text
modelLoadMs：单独统计；
sliceProcessingMs：grid + mask + texture + support + layer compute；
outputWriteMs：TIFF + preview + report + publish；
totalMs：完整墙钟时间。
```

算法优化验收只使用 Release `sliceProcessingMs` 和对应子阶段字段。

### 6.2 当前参考基线

`meigui_fudiao` 历史 Release profile：

```text
maskSamplingMs      92.076
texturePrepareMs    37.639
supportGenerationMs 2801.870
layerComposeMs      1595.716
推算纯切片          4527.316 ms
```

该数据用于立项，不替代 12F-R1 的当前代码重新测量。

### 6.3 优化目标

| 阶段 | 目标 |
|---|---|
| R1 | 刷新三真实模型 Release baseline，确认当前热点 |
| R2 | 支撑生成至少一个低风险优化，语义统计完全一致 |
| R3 | layer compose 至少一个扫描/分配优化，通道 hash 一致 |
| R4 | relief 稠密 mask candidate 输出 mask diff=0 或明确拒绝 |
| R5 | 形成最终性能矩阵和是否继续并行化的决策 |

任何百分比改善都必须来自实际 benchmark，不在 PRD 中预先承诺。

## 7. 验收标准

```text
1. Debug/Release Runtime 均可执行 UI self-test；
2. Release Runtime 中 UI、CLI、RIP reader 位于同一目录；
3. Qt platform plugin 存在；
4. manifest 明确 config、buildDir、Qt5Dir 和 useOpenVdb=false；
5. VS Code 不再提供两个功能重叠的 Qt Debug launch；
6. 后续性能改动必须保持 grid/modelPixels/supportPixels/channel hash；
7. strict admission 和 RGBWSV 固定协议无回退。
```
