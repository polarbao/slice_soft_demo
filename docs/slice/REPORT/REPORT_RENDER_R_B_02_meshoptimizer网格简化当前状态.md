# REPORT R-B-02 meshoptimizer 网格简化当前状态

> 状态：**COMPLETE**
> 日期：2026-08-10
> 决策：`RD-B = meshoptimizer`，用户已明确授权第三方依赖

## 1. 任务结论

R-B-02 已用 meshoptimizer 1.1 的属性感知网格简化替换 ViewData 原有跳采样。LOD 降级不再任意删除
三角形；当输入拓扑或预算无法安全闭合时，模块显式返回
`PM-SLICER-VIEWDATA-SIMPLIFICATION`，禁止回退为破洞网格。

本任务不改变 `PM_SPI_VERSION=1`、11 个导出、15 项能力、RGBWSV TIFF、OpenVDB 默认状态或生产切片算法。

## 2. 实现范围

- `MeshSimplifier` 是唯一第三方适配边界，meshoptimizer 类型不进入公开 DTO；
- 每个材质组按 `position + UV` 建立索引，材质边界和 UV seam 保持分裂；
- 使用 `meshopt_simplifyWithAttributes`，UV 参与属性误差，最大相对误差固定为 2%；
- 使用 `meshopt_SimplifyLockBorder` 锁定外轮廓，禁止 sloppy/prune；
- 简化后复核索引、退化三角、连通分量和新增孤立三角；
- 低于预算的 mesh 保持原索引，不发生无意义简化；
- 多材质预算继续沿用 R-B-01 的确定性比例配额。

## 3. 依赖与部署

| 项目 | 冻结结果 |
|---|---|
| 依赖 | meshoptimizer 1.1 |
| 许可证 | MIT；全文为 `licenses/meshoptimizer.txt` |
| vcpkg | manifest mode，仓库锁定 baseline |
| 默认 triplet | `x64-windows-static-md` |
| 链接位置 | `slicer_base` 私有静态链接 |
| Runtime | 不新增 meshoptimizer DLL；NOTICE 和完整 licenses 必须随包分发 |

LibTIFF 可选轨道继续使用 `x64-windows`。Assimp、TIFF 默认后端及 OpenVDB 状态均未因本任务改变。

## 4. 质量门禁

新增用例覆盖：

1. 16000 三角双材质连通网格压至 10000 三角预算内，各组比例误差不超过 5%；
2. 每个材质 submesh 保持单一连通分量且无孤立三角；
3. 平面 fixture 的锁定轮廓 X/Y 边界零漂移，满足不超过模型尺寸 2% 的门禁；
4. UV seam 两侧 `u=0` 与 `u=1` 均被保留；
5. 10001 个断开三角无法安全简化时明确失败，不产生跳采样结果；
6. 真实 OBJ fixture 继续通过顶层 mesh identity 复用合同。

## 5. 实际验证

已在本次任务实际运行：

```powershell
cmake --build build-slicesoft/render-rb --config Debug --target `
  textured_scene_viewdata_14b03a_unit_tests `
  textured_scene_viewdata_14b03a_real_fixture_tests
ctest --test-dir build-slicesoft/render-rb -C Debug `
  -R "^(slicer_stage14b_target_graph_test|slicer_source_size_guard_self_test|third_party_notice_contract_test|textured_scene_viewdata_14b03a_unit_tests|textured_scene_viewdata_14b03a_real_fixture_tests)$" `
  --output-on-failure
```

结果：`5/5 PASS`。

Release 同一组五项门禁已通过；能力包重新构建后，`TestSlicerModulePackage.ps1` 通过纯 C 宿主生成并验证
RGBWSV 参考包，输出 `STAGE14F01_PACKAGE_VALIDATION_PASS`。打包门禁同时确认 NOTICE、
`licenses/meshoptimizer.txt`、checksum 和 runtime inventory 完整。验证过程中发现门禁仍固定查找旧
`stage14e01_package` 目录，已改为在隔离 evidence root 中要求恰好一个 `manifest.json`，与 H-A-03 当前动态
package identity 对齐。

## 6. 剩余边界

- R-B-03 尚未细分 `truncationReason`；当前安全简化与历史抽稀语义仍需合同化区分；
- R-B-04 的半精度量化尚未执行，不在本任务中修改 DTO 二进制表达；
- 真实甲片的交互帧时间、上传延迟和 VRAM 指标归后续 R-E 性能任务；
- 本任务只修显示 ViewData，不改变生产 TIFF 像素、层数和材料所有权。
