# ROADMAP_11A_Stage12前置OpenVDB_OBJ彩色纹理切片计划

> 文档版本：v0.1
> 文档状态：ROADMAP / Stage 11A
> 生成日期：2026-07-02

## 1. 阶段位置

Stage 11A 插入在：

```text
Stage 11 UI 切片层预览、交互配置与多模型能力
Stage 12 后续正式阶段
```

之间。

Stage 12 开始前，必须先完成 Stage 11A 的计划闭环。

## 2. 目标

```text
用 model/obj 作为真实 OBJ 彩色纹理标准模板；
固化 legacy 一键切片测试；
明确 OpenVDB 当前不是 production path；
拆出 OpenVDB OBJ 彩色纹理 production candidate 改造任务；
让 UI 具备按钮式 legacy / OpenVDB diagnostic / future candidate 操作路径。
```

## 3. 里程碑

### Milestone 11A-M1：标准模板登记

输出：

```text
model/obj/README.md
samples/configs/obj_standard/standard_obj_texture_legacy.json
samples/scenarios/slicer_scenarios.json 中新增标准模板场景
```

完成标准：

```text
inspect-model 可读取标准 OBJ；
文档明确该目录为 OBJ 功能性测试模板。
```

### Milestone 11A-M2：UI 与 legacy 测试闭环

输出：

```text
UI 一键导入模型并切片；
UI 操作手册；
legacy package + RIP summary。
```

完成标准：

```text
model/obj 标准模板可生成 p0.rgbwsv.2 package；
UI 能加载输出包和层预览；
RIP reader PASS。
```

### Milestone 11A-M3：OpenVDB diagnostic 闭环

输出：

```text
UI OpenVDB diagnostic 按钮；
experimental report；
OpenVDB 能力评估文档。
```

完成标准：

```text
diagnostic report 明确 productionPackageWritten=false；
UI 能读取 report；
文档明确当前不可 production。
```

### Milestone 11A-M4：OpenVDB production candidate 设计冻结

输出：

```text
DEV_11A_OpenVDB_OBJ彩色纹理切片改造计划.md；
TASKS_11A_OpenVDB_OBJ彩色纹理切片前置任务清单.md；
candidate 验收命令占位。
```

完成标准：

```text
配置、pipeline、texture transfer、composer、writer、UI、验证任务拆分清晰；
可以进入后续实现迭代。
```

### Milestone 11A-M5：OpenVDB production candidate 实现（可选但建议先于 Stage 12）

输出：

```text
surface_shell_from_sdf pipeline；
OBJ texture transfer；
candidate RGBWSV package；
RIP/golden tests。
```

完成标准：

```text
strict_closed PASS 才写 package；
rip_reader_test PASS；
texture fidelity PASS；
legacy regression PASS。
```

## 4. Stage 12 准入

Stage 12 准入最低要求：

```text
M1-M4 完成；
M5 若未完成，必须明确作为 Stage 12 前置遗留风险，不得在 Stage 12 中默认使用 OpenVDB production path。
```

推荐要求：

```text
M1-M5 全部完成后再进入 Stage 12。
```
