# REPORT HOSTFLOW H-E-01 STL 导入当前状态

> 状态：**COMPLETE**
> 日期：2026-08-10
> 任务：`H-E-01`
> 范围：参考宿主 STL 文件入口、导入、预检、有效 Profile 与生产切片闭环

## 1. 当前结论

参考宿主现已把 STL 纳入与 OBJ/3MF 相同的公开能力链路：

```text
文件选择 → model.import → addInstance → geometry.preflight
         → 宿主有效 Profile（input.format=stl）
         → slice.rgbwsv → manifest.json
```

实现没有新增 SPI、能力或导出函数，也没有读取模块内部配置目录。ASCII/binary
编码判定继续由核心 importer 负责，宿主不根据文件名或文件头猜测编码。

## 2. 实现范围

- 文件选择器、模型列表提示和导入摘要增加 STL；
- `HostModelImportWorkflow` 接受规范化 `.stl` 后缀；
- `HostEffectiveProfileBuilder` 与 C request builder 接受 `stl`，并保持后缀/声明一致校验；
- 增加四面体 ASCII fixture；binary fixture 在测试内以固定 little-endian 字节生成；
- 新增独立 `hostflow_he01_stl_import` 门禁，实际执行两种编码的导入与切片；
- 伪装 STL、截断 binary STL、未知扩展名均显式失败且场景 revision 不变。

## 3. 实测验证

2026-08-10 已执行：

```powershell
cmake --build build --config Debug --target hostflow_he01_stl_import_tests slicer_ui_host_sim
cmake --build build --config Release --target hostflow_he01_stl_import_tests slicer_ui_host_sim

ctest --test-dir build -C Debug -R "^(slicer_source_size_guard_self_test|slicer_stage14e02_qt_host_boundary_test|hostflow_hb01_model_import|hostflow_he01_stl_import|hostflow_hb05_slice_settings|hostflow_hb06_slice_job)$" --output-on-failure
ctest --test-dir build -C Release -R "^(slicer_source_size_guard_self_test|slicer_stage14e02_qt_host_boundary_test|hostflow_hb01_model_import|hostflow_he01_stl_import|hostflow_hb05_slice_settings|hostflow_hb06_slice_job)$" --output-on-failure
```

结果：Debug `6/6 PASS`，Release `6/6 PASS`。H-E-01 专项输出为：

```text
HOSTFLOW_H_E_01_STL_PASS ascii=1 binary=1 slices=2 negatives=3
```

## 4. 边界

- STL 没有 OBJ/MTL 贴图语义，本任务不为 STL 推断纹理；
- 未实现批量导入，归属 E3 的 H-E-02；
- 未修改 `apps/slicer_debug_ui/**`；
- 未修改 RGBWSV、TIFF、RIP、ViewData 或冻结 ABI 合同；
- E1 仍需完成 H-E-03，并在批次门复核 Profile 编辑框架后才能进入 E2。
