# DOC PREP R-F-02 真实资产预算重测实施准备

> 状态：**PREPARATION COMPLETE / IMPLEMENTATION AUTHORIZED**  
> 日期：2026-08-11  
> 前置：R-F-01 `32b89b0` 已完成并通过 Debug/Release 门禁

## 1. 目标与边界

R-F-01 改变了 ViewData 顶点法线和 `meshIdentity`，旧的 `105.15 B/三角` 及其聚合预算结论失效。本任务在同一 Release 真实资产矩阵中重测：

1. 22 个可显示 OBJ 的网格、纹理和俯视预览字节；
2. CPU 参考后端的网格/纹理真实复制上传耗时；
3. three_d 与 top ViewData 的完整刷新耗时；
4. `meshIdentity` 变化对缓存和断言的影响；
5. R-C-01 屏幕空间纹理分辨率合同是否具备数据价值。

本任务不修改 DTO、SPI、LOD 选择策略、纹理分辨率、渲染后端和生产 TIFF。

## 2. 已读取实现

- `tests/hostflow/HostThreeDCanvasTests.cpp`：既有 36 OBJ / 22 有效资产矩阵；
- `apps/slicer_ui_host_sim/render/SceneRenderPolicy.*`：three_d ViewData、float16 wire 与后端上传；
- `apps/slicer_ui_host_sim/render/TopViewRenderPolicy.*`：俯视 preview 解码缓存；
- `apps/slicer_ui_host_sim/render/CpuRasterBackend.*`：宿主实际资源复制边界；
- `REPORT_RENDER_R_A_02_顶点共享后真实资产重测.md`：旧 105.15 B/三角基线；
- `REPORT_RENDER_R_B_04_ViewData半精度传输当前状态.md`：304 B → 176 B fixture 基线。

## 3. 实施方案

1. 让测量后端包装 `CpuRasterBackend`，不再以空实现伪造上传成功；
2. 分别用 `QElapsedTimer` 包围 mesh/texture 后端上传和 three_d/top 完整刷新；
3. 对 top frame 按 `previewIdentity` 去重后累计 `QImage::sizeInBytes()`；
4. 保留既有 CSV 字段并追加上传/刷新耗时，避免破坏历史证据读取；
5. 聚合证据显式区分：
   - `meshBytes`：宿主解码后的 float32 资源内存；
   - `textureBytes`：宿主 RGBA8 纹理内存；
   - `previewBytes`：宿主 RGBA8 俯视预览内存；
   - `*UploadNs`：CPU 后端资源复制；
   - `*RefreshNs`：模块查询、生成、传输、解码和上传的整段耗时。

## 4. 风险控制

- 单轮耗时只能作为本次同机证据，不能宣称稳定性能基线或 p50；
- `threeDRefreshNs` 和 `previewRefreshNs` 不是纯上传耗时，报告中不得混写；
- wire float16 的 176 B fixture 与宿主解码后的 `meshBytes` 不是同一口径；
- 资产合同拒绝仍保持 fail-closed，不通过灰模扩大“有效资产”数量；
- 不把 R-C-01 自动转 READY；即使数据证明收益，也仍需受控合同修订和打印侧回签。

## 5. 验证命令

```powershell
cmake --build build-slicesoft/main --config Release `
  --target hostflow_hd02_three_d_canvas_tests --parallel
ctest --test-dir build-slicesoft/main -C Release `
  -R '^hostflow_hd02_real_asset_matrix$' --output-on-failure
ctest --test-dir build-slicesoft/main -C Release `
  -R '^slicer_stage14e04c_three_d_contract_test$' -V
```

输出证据目录：`build-slicesoft/main/hostflow_hd02_evidence/Release/`。
