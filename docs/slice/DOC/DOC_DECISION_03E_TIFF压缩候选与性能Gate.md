# DOC_DECISION_03E TIFF 压缩候选与性能 Gate

> 状态：INTERNAL PRODUCTION CONTRACT COMPLETE / EXTERNAL RIP PENDING / PRODUCTION DEFAULT UNCHANGED
> 日期：2026-08-03  
> 前置：03D-01..07 `GO_OPTIONAL`，默认 Writer 仍为 handwritten

## 1. 问题

当前 RGBWSV TIFF 的 `Compression(259)` 为 `1`，即未压缩。六通道图层包含大量
`255=不打印` 的连续空白，理论上压缩可减少文件体积和读取 IO；但生产写包位于切片关键路径，
不能只因文件变小就直接切换默认格式。

本专项先回答三个问题：

1. 手写 Writer 与 LibTIFF Writer 能否生成像素等价的压缩 TIFF；
2. 压缩对文件体积、写入耗时和严格 Reader 读取耗时分别有什么影响；
3. 是否具备进入生产配置和 RIP 兼容验证的条件。

## 2. 候选比较

| 候选 | 优点 | CMake / vcpkg | 许可证与部署 | 主要风险 | 本阶段结论 |
|---|---|---|---|---|---|
| PackBits | TIFF 标准内建；适合大量 255 空白；实现与诊断简单 | LibTIFF 无需新增 feature；手写 Writer/Reader 可本地实现 | 不新增 DLL 和许可证；沿用 LibTIFF 现有许可证 | 纹理噪声高时压缩率下降，写入增加 CPU | 首选原型 |
| Deflate | 通常压缩率更高，对复杂纹理更稳定 | LibTIFF 需启用 `tiff[zip]`，引入 zlib；手写端可复用 miniz | vcpkg 动态 triplet 可能新增 zlib Runtime；需同步许可证与 Runtime manifest | CPU 成本、依赖和部署面更大 | 暂不接入，待 PackBits Gate 后评估 |

不采用 JPEG：RGBWSV 是材料生产数据，必须逐字节无损；JPEG 有损，不符合协议。

## 3. 固定合同

压缩只允许改变 TIFF payload 编码，不允许改变：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
planarConfig = contiguous
polarity = black_is_print
printValue = 0
emptyValue = 255
storageMode = stripped / tiled
```

`Compression(259)` 的候选值为：

```text
1     = none
32773 = PackBits
```

## 4. 分阶段实施边界

### 4.1 03E-01 原型边界

- `TiffImageSpec` 增加 `None/PackBits`，默认保持 `None`；
- handwritten 与 LibTIFF Writer 支持 PackBits stripped/tiled；
- 项目严格 Reader 支持并严格校验 PackBits；
- benchmark 增加同 buffer 的写入、读取和文件体积测量；
- 03E-01 不增加 UI 和生产 Profile 开关，不修改 manifest schema，不切换生产默认压缩；
- 当前不存在 LibTIFF Reader backend。读取对比统一使用项目严格 Reader，以隔离 Writer 输出差异。

### 4.2 03E-02 已授权生产配置边界

03E-02 在不提升 schema 版本的前提下增加以下显式合同：

```json
{
  "output": {
    "tiffCompression": {
      "algorithm": "none"
    }
  }
}
```

- 可选值仅为 `none`、`packbits`；缺省配置继续解析为 `none`；
- 新写 package 必须在 `manifest.tiff.compression` 显式记录实际算法；
- 历史 `p0.rgbwsv.2` manifest 缺少该字段时按 `none` 兼容读取；
- manifest 声明与 TIFF `Compression(259)` 不一致时返回
  `E_TIFF_COMPRESSION_MISMATCH`；未知算法返回 `E_TIFF_COMPRESSION_INVALID`；
- Qt 配置页允许显式选择 PackBits，但标记为实验选项；生产默认仍为 `none`；
- PackBits 没有压缩等级，不增加 `compressionLevel` 或压缩比例参数。

## 5. Gate

### 5.1 必须通过

```text
handwritten PackBits stripped/tiled exact decode
LibTIFF PackBits stripped/tiled exact decode
双 Writer decoded pixels / checksum / stats 等价
旧 none 合同与 03D 回归不变
```

### 5.2 性能判定

使用 Release、同 buffer、同机器、同目录策略：

```text
文件缩减最小值 >= 15%
严格 Reader p50 最小改善 >= 15%
记录 Writer p50 增幅，不因读性能改善而忽略写入退化
```

通过以上条件只得到 `GO_OPTIONAL_EXPERIMENTAL`。要进入生产 Profile，还必须新增：

1. 完整 Package / RIP strict 压缩兼容矩阵；
2. Photoshop/目标 RIP/打印控制软件的真实互操作验证；
3. 配置 schema、manifest 压缩声明和坏包错误码；
4. 用户明确授权默认策略变更。

## 6. 03E-02 内部 Gate 结果

证据：`output/benchmarks/03e_02/20260803_131354_875/production_compression_gate.json`。

| 用例 | Writer | TIFF 体积缩减 | 完整写包 wall 变化 |
|---|---|---:|---:|
| deterministic_small | handwritten | 0.643% | 快 5.952% |
| deterministic_small | LibTIFF | 0.643% | 快 1.059% |
| real_meigui_04 | handwritten | 60.879% | 快 29.083% |
| real_meigui_04 | LibTIFF | 60.879% | 慢 27.334% |

真实 `meigui/04.obj` 的 none/PackBits、handwritten/LibTIFF 四个 package 均通过项目严格 RIP，
TIFF 解码和固定 RGBWSV 合同保持一致。该轮每 case 只有一次完整流程 wall 测量，用于观察端到端量级，
不替代 03E-01 的多次 Writer/Reader p50 矩阵。

项目内部 Gate 不能代替 Photoshop、目标 RIP 或打印控制软件的真实互操作验证，因此当前决策为：

```text
NO_GO_DEFAULT_EXTERNAL_INTEROP_PENDING
```

## 7. 决策

PackBits 已进入显式可选的生产配置、manifest、严格 Reader 和 Qt 高级输出设置；生产默认继续使用
未压缩 TIFF。Deflate 不在本次实现范围。只有目标 RIP 互操作通过并获得新的明确授权后，才允许重新
讨论默认压缩策略。
