# DOC_PREP_TIFF_T_A_03 默认 LibTIFF 切换准备

> 状态：`PREPARATION_GATE=PASS`
> 日期：2026-08-11
> 任务：`T-A-03`
> 上游：`TASKS_TIFF_默认后端切换与对齐根治任务清单.md`、T-A-01/02 机器证据

## 1. 目标与冻结边界

本卡只把**默认 TIFF Writer 构建后端**由 `handwritten` 切换为 `libtiff`，以根治生产
TIFF 的 IFD/外置字段奇偏移。以下内容保持不变：

- `p0.rgbwsv.2`、通道顺序 `R/G/B/W/S/V`；
- `uint8`、`black_is_print`、`0=打印 / 255=不打印`；
- stripped/tiled 生产语义和项目 strict Reader；
- 默认压缩仍为 `none`，PackBits 不在本卡翻默认；
- SPI v1、11 个导出、15 项能力和 Worker 文件合同。

## 2. 已满足的准入证据

| 准入项 | 证据 | 结论 |
|---|---|---|
| 对齐回归有效 | handwritten 对齐探针命中 tag 273 奇偏移 | PASS |
| LibTIFF 对齐 | `tiff_writer_alignment_conformance_unit_tests` | PASS |
| 四组合语义 | stripped/tiled × none/PackBits，strict Reader exact | PASS |
| 后端与等价性 | contract/build-info/backend/equivalence 5/5 | PASS |
| 依赖可获得 | vcpkg `tiff:x64-windows@4.7.1` 已构建 | PASS |
| 再分发文本 | `THIRD_PARTY_NOTICES.txt`、`licenses/libtiff.txt` 已存在 | PASS |
| Runtime 部署能力 | `PrepareSliceSoftRuntime.ps1` 已支持复制 DLL、许可和哈希 | PASS |

## 3. 受控修改清单

### 3.1 构建默认值

1. `CMakeLists.txt`：`SLICESOFT_TIFF_BACKEND` 默认改为 `libtiff`。
2. `CMakePresets.json`：主轨道改为 `libtiff + x64-windows` 动态依赖 triplet。
3. NMake fallback 同步使用 `libtiff + x64-windows`，避免同名“默认”入口行为分裂。
4. 保留一条显式 `handwritten` 遗留验证 preset，服务 T-A-05 的弃用观察期；不得再称其为生产默认。
5. `PrepareSliceSoftRuntime.ps1` 默认参数改为 `libtiff`。

主轨道不能继续使用 `x64-windows-static-md`：当前 CMake 的 LibTIFF Runtime 合同要求从
vcpkg `bin` 解析并部署 `tiff.dll`，静态 triplet 不满足该合同。

### 3.2 分发与许可

项目有两条分发清单路径，不能混为同一脚本产物：

- Qt Runtime：`PrepareSliceSoftRuntime.ps1` 生成 `runtime_manifest.json`，其中记录
  `tiff.dll` SHA-256 和 `licenses/libtiff.txt`；
- Stage 14 能力包：`PackageSlicerModule.ps1` 生成 `runtime_dependencies.json` 与
  `checksums.sha256`，并把 Worker 对 `tiff.dll` 的 app-local 依赖写入清单。

因此本卡不手工写死运行时哈希，只更新静态分发声明中的 `currentUsage` 和 notice 文案，随后
分别通过 Qt Runtime staging 与能力包 Gate 重新生成并校验实际清单。

### 3.3 Golden 处置

仓库当前没有提交生产 TIFF 原始字节文件，也没有启用中的固定 TIFF SHA-256 基线；已有
Golden 主要冻结协议字段、报告投影和像素语义。切换后采用以下方式处置：

1. 保留 handwritten 显式车道，生成旧后端对照；
2. 默认 LibTIFF 车道运行 semantic golden、RIP strict 和 TIFF 合同；
3. 对照 Writer 允许字节布局和哈希不同，但解码像素、tag 合同、通道统计必须一致；
4. 不伪造“旧 Golden 文件已提交”的证据。

## 4. 兼容与回滚

- T-A-03 完成后，`handwritten` 只作为遗留显式选项存在；
- 非 16 对齐 tiled 输入目前仍会在 Factory 内回退 handwritten，这是 T-A-05 删除前必须
  清点并迁移的兼容债务，T-A-03 不静默改变该行为；
- 回滚只能显式选择遗留 preset/参数，不得把主 preset 改回 handwritten；
- 默认压缩保持 `none`，因此不引入新的外部 RIP 压缩兼容风险。

## 5. 验证矩阵

```text
1. 默认 preset/configure：backend=libtiff，triplet=x64-windows
2. 默认 Release：contract + alignment + backend + equivalence + strict Reader
3. 遗留 handwritten：常规合同 PASS，对齐已知失败门禁 PASS(WILL_FAIL)
4. Qt Runtime staging：tiff.dll、libtiff 许可、runtime_manifest 内 SHA-256 全部存在
5. Stage 14 能力包：runtime_dependencies、checksums、tiff.dll 与许可全部存在
6. Source guard、JSON 解析、git diff --check
```

## 6. 准备结论

`T-A-03` 的代码、依赖、分发、Golden 和回滚边界已经明确，准备门为 **PASS**。可以进入
开发；但本结论不授权 T-A-04 默认 PackBits，也不提前执行 T-A-05 Writer 删除。
