# DOC_PREP HOSTFLOW H-E E2 材料与纹理实施准备

> 状态：**H-E-04 COMPLETE / H-E-05 READY**
> 日期：2026-08-10
> 范围：E1 批次复核、H-E-04 材料工艺 Profile、H-E-05 生产纹理设置。

## 1. E1 批次复核

H-E-01 与 H-E-03 的 Debug/Release 门禁均通过。H-E-03 建立的宿主 Profile 编辑链路可继续复用：

```text
独立可折叠编辑器
  -> hostslicesettings 结构化草稿
  -> HostEffectiveProfileBuilder
  -> 纯 C HostRequestBuilder
  -> canonical Profile / profileHash
  -> HostWorkspaceState 宿主持久化
```

复核结论为 **E1_GATE=PASS**。E2 不需要新增 SPI、能力或导出，也不得读取内部
`slicer_scenarios.json`。

## 2. H-E-04 材料工艺合同

宿主编辑器覆盖：

- RGB、RGB+W、RGB+V、RGB+W+V、单 W、单 V 六种材料策略；
- 材料角色映射开关、默认角色、白墨/光油名称规则；
- 输入支撑材料准入；
- 白墨扩张/收缩、光油顶层层数、异常重叠阈值；
- 材料设置进入 canonical Profile、自哈希和工作区草稿持久化。

实现保持 `HostRequestBuilder.c` 尺寸门禁：材料 JSON 由独立的
`HostMaterialProfile.c` 生成。UI 编辑仅修改宿主草稿，不调用 DLL。

## 3. H-E-05 准备合同

H-E-05 只补生产纹理 Profile 段，字段必须以当前 core 配置和 Stage 15 冻结语义为准：

1. 纹理应用模式及非表面 RGB 策略；
2. UV 寻址、过滤和坐标约束；
3. 纹理表面宽度/层数与诊断边界；
4. `unprintable_white_*` 仅在已授权的 Legacy 全实体 RGB 场景启用；
5. 纹理缺失、解码失败、UV/材质绑定无效继续 fail-closed。

H-E-05 开发前必须逐项复核 `ProductionTextureSettingsPanel`、Contract、Model、
`slicer_core/config` 和 Stage 15 设计，禁止仅凭 UI 名称猜测字段。

## 4. Gate

```text
E1_GATE = PASS
H_E_04_GATE = PASS
H_E_05_PREPARATION_GATE = PASS
E3_GATE = WAIT_E2_REVIEW
```

E2 完成后必须复核八项 `adapt_to_host_profile` 是否都有宿主归宿，之后才能进入 E3。
