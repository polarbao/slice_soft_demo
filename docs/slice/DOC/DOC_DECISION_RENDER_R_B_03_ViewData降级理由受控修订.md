# DOC DECISION R-B-03 ViewData 降级理由受控修订

> 状态：**ACCEPTED / USER AUTHORIZED**
> 日期：2026-08-10
> 范围：`scene.get_viewdata.truncationReason`；不增加字段、不修改 C ABI

## 1. 背景

R-B-02 已删除生产 ViewData 的三角跳采样，并改用 meshoptimizer 安全简化。原字符串
`mesh_lod_reduced_for_max_bytes` 无法告诉宿主几何是安全简化还是历史抽稀，也会在小网格根本没有减少
三角数时误报几何降级。

## 2. 决策

合同版本升至 v1.9，字段形状不变，只冻结字符串语义：

```text
安全简化：mesh_simplified_lod1_for_max_bytes
          mesh_simplified_lod2_for_max_bytes
历史抽稀：mesh_decimated_lod1_for_max_bytes
          mesh_decimated_lod2_for_max_bytes
```

当前 Provider 只可产生 `mesh_simplified_*`。`mesh_decimated_*` 是兼容诊断保留字，用于宿主识别旧模块
或未来显式实验实现；当前代码重新产生该理由应视为回归。多个原因继续以 `;` 连接。

## 3. 运行规则

1. `lod=auto` 且输出三角数实际小于源三角数时，返回对应 `mesh_simplified_lodN_for_max_bytes`；
2. 显式请求 lod1/lod2 不算“未按请求返回”，不设置 `truncated`；
3. 仅降低纹理分辨率时，只返回 `texture_resolution_reduced_for_max_bytes`；
4. 无法安全简化时沿用 R-B-02 fail-closed，不返回 `mesh_decimated_*` 兜底；
5. 字符串参与 `viewdataIdentity`，同一输入和预算必须确定性一致。

## 4. 兼容性

- `PM_SPI_VERSION=1`、11 个导出、15 项能力不变；
- `truncated: boolean` 与 `truncationReason: string|null` 字段不变；
- 旧宿主把新值作为普通字符串显示仍可工作；新宿主可以按前缀区分提示等级；
- 不改变 mesh、纹理 blob、RGBWSV TIFF 或生产切片。

## 5. 验收

- 真实发生简化的 auto fixture 返回 `mesh_simplified_lod2_for_max_bytes`；
- 小网格仅降低纹理时不得包含 `mesh_simplified_*` 或 `mesh_decimated_*`；
- 机器合同同时登记两组不同保留字，并声明当前 Provider 不产生 decimated；
- Debug/Release ViewData 与合同门禁通过。
