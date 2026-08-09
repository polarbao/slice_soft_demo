# DOC_PREP R-A-02 真实资产重测实施准备

> 状态：**PREPARATION_GATE=PASS**
> 日期：2026-08-10
> 前置：RB-P1、R-B-00、R-B-05 COMPLETE

## 1. 测量范围

沿用 R-A-01 的冻结资产根 `model/obj/**/*.obj`。当前仓库仍恰好包含 36 个 OBJ，
不得以新增样例替换失败资产，也不得把纹理合同错误计为预算错误。

本卡同时测量两种场景：

1. 逐资产单实例：确认是否能以 `lod0` 闭合，记录实际上传顶点、三角、mesh 字节和纹理字节；
2. 真实聚合场景：把逐资产可闭合的模型一次性导入，使用宿主默认 128 MiB `maxBytes`，
   记录最终实际 LOD 和去重后的上传字节。

## 2. 统计口径

测试后端从冻结的 `MeshDesc` 计算实际上传量：

```text
meshBytes = vertexCount * (position3 + normal3 + uv2) * sizeof(float)
          + triangleCount * 3 * sizeof(uint32)
```

纹理按 `width * height * 4` 统计。测试只记录上传描述符，不执行像素光栅化，因此不会把
CPU 渲染时间混入 ViewData 预算结论。生成证据：

- `render_ra02_real_asset_matrix.csv`
- `render_ra02_aggregate_scene.txt`

## 3. 判定规则

- `lod0`：完整几何闭合；
- `lod1/lod2`：预算触发了当前破坏性跳采样，必须记为质量降级；
- `PM-SLICER-VIEWDATA-BUDGET`：预算拒绝；
- `PM-SLICER-INPUT-0001/0002`：资产合同拒绝，不能伪装成预算问题；
- 理论字节阈值只作解释，正式结论以实际 provider 返回 LOD 为准。

## 4. 准备结论

资产集合、预算、测量边界、证据格式和错误分类均已冻结，R-A-02 可执行。该卡只负责重测
和裁决输入；不得在同一提交中引入 `meshoptimizer` 或修改 LOD 算法。
