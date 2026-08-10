# DOC PREP R-B-04 半精度网格传输实施准备

> 状态：**PREPARATION_GATE=PASS / IMPLEMENTED**
> 日期：2026-08-10
> 前置：R-B-02、R-B-03 COMPLETE

## 1. 算术复核

无共享三角的原始 wire 口径为 108 B/三角：position 36、normal 36、UV 24、index 12。
三类属性全部 half 后为 60 B/三角，缩减 44.4%。在 32 MiB 总预算与 22 实例均分下：

```text
32 MiB / 22 = 1,525,201 B/实例
25,000 × 60 B = 1,500,000 B
```

因此 25k 阈值可闭合。仅量化 position/normal 时为 72 B/三角，无法达到 40% 缩减与 25k
双重验收，必须按 R-B-04 受控决策补充 UV half。

## 2. 文件边界

| 模块 | 改动 |
|---|---|
| core DTO/budget/identity | 增加 attribute format；预算按 wire scalar；identity 纳入格式 |
| core quantizer | 封装 `meshopt_quantizeHalf`，第三方类型不越过接口 |
| module adapter | 按请求编码 float32 或 float16 blob |
| Qt 参考宿主 | 显式请求 float16；兼容解码 float32/float16 |
| contracts/tests | DTO v1.10、长度/格式/体积/渲染门禁 |

不修改 `apps/slicer_debug_ui/**`，不改变 worker、生产 TIFF 或渲染后端公共接口。

## 3. 验证矩阵

1. 合同脚本验证 v1.10、新请求字段、响应 format 枚举和缺省兼容；
2. core unit 验证 meshoptimizer half 位模式、≥40% 与 25k 阈值；
3. 参考宿主 three_d 测试同时请求缺省 float32 和 float16，确认 identity 隔离；
4. float16 blob 经宿主解码后完成真实纹理渲染；
5. Debug/Release 运行同一组 contract/provider/three_d/source-size 门禁；
6. `git diff --check` 与第三方 NOTICE/能力包回归。

## 4. 准备结论

合同兼容路径、算术、文件所有权、失败语义和测试入口均已闭合，R-B-04 可实施。用户已明确授权
R-B-02 引入 meshoptimizer；R-B-04 复用同一已锁定的 1.1/MIT/static-md 依赖，不新增第三方库。
