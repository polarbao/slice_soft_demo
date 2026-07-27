# CODEX_PROMPT 13B-03 11x2 规则排版执行指令

> 状态：COMPLETE
> 日期：2026-07-27
> 前置：13B-02 COMPLETE
> 完成提交：`aef10c1 feat(13B-03): 建立11x2规则排版与Qt操作闭环`

## 1. 必读

```text
AGENTS.md
docs/slice/REPORT/REPORT_13B_02_模型列表与实例操作当前状态.md
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md
docs/slice/DEMO/DEMO_13B_多模型排版联合切片验证方案.md
docs/slice/DOC/DOC_PREP_13B_03_11x2规则排版准备.md
```

## 2. 本次范围

只执行 13B-03：

1. 实现无 Qt `GridLayoutPolicy` 或等价核心策略；
2. 支持 1..11 列、1..2 行、最多 22 个实例；
3. 默认列净距 20.00 mm、行净距 30.00 mm；
4. 按稳定实例顺序执行 row-major；
5. 基于变换后 XY AABB 计算边到边净距；
6. 隐藏实例保留占位，锁定实例保持位置且冲突 fail-closed；
7. 排版结果以单事务写回 `SceneDocument`；
8. 保存 requested/derived/effective layout，并回读验证；
9. 新增中文 `SceneLayoutPanel`；
10. 新增 `grid_layout_policy_unit_tests` 和 `scene-grid-layout` UI Smoke；
11. 更新状态报告、任务看板和上下文。

## 3. 禁止范围

```text
不实现精确轮廓碰撞或设备幅面 production admission；
不实现自动 nesting；
不实现跨模型支撑；
不实现联合 Raster、TIFF 或 package；
不修改 Legacy/OpenVDB 默认关系；
不修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print；
不在 Qt 层复制排版数学；
不允许 silent fallback。
```

## 4. TDD 顺序

1. 先新增 1/11/12/22、容量、不同 bbox、间距和确定性失败测试；
2. 运行定向 target 并记录 RED 原因；
3. 最小实现无 Qt layout DTO、policy 和稳定错误；
4. 增加锁定、隐藏、stale revision 和原子失败测试；
5. 接入 `SceneDocument` 原子 apply/restore；
6. 增加 Scene Effective Config 保存、回读和 hash 测试；
7. 实现 `SceneLayoutPanel` 和俯视画布刷新；
8. 运行 `scene-grid-layout` 三窗口 Smoke；
9. 运行 Quick CI；
10. 生成 `REPORT_13B_03_11x2规则排版当前状态.md`。

## 5. 验证

```powershell
cmake --build build --config Debug --target grid_layout_policy_unit_tests scene_document_unit_tests scene_transform_controller_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(grid_layout_policy_unit_tests|scene_document_unit_tests|scene_transform_controller_unit_tests|model_transform_unit_tests|scene_view_geometry_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test --repo-root .
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-list
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-grid-layout
.\scripts\run_ci_quick.ps1
git diff --check
```

## 6. 停止条件

```text
必须修改生产 slicer_cli 才能完成；
必须修改 Z 落台；
需要提前实现 13B-04 碰撞或 production buildVolume Gate；
排版失败会部分移动实例；
锁定实例被隐式移动；
13A/13B-02 回归失败。
```

13B-03 已完成并解锁 13B-04。实际证据见
`docs/slice/REPORT/REPORT_13B_03_11x2规则排版当前状态.md`。
