# CODEX_PROMPT_R0_正式项目架构审查与重构设计指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 适用阶段：R0  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_07B_R1_UI真实OverlaySmoke与配置编辑器小收口当前实现状态.md
docs/slicer/DOC_DECISION_R0_07B_R1后进入正式项目架构重构阶段.md
docs/slicer/PRE_R0_DECISION_纹理壳层与光油几何策略约束.md
docs/slicer/ARCH_00_R0_P0Demo架构审查与正式项目重构策略.md
docs/slicer/ARCH_01_R0_正式项目模块边界与目录结构设计.md
docs/slicer/ARCH_02_R0_pipeline执行链路与策略插入点设计.md
docs/slicer/ARCH_03_R0_config_report_test_schema设计.md
docs/slicer/ROADMAP_R0_R1_R2_正式项目重构路线.md
docs/slicer/TASKS_R0_架构审查与重构设计任务清单.md
```

当前阶段：

```text
R0：P0 Demo 架构审查与正式项目重构设计
```

R0 目标：

```text
1. 盘点当前 Demo 代码资产；
2. 判断哪些模块保留、哪些模块重构；
3. 设计正式项目目录结构；
4. 设计 pipeline step；
5. 设计 TextureApplicationPolicy；
6. 设计 VarnishGeometryPolicy；
7. 设计 config/report/test schema；
8. 输出 R1/R2 的边界，不提前做详细 patch。
```

R0 不要做：

```text
大规模移动代码；
重写 slicer_core；
实现 surface_shell_texture；
实现 compensated_varnish；
引入 OpenVDB；
引入设备通信；
修改 p0.rgbwsv.2 输出协议。
```

必须保持：

```text
当前 quick regression 通过；
当前 slicer_cli / rip_reader_test / slicer_debug_ui 可构建；
当前 RGBWSV 输出协议不变。
```

完成后生成：

```text
docs/slicer/REPORT_R0_正式项目架构审查与重构设计当前状态.md
```

报告必须包含：

```text
1. 当前代码资产盘点；
2. 模块保留/重构清单；
3. 正式目录结构建议；
4. pipeline step 设计；
5. TextureApplicationPolicy 设计；
6. VarnishGeometryPolicy 设计；
7. config schema 设计；
8. report schema 设计；
9. test/CI 设计；
10. R1/R2 是否可以进入的判断。
```
