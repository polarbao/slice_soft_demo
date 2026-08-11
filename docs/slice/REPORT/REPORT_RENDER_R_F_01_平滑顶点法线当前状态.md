# REPORT R-F-01 平滑顶点法线当前状态

> 状态：**IMPLEMENTATION COMPLETE / VALIDATED**  
> 日期：2026-08-11  
> 后续：R-F-02 基线重固化与真实资产预算重测

## 1. 当前成果

- `SceneViewMeshBuilder` 不再把单个面法线直接复制给三角形的三个顶点；
- 按共享几何边建立相邻面关系，并按顶角加权计算平滑顶点法线；
- 以 `40°` 作为当前冻结的 crease angle，低于阈值的曲面连续平滑，高于阈值的硬边保持分裂；
- UV seam 可以共享平滑法线，但最终顶点键仍包含 UV，因此不会破坏纹理接缝；
- 材质组之间不跨组平滑，避免把材料边界意外抹平；
- `meshIdentity` 随派生法线内容变化，SPI v1、11 个导出、15 项能力和生产 RGBWSV TIFF 合同均未修改。

## 2. 验收结果

| 项目 | 结果 | 证据 |
|---|---|---|
| 小于 40° 的连续曲面共享顶点 | PASS | 新增轻微弯折四边形单测，输出 4 顶点 |
| 90° 硬边保持分裂 | PASS | 新增直角折面单测，输出 6 顶点 |
| UV seam 保持 | PASS | 原有 ViewData 正向用例继续通过 |
| Debug provider 回归 | PASS | `textured_scene_viewdata_14b03a_unit_tests` 1/1 |
| Release 真实纹理与宿主回归 | PASS | provider、real fixture、three_d、hostflow 4/4 |
| 真实宿主显示 | PASS | `build-slicesoft/main/hostflow_hd02_evidence/Release/hostflow_hd02_textured_three_d.png` |

真实宿主截图已重新生成并人工检查：纹理甲片能够在 3D 视图中正常显示，曲面不再使用逐三角面着色；底部和高角度边界仍保留清晰轮廓。仓库没有保留修改前同场景、同相机截图，因此本任务不虚构逐像素前后差异；量化的顶点合并率和字节变化统一由 R-F-02 的 22 资产重测给出。

## 3. 实际验证

```powershell
cmake --build build-slicesoft/main --config Debug --target `
  textured_scene_viewdata_14b03a_unit_tests --parallel
ctest --test-dir build-slicesoft/main -C Debug `
  -R '^textured_scene_viewdata_14b03a_unit_tests$' --output-on-failure

cmake --build build-slicesoft/main --config Release --target `
  textured_scene_viewdata_14b03a_unit_tests `
  textured_scene_viewdata_14b03a_real_fixture_tests `
  stage14e04c_three_d_tests `
  hostflow_hd02_three_d_canvas_tests --parallel
ctest --test-dir build-slicesoft/main -C Release `
  -R '^(textured_scene_viewdata_14b03a_unit_tests|textured_scene_viewdata_14b03a_real_fixture_tests|slicer_stage14e04c_three_d_contract_test|hostflow_hd02_three_d_canvas)$' `
  --output-on-failure
```

Debug 为 `1/1 PASS`；Release 为 `4/4 PASS`。

## 4. 边界与风险

- crease angle 当前是 ViewData 内部策略参数，默认 `40°`，尚未提升为公开 DTO 或 UI 参数；
- 本任务只修正显示派生网格的法线，不修改切片几何、模型修复、OpenVDB、TIFF 或材料策略；
- 对极端非流形顶点，仅平滑真正共享边且满足角度阈值的面，不把“只碰到同一点”的孤立面错误合并；
- R-F-01 完成后，旧的 `105.15 B/三角` 基线已经失效，必须完成 R-F-02 后才能关闭 R-F 线。
