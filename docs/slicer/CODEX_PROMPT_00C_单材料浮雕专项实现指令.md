# CODEX_PROMPT_00C_单材料浮雕专项实现指令

> 文档版本：v0.1  
> 建议提交目录：`docs/slicer/`  
> 用途：复制给 VS Code Codex 执行 00C 阶段

---

## 指令

请阅读以下文档：

```text
docs/slicer/DOC_REVIEW_00C_基于当前代码的浮雕专项判断.md
docs/slicer/DOC_DECISION_00C_单材料浮雕模型切片优先级提升.md
docs/slicer/PRD_00C_单材料浮雕模型切片修正.md
docs/slicer/DEV_00C_relief_heightfield单材料浮雕切片设计.md
docs/slicer/DEMO_00C_单材料浮雕切片实施方案.md
docs/slicer/TASKS_00C_单材料浮雕切片任务清单.md
```

当前任务：实现 00C 单材料浮雕模型切片修正。

注意：

```text
1. 不要实现彩色纹理。
2. 不要实现完整光油覆盖策略。
3. 不要引入 Unity。
4. 不要引入 VTK。
5. 不要破坏 closed_mesh_scanline 原有行为。
6. 必须保持 00B 协议：uint8，0=打印，255=不打印。
7. 必须保持通道顺序：R G B W S V。
```

实现内容：

```text
1. 新增 slicingMode = relief_heightfield。
2. 新增 modelMaterial.materialChannel。
3. 新增 modelMaterial.applyMode。
4. 新增 relief.fillMode / relief.baseZMm。
5. 新增 relief_heightfield sampler。
6. 单材料光油模型输出 V=0。
7. 空白区域全部 255。
8. relief 模式默认 support.enabled=false。
9. 输出 reports/relief_report.json。
10. manifest 增加 slicing.mode。
11. 新增 samples/configs/slice_config_relief_varnish.json。
12. 更新 REPORT_00_P0_Demo当前实现状态.md。
```

执行完成后请说明：

```text
1. 修改了哪些文件；
2. 新增了哪些配置字段；
3. 新增 relief sampler 的算法边界；
4. 如何运行普通模式；
5. 如何运行浮雕光油模式；
6. 如何用 rip_reader_test 验证。
```
