# DOC_DECISION_12C UI 产品默认值与交互冻结

> 文档状态：Decision / Stage 12C
> 日期：2026-07-10
> 适用阶段：12C-R1 / 12C-R2

## 1. 决策目的

关闭 `DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md` 中仍未冻结的产品交互问题，使 12C 在 R0 构建门禁通过后可以直接进入原子任务实施，不需要再次猜测默认行为。

## 2. Profile 默认集

普通用户默认只显示以下四类稳定 Profile：

```text
textured_nail_rgb_white_lower_support：彩色纹理甲片，RGB 表层、白墨模型填充、下表面支撑；
textured_nail_rgb_varnish_lower_support：彩色纹理甲片，RGB 表层、光油模型填充、下表面支撑；
single_material_relief：单材料浮雕，填充材料由设置面板选择；
production_rgb_inspection：生产 RGB 检查，作为明确标注的调试 Profile。
```

`advanced`、`fixture` 和 `hidden` 配置继续保留，但默认不进入普通用户下拉列表。R1 可以校正具体模板路径，不得改变上述分类语义，除非新增决策记录。

## 3. UI 修改与运行行为

用户点击“一键切片”或等价运行入口时，UI 必须自动执行：

```text
Profile template + 当前 UI overrides
-> session generated effective config
-> config validation
-> slicer_cli
```

不得要求用户先覆盖保存原始 template/fixture JSON。原始配置保持只读语义；生成配置写入本次 session 目录。若校验失败，必须停止切片并显示具体字段错误。

## 4. 模型填充默认语义

生产 Profile 的模型内部填充不得为空。默认使用白墨，也可显式选择光油或后续正式注册的其他模型填充材料。`none/empty` 只允许测试 fixture 使用，并必须标记为非生产安全。

模型填充和模型外支撑是不同语义：

```text
模型内部填充：W/V/其他正式材料通道；
模型外部支撑和内部镂空支撑：S 通道；
颜色表层：RGB；
外侧光油壳层：V 通道，默认厚度 0 mm。
```

## 5. 诊断区域默认位置

`DiagnosticsDock` 默认位于底部并折叠。右侧区域保留当前层摘要、材料图例和六通道像素探针。用户可以展开底部区域查看报告、曲线和日志，但诊断区域不得永久挤压主预览。

## 6. 12D 接入边界

12C 不实现 12D 横截面材料闭环算法。若 package 已包含 12D 兼容报告，12C 可以只读显示；没有报告时显示“未提供闭环诊断”，不得自行推断生产通过。

12D 不阻断 12C-R0/R1。12C-R2 也不得为了显示占位状态而把 12D 业务判断写入 UI。

## 7. OpenVDB 展示边界

OpenVDB 固定显示为 `utility/candidate`：

```text
默认关闭；
productionReplacementAllowed=false；
可显示诊断和 fallback；
不得成为普通 Profile 的默认生产引擎；
不得把 utility PASS 翻译成生产切片 PASS。
```

## 8. 状态划分

### Current State

现有 `ScenarioRegistry`、`QuickConfigPanel` 和三个预览 panel 已提供基础能力，但尚未实现本决策要求的 generated effective config、统一工作区和底部诊断区。

### Target State

R1/R2 按本决策实现稳定 Profile、自动会话配置、非空模型填充、统一预览和底部诊断区。

### Historical State

直接保存 fixture JSON、顶级报告/曲线 tab 和独立预览 layer 状态属于历史调试交互，不再作为产品目标。

### Pending Confirmation

R0 无待确认产品问题。R1 可以在不改变本决策语义的前提下确认具体模板文件和字段映射；涉及默认材料、诊断位置或 OpenVDB 生产定位的变更必须新增决策记录。
