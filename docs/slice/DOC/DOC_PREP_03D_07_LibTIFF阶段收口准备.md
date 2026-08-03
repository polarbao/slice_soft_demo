# DOC_PREP_03D-07 LibTIFF 阶段收口准备

> 状态：CONSUMED / 03D-07 COMPLETE / GO_OPTIONAL
> 日期：2026-08-03
> 前置：03D-06 COMPLETE / GO_OPTIONAL

## 1. 当前判定

03D-06 功能兼容与内存 Gate 通过，但主生产量级 warm stripped p50 改善未达到 15%。
因此 03D-07 只允许执行“可选后端阶段收口”，不允许切换默认 Writer。

## 2. 可执行范围

```text
更新 03D 最终状态、用户构建说明和可选 LibTIFF 使用入口；
保留 slicesoft-default=handwritten；
保留 slicesoft-libtiff 显式 opt-in Runtime；
确认 tiff.dll、许可证、版本和 runtime manifest；
运行完整 regression、Runtime、Package 与 RIP Gate；
记录 GO_OPTIONAL 和未来重测入口。
```

## 3. 禁止范围

```text
不得把 SLICESOFT_TIFF_BACKEND 默认值改为 libtiff；
不得让默认 VS Code/CMake Preset 隐式选择 LibTIFF；
不得删除 handwritten Writer 或回滚入口；
不得实现压缩、BigTIFF、planar separate、多 IFD 或并行写层；
不得改变 p0.rgbwsv.2、RGBWSV、uint8、black_is_print。
```

## 4. 未来默认切换重新准入

只有同时满足以下条件，才可重新打开默认切换：

```text
固定参考机器和正式生产量级 fixture；
03D-05 compatibility 全部 PASS；
主生产 stripped p50 改善 >= 15%，或真实 Package TIFF 总耗时改善 >= 10%；
LibTIFF 峰值工作集 <= handwritten 1.10 倍；
Runtime、Reader、RIP 和部署无回归；
用户对“默认切换”再次明确授权。
```

## 5. 建议验证

```powershell
.\scripts\run_03d_libtiff_writer_matrix.ps1 -Config Release
.\scripts\Run03DTiffCompatibilityGate.ps1 -Config Release -SkipBuild
.\scripts\run_regression.ps1 -Mode full
git diff --check
```

03D-07 的后续代码开发当前不需要改变 Writer 实现；主要工作是状态收口和完整回归。默认切换
保持阻断，除非未来获得新的性能证据与独立授权。

## 6. 执行结果

03D-07 已于 2026-08-03 按本准备文档完成。默认 Writer 保持 handwritten，LibTIFF 作为显式
可选后端保留，结果见 `REPORT_03D_07_LibTIFF可选后端阶段收口.md`。
