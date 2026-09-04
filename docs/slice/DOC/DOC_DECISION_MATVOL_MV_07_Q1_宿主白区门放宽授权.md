# DOC_DECISION_MATVOL MV07-Q1 宿主白区门放宽授权范围

> 文档状态：**ACTIVE / AUTHORIZED**
> 版本：v1.0 ｜ 日期：2026-08-21
> 授权：用户 2026-08-21 明确「可以放宽授权」，并要求先标明授权范围、受影响工艺与模型、后续变化
> 上游：`DOC_PREP_MATVOL_MV_07_宿主接入实施准备.md` MV07-Q1
> 任务真源：`../../codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`

---

## 1. 授权做什么（精确到条件）

只授权**三处新增条件**与**一个新增文件**，使参考宿主能够表达
「多材质纵深 RGB + 按需补白」这一新组合。

### 1.1 `apps/slicer_ui_host_sim/HostSliceSettings.cpp:297-310` 按需补白组合门

```text
放宽方式  给既有拒绝条件补一个 && !settings.materialvolume.enabled，
          使该拒绝在 MATVOL 未启用时【逐字逐句不变】；
          另新增一条只在 MATVOL 启用时生效的拒绝，覆盖 MATVOL 自身的互斥要求。
不改动    既有错误文案「按需补白只支持 Legacy 全实体 RGB 纹理、RGB 实体材料且禁用材料角色映射。」
```

### 1.2 `apps/slicer_host_sim/HostMaterialProfile.c:210-215` 片段路由

```text
放宽方式  在既有 white-carrier 短路【之前】增加一个分支：
          materialvolumeenabled != 0 时改由新文件的片段构造器产出。
不改动    BuildWhiteCarrierFragments（:73-180）与 HostBuildMaterialProfileFragments
          的既有模板一行都不动 —— 这是「旧预设字节不变」的结构性保证，
          而不是靠人工比对。
```

### 1.3 `apps/slicer_host_sim/HostRequestBuilder.c:355-362` slicingMode 推导

```text
放宽方式  useReliefHeightfield 增加 || settings->materialvolumeenabled != 0。
理由      MATVOL 复用 relief 列采样求交，本身就属于 relief_heightfield 路径；
          若不加，宿主会发出 closed_mesh_scanline 而 Worker 必然拒绝。
```

### 1.4 新增文件（不改既有文件即可承载全部新语义）

```text
apps/slicer_host_sim/HostVolumetricProfile.h / .c
  产出含 materialVolumePolicy 的完整材质片段（canonical 与 compact 成对）。
  该目录不在 SourceSizeGuard 的 protectedPrefixes 内，新文件受 ≤500 / ≤200 行约束。
```

## 2. 授权【不】包括什么

```text
⛔ 不放宽 Global 管线、materialPolicy、旧 materialRoleMapping、
   whiteValue 与 emptyValue 冲突、OpenVDB 这五条既有白区禁令中的任何一条
⛔ 不改 p0.rgbwsv.2、RGBWSV 通道顺序、uint8 位深、black_is_print 极性
⛔ 不改 PM_SPI_VERSION、11 个 pm_* 导出、15 项能力
⛔ 不改任何既有工艺预设的字段、显示名或描述
⛔ 不改 ValidateQtHostBoundary 与 ValidateSourceSizeGuard 的任何阈值或白名单
⛔ 不把新语义接进默认路由：MATVOL 预设必须显式选择才生效
⛔ 不引入宿主对 slicer_core / slicer_base / slicer_engine 的任何引用（含注释）
```

## 3. 哪些工艺会受影响

### 3.1 既有六类工艺：**全部不受影响**，且这是结构性保证

| # | 预设 id | 是否受影响 |
|---|---|---|
| 1 | `textured_nail_rgb_only_lower_support` | 不受影响 |
| 2 | `textured_nail_rgb_white_lower_support` | 不受影响 |
| 3 | `textured_nail_rgb_white_ondemand_lower_support`（当前默认） | 不受影响 |
| 4 | `textured_nail_rgb_varnish_lower_support` | 不受影响 |
| 5 | `single_material_relief_white` | 不受影响 |
| 6 | `single_material_relief_varnish` | 不受影响 |

**保证机制（三重）：**

```text
① 六类预设的 materialvolume.enabled 均为 false（新字段默认关闭），
   §1.1 的新增条件与 §1.3 的新增析取项对它们恒为假，判定结果不变；
② §1.2 的路由分支同样不命中，它们继续走【一行未改】的既有片段构造器，
   canonical 串与 compact 串按代码路径同一性保证字节不变；
③ profileHash 因此不变 —— 由 VerifyPresetProfileHashClosure 对每个预设
   用 Worker 同算法重算并比对，新预设自动纳入该门禁。
```

### 3.2 新增第 7 类工艺：仅它启用新语义

```text
新预设   多材质纵深 RGB + 按需补白（候选）
生效条件 用户在工艺下拉里【显式选择】该预设
默认值   工艺默认仍为第 3 类 textured_nail_rgb_white_ondemand_lower_support，不变
```

## 4. 哪些模型会受影响

```text
默认情况下：没有任何模型受影响。
新语义只在【显式选中新预设】时进入 Profile，未选中时 Profile 里不出现
materialVolumePolicy 块（条件产出）。

触发资产：model/obj/reality/finger_suoguo/03.obj
  材质 01 = 绿色 [63,190,126]，开放表面（1382 开放边）
  材质 02 = 浅桃色 [255,220,198]，闭合子网格（0 开放边）
  ⚠️ 该资产的材质 01 为开放面，在 MV-04 的 surface_band 候选获批前，
     选中新预设切它会在【构建期 fail closed】并报
     E_MATVOL_OPEN_SURFACE_REQUIRES_POLICY —— 这是预期行为，不是缺陷。

同目录 08.obj / 09.obj：两个材质皆开放，同样 fail closed，仅作负例。

其余生产模型（model/obj/** 下 36 个）：不选新预设即完全不受影响；
若选中且模型只有单一材质或存在开放材质，一律 fail closed 而非静默降级。
```

## 5. 后续会有什么变化

### 5.1 立即可见的变化

```text
① 工艺下拉多出一项「多材质纵深 RGB + 按需补白（候选）」
② 选中它并提交时，Profile JSON 多出 materialVolumePolicy 块，
   且该预设自身拥有一个新的 profileHash（其余六类哈希不变）
③ 选中它时宿主发出的 slicingMode 为 relief_heightfield
④ 不满足条件（开放材质 / 缺优先级 / 同级重叠 / 材质无 Kd）时
   在【构建期或 Worker 校验期】明确拒绝，并给出 E_MATVOL_* 稳定错误码
```

### 5.2 尚不会发生的变化

```text
⛔ 生产切片路径不变：MATVOL 的语义栈仍未接入 run_slicer（MV-08 未开工）
⛔ 因此选中新预设【暂时不会】产出与旧路径不同的 TIFF 像素
⛔ 结果页仍默认六通道组合预览，S 通道伪彩色仍无说明文字（留待 MV-07C）
⛔ 开放表面壳层仍不可用（MV-04 受 MQ-01/MQ-02 阻塞）
```

### 5.3 后续卡

```text
MV-07B  独立子面板、能力不足时禁用而非回退、UI smoke、持久化
MV-07C  结果页 RGB-only 判读入口与 S 伪彩色标注
MV-08   接入 MEMFLOW bounded/owned 生产路径 —— 届时才会真正改变产出像素，
        且需另行授权；MEMFLOW 当前提交于 codex/memflow-bounded-streaming
        分支及其独立工作树，本产品线尚无对应源文件
```

## 6. 回滚方式

```text
本授权涉及的全部改动可用 git revert 对应提交整体回退：
  新增文件 HostVolumetricProfile.c/.h 直接消失；
  三处新增条件回到原状；
  新增预设从目录移除。
旧六类预设与其 profileHash 在回滚前后均不变，因此回滚不产生二次影响。
```

## 7. 门禁与验收

```text
必过  VerifyPresetProfileHashClosure  七条预设逐条 Worker 同算法哈希闭合
必过  既有六类预设的 profileHash 与放宽前逐条相同（显式对照断言）
必过  Release /W4 /WX 构建，宿主定向 CTest 与全量 CTest 失败集无新增
必过  ValidateSourceSizeGuard：新文件 .c ≤500 行、.h ≤200 行
已知  ValidateQtHostBoundary 的 500 行规则在 HEAD 上已因 8 个既有文件超限而失败；
      本授权不改该规则、不新增超限文件，故不改变该门禁的失败集（MV07-Q2 另议）
```

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-21 | v1.0 | 首版。按用户授权固化 MV07-Q1 的精确范围：三处新增条件加一个新增文件；明确既有六类工艺不受影响的三重保证机制、触发资产与开放面 fail-closed 的预期行为、立即可见与尚不会发生的变化、后续卡、回滚方式与验收门禁；列明七项不在授权内的禁止事项。 |
