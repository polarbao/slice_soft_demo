# DOC_DECISION_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划

> 文档版本：v0.1
> 文档状态：DOC_DECISION / Stage 11A
> 生成日期：2026-07-02
> 阶段定位：Stage 12 前置功能开发计划

## 1. 决策结论

在进入 Stage 12 前，新增 Stage 11A：

```text
Stage 11A：OBJ 标准模板与 OpenVDB OBJ 彩色纹理切片生产候选计划
```

Stage 11A 的目标不是直接扩大 Stage 12，而是在 Stage 12 前先完成以下计划和前置实现：

```text
1. 固定 model/obj 为 OBJ 彩色纹理功能性测试模板目录；
2. 建立 legacy OBJ 彩色纹理功能性测试配置；
3. 制定 OpenVDB OBJ 彩色纹理 production candidate 改造路线；
4. 明确 UI 中非 OpenVDB 与 OpenVDB 按钮式切片入口；
5. 定义进入 Stage 12 前必须满足的验收条件。
```

## 2. 标准模板目录

后续 OBJ 彩色纹理功能性测试标准模板目录为：

```text
model/obj
```

当前标准模板文件：

```text
model/obj/MF_aishen_damuzhi_L_tx02.obj
model/obj/MF_aishen_damuzhi_L_tx02.mtl
model/obj/T_aishen_damuzhi_L_tx02.png
```

模板关系：

```text
OBJ -> mtllib MF_aishen_damuzhi_L_tx02.mtl
MTL -> map_Kd T_aishen_damuzhi_L_tx02.png
```

## 3. 为什么必须放在 Stage 12 前

Stage 12 若继续推进更复杂的正式项目能力，必须先回答：

```text
OBJ 彩色纹理真实模型是否能稳定导入；
legacy 与 OpenVDB 两条切片入口是否边界清晰；
OpenVDB 是否具备 production candidate 输出条件；
UI 是否能通过按钮完成非 OpenVDB / OpenVDB 的用户路径；
真实 OBJ 模板的 texture fidelity / topology / RIP package 是否可验收。
```

这些都是 Stage 12 的前置输入，不应在 Stage 12 中临时发现和拆解。

## 4. 当前 OpenVDB 决策

当前 OpenVDB 仍是：

```text
experimental diagnostic / hardening
```

当前不可直接作为正式 OBJ 彩色纹理生产切片路径。

允许：

```text
UI 按钮触发 OpenVDB diagnostic report；
OpenVDB unavailable / topology blocker / admission status 明确展示；
后续新增 OpenVDB production candidate，但必须受 strict admission gate 控制。
```

禁止：

```text
默认启用 OpenVDB；
用 diagnostic_only 输出冒充 production package；
绕过 ProductionAdmissionPolicy 写 RGBWSV TIFF；
改变 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print。
```

## 5. UI 决策

Stage 11A 后，UI 应至少保留两个清晰入口：

```text
导入模型并切片
导入模型并 OpenVDB 诊断
```

当 OpenVDB production candidate 完成后，再新增或升级为：

```text
导入模型并 OpenVDB 候选切片
```

UI 必须显示：

```text
当前引擎：Legacy / OpenVDB Diagnostic / OpenVDB Candidate
productionAdmission.status
productionAllowed
blockerCodes
warningCodes
output package path
report path
```

## 6. 进入 Stage 12 的门槛

Stage 12 不应开始，除非 Stage 11A 至少完成：

```text
model/obj 标准模板已登记；
legacy 标准 OBJ 模板可生成 p0.rgbwsv.2 package；
OpenVDB diagnostic 可针对标准 OBJ 模板输出 report；
OpenVDB production candidate 路线已拆分成可执行任务；
UI 操作手册已覆盖 legacy / OpenVDB 两条入口；
未改变 legacy production path 和 RGBWSV 协议。
```

若 Stage 11A 中实现 OpenVDB candidate，则还必须满足：

```text
strict_closed 通过才允许 production candidate package；
rip_reader_test PASS；
texture fidelity 指标通过；
legacy regression 不退化；
OpenVDB OFF 默认轨道仍通过。
```
