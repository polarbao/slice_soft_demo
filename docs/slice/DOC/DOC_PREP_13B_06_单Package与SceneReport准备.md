# DOC_PREP 13B-06 单 Package 与 Scene Report 准备

> 文档状态：READY FOR FIXTURE DEVELOPMENT
> 日期：2026-07-27
> 前置：13B-05 FIXTURE COMPLETE
> 下一报告：`REPORT_13B_06_单Package与SceneReport当前状态.md`

## 1. 目标

消费 13B-05 已完成且 `SceneLayerComposeResult::IsValid()` 为真的联合 RGBWSV 内存层，使用现有
共享生产 writer 原子发布一个场景 package，并新增可审计的 scene report。

本任务必须形成：

```text
一个 scene；
一个 package；
每个全局 layerIndex 一个 RGBWSV TIFF；
一个保持 p0.rgbwsv.2 的 manifest；
一个 reports/multimodel_scene_report.json；
RIP strict 可读取；
失败不留下新的成功 package 或半成品。
```

## 2. 已核对的代码事实

当前可直接复用：

```text
SceneLayerComposeResult：13B-05 writer-ready 全局层、grid、协议、统计；
RgbwsvProductionPackageWriteRequest：共享 grid/storage/preview/生产准入请求；
WriteRgbwsvProductionPackage：TIFF、manifest、报告、staging、RIP 校验和原子发布；
validate_slice_package：严格校验 p0.rgbwsv.2、grid、层清单和 TIFF；
WriteReportJsonFile：统一 JSON 输出。
```

当前约束：

```text
共享 writer 只生成 slice_report/preview_report，尚无 typed scene extension；
request.sourceModelPath 是单模型历史字段，不足以承载场景身份；
manifest Reader 当前只严格校验固定生产字段，并允许额外可选元数据；
scene report 必须在 staging 内写完并参与同一次原子发布，不能在 package 发布后补写；
13B-05 结果仍是完整内存层，13B-06 不重复切片、不反读 TIFF、不重新合成材料。
```

## 3. 固定协议和兼容策略

生产协议保持：

```text
schema/schemaVersion=p0.rgbwsv.2；
channelOrder=R,G,B,W,S,V；
channelCount=6；
bitDepth=8；
sampleFormat=uint；
polarity=black_is_print；
printValue=0；
emptyValue=255；
每个 layerIndex 一个 TIFF。
```

manifest 只增加两个可选字段：

```json
{
  "scene": {
    "schema": "slicesoft.multimodel_scene_summary.13b.1",
    "sceneId": "scene_001",
    "sceneRevision": 7,
    "instanceCount": 2,
    "visibleInstanceCount": 2,
    "sceneReport": "reports/multimodel_scene_report.json"
  },
  "reports": {
    "slice": "reports/slice_report.json",
    "preview": "reports/preview_report.json",
    "scene": "reports/multimodel_scene_report.json"
  }
}
```

不得修改或重命名现有 manifest 固定字段。旧单模型请求不提供 scene extension 时，输出必须保持
原行为，不新增空 `scene` 字段。

## 4. Scene Report 合同

文件固定为：

```text
reports/multimodel_scene_report.json
schema=slicesoft.multimodel_scene_report.13b.1
```

至少包含：

```text
status/productionOutputWritten；
sceneId/sceneRevision/sceneHash；
requestedPipelineMode/effectivePipelineMode；
scene-wide Profile identity；
全局 grid、协议和 layerCount；
buildVolume/layout 摘要；
modelCount/instanceCount/visibleInstanceCount/hiddenInstanceCount；
逐 model：modelId/sourcePath/sourceHash/resourceHash；
逐 instance：instanceId/modelId、transform revision/hash、effective transform；
逐 instance admission：visible/admitted/inBounds/collisionIds；
逐 instance compose stats：model/outerVarnish/support pixels；
全局 compose stats：model/outerVarnish/support/empty pixels；
composeMs/peakWorkingBytes；
package identity 和 report path。
```

P0 report 不复制每层完整像素统计，不嵌入纹理二进制，不记录设备未确认值为正式值。

## 5. Typed 扩展边界

新增：

```text
src/slicer_core/reports/MultiModelSceneReport.h/.cpp；
src/slicer_core/pipeline/MultiModelScenePackageWriter.h/.cpp；
tests/unit/multi_model_package_writer/Main.cpp。
```

共享 writer 增加受控的可选 scene extension：

```text
manifest scene summary；
scene report JSON；
固定 report relative path；
```

禁止向共享 writer 暴露任意相对路径和任意文件写入列表。scene report path 必须固定，JSON schema
必须精确匹配，summary/report identity 必须一致。

`MultiModelScenePackageWriter` 只负责：

```text
校验 compose result、scene identity、pipeline mode 和 report identity；
把 SceneRasterGrid 转换为 RgbwsvProductionGridSpec；
移动完整联合层到共享 writer request；
挂接 typed scene extension；
调用一次 WriteRgbwsvProductionPackage。
```

## 6. 原子发布和失败语义

```text
任一输入 blocked/invalid -> 不创建 staging；
scene summary/report identity 不一致 -> 不创建最终 package；
写 TIFF、scene report、manifest、RIP 校验任一步失败 -> 删除 staging；
已有成功 package 只在新 staging 全部通过后替换；
替换失败必须恢复旧 package；
不得发布逐实例 package；
不得把部分实例成功记录为 scene PASS；
不得在发布后再补写 scene report。
```

已有共享 writer 的 staging/backup/restore 是唯一发布事务；13B-06 不再实现第二套目录事务。

## 7. 输入准入

必须同时满足：

```text
13B-04 admissionPassed；
13B-05 compose result available/status=ready_for_writer/IsValid；
sceneId 非空且与 report、summary 一致；
sceneRevision 与 admission/compose 证据一致；
effective pipeline mode 为 legacy 或 global_surface_shell；
requested mode 等于 effective mode；
所有可见实例使用同一 Profile、DPI、layerHeight 和 pipeline mode；
正式输出只能使用 production admission；fixture 输出必须保持 fixture 标识，不能声称 production GO。
```

正式设备 buildVolume 尚未冻结时，只能完成功能 fixture package，不得把 fixture PASS 写成正式设备
production GO。

## 8. TDD 矩阵

先写失败测试，再实现：

```text
单实例和双实例联合层各写一个 package；
每个全局 layerIndex 只有一个 TIFF；
manifest 保持 p0.rgbwsv.2 固定字段；
manifest.scene 与 reports.scene 仅在 scene extension 启用时出现；
scene report schema、sceneId/revision/hash 和实例统计正确；
scene report 全局统计与 SceneLayerComposeResult 对账；
RIP strict 接受带可选 scene metadata 的 package；
旧单模型 writer 输出和测试不变；
blocked/invalid compose result 不写 package；
scene/report identity mismatch 不写 package；
protocol/mode/grid/layer mismatch 不写 package；
非法 scene report schema/path 不写 package；
注入写入失败后无 staging 和伪成功 package；
替换已有 package 失败时旧 package 可恢复；
相同输入 manifest/scene report 业务字段确定。
```

## 9. 验证命令

```powershell
cmake --build build --config Debug --target multi_model_package_writer_unit_tests rgbwsv_production_package_writer_unit_tests rip_reader_test
ctest --test-dir build -C Debug -R "^(multi_model_package_writer_unit_tests|rgbwsv_production_package_writer_unit_tests|multi_model_layer_composer_unit_tests|scene_layer_adapters_unit_tests)$" --output-on-failure
.\build\Debug\rip_reader_test.exe --package <fixture-package> --summary
.\scripts\run_ci_quick.ps1
git diff --check
```

## 10. 非目标和停止条件

本任务不做：

```text
跨实例联合支撑；
混合 Legacy/Global 场景；
重新切片或重新计算材料；
从 TIFF 反读再生成 scene report；
Qt 一键联合切片接线；
13B-07 真实模型矩阵；
13C TIFF 原生统一预览；
RIP、半色调或设备通信。
```

发生以下任一情况立即停止：

```text
需要修改 p0.rgbwsv.2 固定字段；
需要让 RIP Reader 放宽现有必填字段；
需要绕过共享 writer/RIP strict；
需要在最终 package 发布后补写 report；
需要虚构正式设备 buildVolume 或生产预算；
13B-05 compose result 不能作为唯一生产层输入。
```

## 11. 准备结论

13B-06 的输入、输出、manifest 兼容方式、scene report schema、typed writer 扩展、原子事务、负向
测试和停止条件已经冻结。当前没有新的产品输入阻断功能 fixture 开发；正式 production GO 仍等待
设备 buildVolume、轴向和 22 实例预算确认。
