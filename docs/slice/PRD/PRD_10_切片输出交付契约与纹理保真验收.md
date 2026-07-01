# PRD_10_切片输出交付契约与纹理保真验收

> 文档版本：v0.1
> 文档状态：Formal PRD / Stage 10
> 生成日期：2026-07-01
> 阶段定位：Slicing Output Contract / Texture Fidelity Acceptance

---

## 1. 阶段定位

10 阶段不做 RIP、设备通信、喷头 bitstream 或 RIP 半色调。

10 阶段目标是把 SliceSoft 的切片输出定义成稳定、可验收、可交付给下游 RIP 工程团队的契约：

```text
模型切片结果；
纹理 / UV / 材质映射保真信息；
RGBWSV 通道语义；
per-layer summary；
manifest / report；
错误和 fallback 解释；
真实模型验收集。
```

---

## 2. 产品目标

10 阶段需要回答：

```text
1. 输出包中哪些字段是下游必须依赖的稳定契约；
2. 纹理信息是否足够保真，fallback 是否可追踪；
3. RGBWSV 各通道在层级数据中的语义是否可解释；
4. 多材质、白墨、光油、支撑的组合是否有验收规则；
5. 真实模型集合能否形成 release candidate gate；
6. 下游 RIP 工程师需要的 metadata 是否完整；
7. 本项目和 RIP/设备团队的边界是否清楚。
```

---

## 3. 输出契约范围

必须稳定：

```text
package schema；
manifest；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
layer count；
resolution / pixel size；
z / layer height；
per-layer occupancy；
per-channel stats；
material profile summary；
texture source / fallback summary；
diagnostic issue codes；
production admission summary。
```

---

## 4. 纹理保真验收

纹理保真不等于肉眼颜色准确，也不等于 RIP 后最终打印效果。

10 阶段关注：

```text
UV 是否解析；
纹理资源是否找到；
Texture2D / ColorGroup / OBJ MTL 是否可追踪；
fallback 是否记录；
surface shell / full volume 策略是否记录；
纹理 transfer 是否有统计；
每层 RGB 分布是否可比较；
关键模型 golden 是否稳定。
```

---

## 5. 非目标

```text
不实现 RIP 半色调；
不实现 ICC 色彩管理；
不实现设备通信；
不生成喷头 bitstream；
不把下游库并入 slicer_core；
不修改 p0.rgbwsv.2；
不改变 RGBWSV channel order；
不默认启用 OpenVDB。
```

---

## 6. 验收标准

10 阶段完成后必须具备：

```text
1. 输出契约文档；
2. texture fidelity 指标说明；
3. 真实模型验收集清单；
4. golden report / package summary；
5. 下游 handoff checklist；
6. 是否进入 11 阶段 UI layer preview 的明确判断；
7. REPORT_10。
```

