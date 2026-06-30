# ROADMAP_v1.4_REPORT07后续路线_Qt参数编辑与Profile可视化

> 文档版本：v1.4  
> 阶段：REPORT_07 之后  
> 建议目录：`docs/slicer/`

## 1. 当前阶段链路

```text
P0 → 00A → 00B → 00C → 01 → 02 → 03 → 04 → 04A → 05 → 03B → 03C → 06 → 06A → 06B → 05A → 07
```

## 2. 当前能力基线

```text
slicer_debug_ui
QProcess wrapper
config/package 选择
slicer_cli / rip_reader_test / quick regression / profile compare 执行
manifest / reports 查看
preview PNG / PPM 查看
material_process_report summary 查看
日志和错误码查看
```

## 3. 下一阶段：07A

```text
Config form editor
MaterialProcessProfile editor
MaterialPolicy editor
MaterialRoleMapping rule editor
Support parameter editor
per-layer channel chart
profile compare chart
preview overlay
config validation
Save / Save As / duplicate config
```

## 4. 07A 后续路线

完成 07A 后再判断：

- `08`：支撑形态与工艺优化。
- `06C`：复杂 3MF 材料扩展。
- `09`：OpenVDB / SDF 几何内核预研。
- 真实 RIP / Device integration 文档。
