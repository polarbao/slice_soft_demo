# CODEX_PROMPT 13C-02 MaterialPreviewComposer 执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/PRD/PRD_13C_RGBWSV_TIFF原生统一预览.md
docs/slice/DEV/DEV_13C_TIFFLayerSource与统一材料合成设计.md
docs/slice/DEMO/DEMO_13C_TIFF原生统一预览验证方案.md
docs/slice/DOC/DOC_PREP_13C_01_TIFFLayerSource与Cache准备.md
docs/slice/DOC/DOC_PREP_13C_02_MaterialPreviewComposer准备.md
docs/slice/REPORT/REPORT_13C_01_TIFFLayerSource与Cache当前状态.md
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
```

## 本次只执行

```text
1. 新增无 Qt MaterialPreviewComposer Public DTO/API；
2. 支持 R/G/B/W/S/V、RGB、RGB+W/S/V、RGB+S+W+V、Occupancy、Empty；
3. 固定 RGB -> W -> S -> V 显示顺序；
4. 输出连续 RGBA、生产通道统计和六通道像素探针；
5. 新增 material_preview_composer_unit_tests；
6. 更新 13C 状态、索引和上下文；
7. 生成 REPORT_13C_02_MaterialPreviewComposer当前状态.md。
```

## 禁止夹带

```text
不接入 Qt Widget；
不创建 QImage；
不读取 preview PNG；
不删除旧 Panel；
不默认关闭 preview 输出；
不从 TIFF 猜 Texture Surface / Model Fill / Partition；
不修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
不执行 13C-03。
```

## 验证

```powershell
cmake --build build --config Debug --target material_preview_composer_unit_tests
ctest --test-dir build -C Debug -R "^material_preview_composer_unit_tests$" --output-on-failure
cmake --build build --config Debug --target tiff_layer_source_unit_tests tiff_layer_cache_unit_tests rip_reader_test
git diff --check
```

完成后停止在 `13C-03 READY`，不得把核心合成器完成写成 UI 统一完成。
