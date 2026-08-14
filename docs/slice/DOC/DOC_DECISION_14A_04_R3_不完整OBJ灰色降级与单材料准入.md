# DOC_DECISION_14A_04_R3 不完整 OBJ 灰色降级与单材料准入

> 状态：ACCEPTED / USER AUTHORIZED
> 日期：2026-08-14
> 受控修订对象：Stage 14 ViewData 与模型元数据合同

## 1. 决策背景

部分真实 OBJ 含 UV 与 `usemtl mtl0`，但没有 `mtllib`/MTL 定义；另一些 OBJ 的
MTL 已声明漫反射贴图，但贴图文件缺失。此类资产没有足够信息恢复彩色外观，若继续把
`mtl0` 当作有效材质会导致 ViewData 材质引用无法闭合。

## 2. 决策

满足以下任一条件时，模型进入 `singleMaterialOnly` 降级状态：

1. 实际三角形使用具名材质，但 OBJ 完全没有 `mtllib`/MTL 材质表；
2. 实际使用的已解析材质声明漫反射贴图，但目标贴图文件不存在。

降级后的整个模型在 top 与 three_d 视图中统一使用 sRGB 中性灰
`(0.6, 0.6, 0.6)` 与透明度 `0.55`，`alphaMode=blend`，不得只替换局部子网格。
ViewData 返回 `textureStatus=not_provided` 且不伪造纹理 blob。

`model.import` 与 `model.get_metadata` 同时返回：

```text
appearanceStatus
singleMaterialOnly=true
appearanceDetail
```

参考宿主收到该限制后，只允许选择“单材料白墨”或“单材料光油”，禁用所有 RGB
工艺。场景内只要存在一个受限实例，限制即对整个场景生效；移除全部受限实例后解除。

## 3. 不进入降级的错误

以下错误仍然 fail-closed，不得以灰色模型掩盖：

- OBJ 声明了 MTL，但 `usemtl` 名称无法在已声明材质表中解析；
- 贴图文件存在但解码失败；
- 彩色纹理路径缺少有效 UV/材质绑定；
- 其他资源身份、ViewData 合同或拓扑准入错误。

## 4. 冻结边界

- 保持 `PM_SPI_VERSION=1`、11 个 `pm_*` 导出和 15 项能力不变；
- 仅为既有模型能力响应增加向后兼容字段，不新增能力；
- 不修改 `p0.rgbwsv.2`、RGBWSV 通道顺序、uint8 或 `black_is_print`；
- 不改变完整彩色纹理模型、无纹理单材料模型或生产切片算法；
- 不把缺失资源恢复解释为彩色纹理恢复成功。

## 5. 验收

1. `model/obj/reality` 类 OBJ 可导入并在 top/three_d 中显示半透明灰色；
2. 缺失 MTL 与缺失贴图两类状态可由模型元数据区分；
3. 宿主 RGB 工艺不可选择，白墨与光油单材料工艺可选择；
4. 移除全部受限模型后 RGB 工艺恢复；
5. 完整纹理模型仍显示真实纹理；贴图解码失败仍被拒绝；
6. 合同、ViewData、宿主工作流和真实 Reality 夹具回归通过。
