# CODEX_PROMPT 13B-02 模型列表与实例操作执行指令

> 状态：READY FOR DEVELOPMENT
> 日期：2026-07-27
> 前置：13A-05 COMPLETE / M13-1 CANDIDATE PASS，13B-01 COMPLETE

## 1. 必读

```text
AGENTS.md
docs/slice/REPORT/REPORT_13A_模型俯视工作区与实例变换当前状态.md
docs/slice/REPORT/REPORT_13B_01_MultiModelScene与EffectiveConfig当前状态.md
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md
docs/slice/DOC/DOC_PREP_13B_02_模型列表与实例操作准备.md
```

## 2. 本次范围

只执行 13B-02：

1. 将 Qt `SceneDocument` 从单实例兼容形态扩展为有序 1..22 实例场景草稿；
2. 实现添加、复制、删除、选择、显示/隐藏、锁定/解锁；
3. 同源实例共享只读 `SceneModel`，不同 `modelId` 保持独立 `ResourceScope`；
4. 列表与俯视画布保持单选同步；
5. 锁定实例可查看和选择，但禁止变换及删除；
6. 第 23 个实例必须 fail-closed，失败不得留下半个 source/instance；
7. 保存、回读和取消保持 scene identity、稳定顺序、revision 和 hash；
8. 增加 `scene_document_unit_tests`；
9. 增加 `--ui-smoke-test --case multi-model-list`；
10. 更新状态报告、任务看板和上下文交接。

## 3. 禁止范围

```text
不实现 11x2 规则排版；
不实现碰撞、buildVolume 或生产准入；
不实现多模型联合切片和 package；
不修改 RGBWSV/TIFF 协议；
不修改 Legacy/OpenVDB 默认关系；
不按实例复制完整网格或纹理；
不允许 silent fallback。
```

## 4. TDD 顺序

1. 先新增失败测试，覆盖 1/11/22、23 拒绝、复制、删除、可见、锁定、选择和稳定顺序；
2. 运行定向测试并记录 RED 原因；
3. 最小实现场景文档和命令 API；
4. 补充资源隔离、异步 stale、保存/回读/取消/篡改测试；
5. 实现 `ModelListPanel` 和画布选择同步；
6. 运行 Qt self-test 和 `multi-model-list` smoke；
7. 运行 Quick CI；
8. 生成 `REPORT_13B_02_模型列表与实例操作当前状态.md`。

## 5. 验证

```powershell
cmake --build build --config Debug --target scene_document_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(scene_document_unit_tests|model_transform_unit_tests|scene_view_geometry_unit_tests|scene_transform_controller_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-list
.\scripts\run_ci_quick.ps1
git diff --check
```

若实际 Debug UI 路径不同，使用当前 CMake 输出路径，但不得虚构验证结果。

## 6. 停止条件

以下任一情况必须停止并记录：

```text
需要修改生产 slicer_cli 才能完成；
需要提前实现规则排版、碰撞或联合切片；
同源实例必须复制完整 SceneModel 才能工作；
第 23 个实例产生部分提交；
锁定实例仍可被变换或删除；
旧单模型 13A 回归失败。
```

13B-02 完成后只把 13B-03 标记为下一任务，不自动实现 13B-03。
