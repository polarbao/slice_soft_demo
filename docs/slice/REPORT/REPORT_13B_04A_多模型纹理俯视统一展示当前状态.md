# REPORT 13B-04A 多模型纹理俯视统一展示当前状态

> 状态：COMPLETE
> 日期：2026-07-27
> 后续：13B-05 全局 Raster 与联合层合成

## 1. 完成内容

```text
SceneViewGeometry 增加变换后 Z、UV、材质索引和显示材质资源；
后台投影任务生成最大 768 像素边长的 RGBA 俯视 SurfacePreview；
SurfacePreview 使用逐像素 +Z Z-buffer，避免三角形平均深度导致的错误遮挡；
OBJ/3MF 已解析贴图按 UV 和配置采样参数显示，无贴图时显示 diffuse RGB；
纹理采样参数和完整材质显示信息进入资源/缓存 identity；
Qt paintEvent 只绘制内存 SurfacePreview，不执行贴图文件 IO；
追加导入后自动执行当前 11x2 规则排版，避免全部实例叠在原点；
自动排版失败时保留场景并显示错误；
场景存在尚未完成的重投影时禁止追加和排版，避免陈旧几何被改写 identity；
排版平移后同步刷新 transformHash、geometryHash 和 revision；
工作区文案明确为“场景 +Z 俯视”和“全部可见模型”。
```

## 2. 验证证据

已实际运行：

```powershell
cmake --build build --config Debug --target scene_view_geometry_unit_tests
cmake --build build --config Debug --target scene_document_unit_tests
ctest --test-dir build -C Debug -R "^(scene_view_geometry_unit_tests|scene_document_unit_tests)$" --output-on-failure
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-list --repo-root .
```

结果：

```text
scene_view_geometry_unit_tests、scene_document_unit_tests：PASS；
multi-model-list：PASS；
覆盖材质/UV/深度合同、真实纹理 Fixture 的颜色变化、自动排版、资源共享、
真实纹理 Fixture 导入、选择/隐藏/锁定/删除和
1280x720、1440x900、1920x1080 布局。
```

## 3. 当前边界

```text
本阶段是切片前显示能力，不替代 13B-05 生产 Raster；
SurfacePreview 最大边长 768 像素，不承诺逐像素等同生产 TIFF；
正式设备 buildVolume/origin/axes 仍是 13B production 外部 Gate；
无贴图或贴图缺失时按 diffuse/fallback 显示，不静默生成生产颜色。
```

## 4. 结论

当前俯视工作区已从“单模型感知的固定色轮廓”提升为“全部可见实例统一显示、追加后自动分开、
可显示纹理资源”的场景视图。13B-05 可以继续在此基础上实现生产级共享 Raster 和联合层合成。
