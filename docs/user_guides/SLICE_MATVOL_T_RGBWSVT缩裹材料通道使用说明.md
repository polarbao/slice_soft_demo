# SliceSoft RGBWSVT 缩裹材料 T 通道使用说明

> 适用版本：MATVOL-T T-09  
> 状态：显式生产 opt-in  
> 日期：2026-08-26

## 1. 使用边界

RGBWSVT 是独立新版协议，不替换旧 RGBWSV 默认流程。需要 T 通道时，在参考 Host 中显式选择
`host-reference-transfer-channel`，再选择部署目录中的 `_rgbwsvt` 新版工艺。未选择该 Profile 时，
软件继续使用旧 `p0.rgbwsv.2` 六通道协议。

生产准入只适用于 Host/Worker 的单实例 Scene 路径。直接运行 `slicer_cli` 生成的 RGBWSVT 包标记为
`rgbwsvt_candidate_unvalidated`，不能当作已准入生产包。

## 2. 工艺与材质

- 材质 `01` 是甲片，材质 `02` 是缩裹区域。
- 软件不按材质名或文件名识别缩裹区域；匹配颜色来自所选新版工艺 JSON 的
  `transferChannelPolicy.materialDiffuseRgbValues`。
- 甲片继续使用对应工艺的彩色 RGB、白墨 W 或光油 V 语义；缩裹像素只写 T，和前六通道互斥。
- 模型没有匹配缩裹区域时，T 全空，甲片按所选新版工艺正常切片。

需要调整材质 Kd/RGB 时，应新增或修改新版 `_rgbwsvt` 工艺文件，不在 C++ 软件中固化颜色。旧工艺
文件必须保留，不应原地改成七通道。

## 3. 输出合同

```text
Package schema     p0.rgbwsvt.1
Channel order      R G B W S V T
Bit depth          uint8
Polarity           black_is_print
Production status  productionAcceptance=admitted
```

Host/Worker 在切片完成后会再次严格校验 Package。协议错配、缺少 T、准入状态缺失或未知、统计不一致、
多材质同时匹配、缩裹拓扑不合格时均直接失败，不会回退旧协议。

## 4. 当前模型范围

- `03.obj`：当前仓库内显式生产正例，材质 02 可形成闭合 T 体积。
- `08/09.obj`：颜色可识别，但当前材质 02 的开放拓扑不满足体积 Gate，软件会稳定拒绝且不产包。
- 本专项不修复 `08/09.obj`，也不把其拒绝结果计为生产正例。

## 5. 验收检查

切片完成后，应在 Package Review 中确认：协议为 `p0.rgbwsvt.1`、通道数为 7、顺序以 T 结尾、
`productionAcceptance` 为 `admitted`，且缩裹模型的 T 打印像素非零。设备和实物打印效果仍应按现场
验收流程确认；仓库内通过不等于物理打印或现场 SLA 已通过。

## 6. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-26 | v1.0 | 建立 RGBWSVT 显式生产 opt-in、工艺配置、输出合同和模型边界说明。 |
