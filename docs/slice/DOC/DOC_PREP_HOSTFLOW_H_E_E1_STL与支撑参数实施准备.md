# HOSTFLOW H-E E1 STL 与支撑参数实施准备

> 状态：**E1 PREPARATION GATE = PASS / H-E-01 COMPLETE / H-E-03 READY**
> 日期：2026-08-09
> 范围：H-E-01、H-E-03；不提前实现 E2/E3。

## 1. 边界

E1 只补齐参考宿主的 STL 导入和宿主 Profile 支撑段。所有参数仍经既有
`slice.rgbwsv` 有效 Profile 通道下发，不新增 SPI、能力、导出函数或内部 JSON 依赖。
主干 `apps/slicer_debug_ui/**` 只作为行为参照，不在本阶段修改。

## 2. H-E-01 STL 导入

### 2.1 已确认缺口

核心 importer 已支持 STL，但参考宿主当前在四处仅接受 OBJ/3MF：

- 文件选择器过滤和标题；
- `HostModelImportWorkflow` 后缀校验；
- `HostEffectiveProfileBuilder::Validate`；
- 宿主 Profile C 构造器的格式白名单及相关提示文本。

### 2.2 实施范围

1. 文件过滤增加 `*.stl`，提示统一为 OBJ/3MF/STL；
2. 导入工作流、有效 Profile 校验和 C request builder 同时接受小写规范化 `stl`；
3. 继续检查文件存在、后缀与声明格式一致，未知格式 fail-closed；
4. 导入摘要显示 importer 返回的三角面、顶点、包围盒、法线和预检结果；
5. 不按文件名猜测 ASCII/binary，由核心 importer 处理编码。

### 2.3 Fixture 与门禁

- ASCII：复用已登记的 `samples/models/sample.stl`；
- binary：测试内按固定字节生成最小二进制 STL，避免提交来源不明资产；
- negative：扩展名伪装、截断 binary、未知格式必须失败且不推进 scene revision；
- Debug/Release 均验证导入、addInstance、有效 Profile 与切片请求构造。

## 3. H-E-03 支撑 Profile 段

### 3.1 宿主可编辑合同

E1 采用“生产常用字段 + 高级字段显式收起”的最小结构，不复制主干全部历史实验参数。

| 字段 | 类型/范围 | 默认值 | 语义 |
|---|---|---|---|
| `support.enabled` | bool | true | 是否生成模型外可剥离支撑 |
| `support.mode` | enum | `bottom_projection` | `none`、`bottom_projection`、`unsupported_only`、`bottom_projection_plus_unsupported`、`full_vertical_projection` |
| `support.placement` | enum | `lower` | E1 只允许 `lower`；上表面策略留待材料工艺阶段 |
| `support.value` | 固定整数 | 0 | `black_is_print` 下 S 通道打印值，不提供自由编辑 |
| `support.offsetMm` | 0..10 mm | 0 | 支撑 XY 外扩 |
| `support.minAreaPx` | 0..1000000 | 0 | 过滤小支撑区域 |
| `internalVoid.enabled` | bool | true | 内部闭合镂空写支撑 |
| `internalVoid.minAreaPx` | 0..1000000 | 16 | 内部镂空最小面积 |
| `internalVoid.fillRule` | 固定 enum | `all_internal_voids` | E1 不开放实验规则 |
| `baseProjection.enabled` | bool | false | 支撑最大投影铺底开关 |
| `baseProjection.layerCount` | 0..10000 | 30 | 开启时写入的铺底层数 |

`baseProjection` 若现有生产 schema 还要求 `source` 或 `layerPlacement`，构造器必须写入当前
冻结默认值，UI 不暴露实验枚举。配置对象中的 `materialProcessProfile.support.expected`
与 `support.enabled` 保持一致，禁止形成相互矛盾的双真源。

### 3.2 UI 与数据流

H-E-03 在 `HostSliceSettingsPanel` 增加可折叠“支撑”段；字段先写入
`hostslicesettings`，由 `HostEffectiveProfileBuilder` 校验，再由宿主 C request builder
生成有效 Profile 和 `profileHash`。UI 控件不得直接拼 JSON。

每次编辑必须：更新草稿状态、重新校验、重新计算 hash、使旧的已提交 Profile 失效；
越界值或不支持枚举显示明确中文原因并禁止切片。

### 3.3 测试

1. lower support 默认配置与生产样例字段等价；
2. 关闭支撑时 `expected=false` 且没有残留 S 策略；
3. internal void、base projection 的开关和层数进入有效 Profile；
4. 任一字段变化都会改变 `profileHash`；
5. 越界/未知枚举 fail-closed；
6. workspace 持久化只保存宿主 Profile 草稿，不保存 scene/job/cache 身份；
7. Debug/Release H-A/H-B 与 package 验证回归保持通过。

## 4. 后续批次状态

| 批次 | 状态 | 说明 |
|---|---|---|
| E1 H-E-01 | **COMPLETE（2026-08-10）** | ASCII/binary 均完成导入与切片；三类负例 fail-closed |
| E1 H-E-03 | **READY** | 支撑 Profile 可编辑段可独立开发、验证、提交 |
| E2 H-E-04/05 | **WAIT E1 GATE** | 需复核 E1 Profile 编辑框架后再细化字段 |
| E3 H-E-02/06 | **WAIT E2 GATE** | 批量事务语义与白区预检身份仍不得提前实现 |

## 5. 建议提交顺序

```text
H-E-01  STL 导入（独立提交）
H-E-03  支撑 Profile 段（独立提交）
E1 Gate Debug/Release + 人工复核（状态提交）
```
