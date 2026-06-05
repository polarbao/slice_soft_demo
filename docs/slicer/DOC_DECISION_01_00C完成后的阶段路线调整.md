# DOC_DECISION_01_00C完成后的阶段路线调整

> 文档版本：v0.1  
> 文档状态：Draft / 阶段路线决策  
> 适用阶段：00C 完成后  
> 建议提交目录：`docs/slicer/`

---

## 1. 当前阶段判断

根据当前项目状态，P0 / 00A / 00B / 00C 已经形成可验证闭环：

```text
closed_mesh_scanline 普通模型路径
relief_heightfield 浮雕模型路径
RGBWSV uint8 TIFF
0 = 打印，255 = 不打印
V 光油模型材料
S 下表面支撑材料
manifest / reports / preview / rip_reader_test
```

00C 已经完成：

```text
slicingMode = relief_heightfield
materialChannel = V / W / RGB / auto
applyMode = solid_volume
relief.fillMode = surface_to_base / intersection_range
relief.baseZMm
relief column sampler
relief_report.json
manifest.slicing
V 光油 + S 支撑联合输出
```

这说明当前项目已经不再只是 P0 Demo 骨架，而是具备了一个可继续演进的“单材料浮雕切片最小内核”。

---

## 2. 关键结论

00C 完成后，不建议立刻进入彩色纹理切片实现。

推荐先进入：

```text
PRD_01 / DEV_01：正式 2.5D / Relief 浮雕切片路线
```

原因：

```text
1. 浮雕模型是当前业务高频模型；
2. 00C 已经证明 relief_heightfield 路线可行；
3. 彩色纹理涉及 UV / MTL / Texture / ColorShell，复杂度更高；
4. 目前更需要把浮雕路线从 Demo 能力固化为产品能力；
5. 在彩色之前，应明确高度场、材料策略、支撑策略、预览和报告结构。
```

---

## 3. 新阶段顺序

原 ROADMAP 中 PRD_01 是彩色纹理，PRD_02 是支撑，PRD_03 是协议。

根据 00C 完成后的实际情况，建议调整为：

```text
PRD_01 / DEV_01 / DEMO_01：
正式 2.5D / Relief 浮雕切片路线

PRD_02 / DEV_02：
支撑生成、孤岛检测与 SupportType 扩展

PRD_03 / DEV_03：
RGBWSV TIFF 协议与 RIP 输入规范固化

PRD_04 / DEV_04：
彩色纹理模型切片

PRD_05 / DEV_05：
3MF 与多材料模型支持

PRD_06 / DEV_06：
Qt 调试 UI 与模型预览

PRD_07 / DEV_07：
OpenVDB / SDF 正式体素内核
```

注意：

```text
彩色纹理从原 PRD_01 后移为 PRD_04。
```

---

## 4. 为什么 PRD_03 协议不先于 PRD_01

RGBWSV 协议已经在 00B / 00C 中形成稳定事实：

```text
uint8
black_is_print
R G B W S V
V = 光油
S = 支撑
manifest.slicing
rip_reader_test
```

因此 PRD_03 可以并行编写，但不应阻塞 PRD_01。

推荐执行方式：

```text
工程实现主线：先推进 PRD_01 浮雕正式路线
文档规范主线：同步补 PRD_03 RGBWSV 协议固化
```

---

## 5. PRD_01 的定位

PRD_01 不再是“彩色纹理”，而应是：

```text
2.5D / Relief 浮雕正式切片路线
```

PRD_01 要从 00C 的 Demo 能力上升为产品能力：

```text
1. 支持 relief_heightfield 作为正式模式；
2. 支持浮雕模型高度场诊断；
3. 支持 V / W / RGB 单材料输出；
4. 支持下表面支撑；
5. 明确 solid_volume / top_surface_only / top_n_layers 的阶段边界；
6. 支持更完整的 relief_report；
7. 建立 relief 专用测试模型集；
8. 明确哪些能力不进入 PRD_01。
```

---

## 6. 当前不建议做的事

当前不建议立即做：

```text
彩色纹理
UV 采样
MTL 材质映射到 RGB/W/V
3MF 多材料
OpenVDB
复杂支撑树
完整 Qt UI
```

这些可以继续保留在后续路线中。

---

## 7. Codex 后续执行原则

Codex 后续必须先阅读：

```text
docs/slicer/REPORT_00_P0_Demo当前实现状态.md
docs/slicer/DOC_DECISION_01_00C完成后的阶段路线调整.md
docs/slicer/ROADMAP_v0.2_00C后续PRD_DEV文档生成计划.md
docs/slicer/PRD_01_2_5D浮雕正式切片路线.md
docs/slicer/DEV_01_relief_heightfield正式切片设计.md
docs/slicer/DEMO_01_2_5D浮雕切片验证方案.md
```

再进入代码实现。

---

## 8. 结论

00C 完成后，项目下一阶段应从“P0 Demo 修补”转入：

```text
正式 2.5D / Relief 浮雕切片产品化
```

而不是直接跳到彩色纹理。
