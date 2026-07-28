# REPORT 13B-06 单 Package 与 Scene Report 当前状态

> 文档状态：FIXTURE COMPLETE / PRODUCTION INPUT OPEN
> 日期：2026-07-28
> 前置：13B-05 FIXTURE COMPLETE
> 下一任务：13B-07 真实模型矩阵与阶段收口

## 1. 本任务结论

13B-06 已把 13B-05 的 writer-ready 联合内存层接入现有共享 RGBWSV writer，形成一个场景、
一个 package、每个全局 layerIndex 一个 TIFF，并在同一 staging 事务内发布 scene summary 和
`reports/multimodel_scene_report.json`。

当前结论仅为功能 fixture 完成，不代表正式设备 production GO。fixture 使用显式
`functional_fixture_admitted`，scene report 使用 `functional_fixture_format_written`，
`productionReady=false`，不会冒充正式设备准入。

## 2. 当前实现

### 2.1 Scene 写包入口

新增 `WriteMultiModelSceneProductionPackage`：

```text
只消费 SceneLayerComposeResult 完整联合层；
校验 scene/admission/composition 的 sceneId 与 revision；
校验请求模式等于实际模式；
把 SceneRasterGrid 映射到共享 RGBWSV grid；
构造 typed scene extension；
只调用一次 WriteRgbwsvProductionPackage。
```

该入口不重新切片、不反读 TIFF、不重复材料合成，也不生成逐实例 package。

### 2.2 Typed scene extension

共享 writer 新增受控的可选扩展：

```text
manifest.scene；
manifest.reports.scene；
reports/multimodel_scene_report.json。
```

固定 schema：

```text
slicesoft.multimodel_scene_summary.13b.1
slicesoft.multimodel_scene_report.13b.1
```

未提供 scene extension 的旧单模型请求保持原输出，不增加空 scene 字段。

### 2.3 Scene report

scene report 当前记录：

```text
sceneId/sceneRevision/sceneHash；
requested/effective pipeline mode；
scene-wide Profile；
buildVolume/layout；
model source/resource identity；
逐实例 transform revision/hash；
逐实例 admission 与 composition 统计；
全局 grid/protocol/layerCount；
model/outerVarnish/support/empty 像素；
composeMs/peakWorkingBytes；
package 与固定 report 路径。
```

### 2.4 证据闭环

写包前新增以下 fail-closed 校验：

```text
MultiModelScene 按 fixture/production purpose 重新校验；
scene 与 admission buildVolume 必须一致；
可见实例必须 admitted、boundsValid、inBounds、无碰撞和错误；
transform revision/hash 必须与当前实例一致；
联合层必须保持 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
层序、尺寸、true-Z、channel order 和 byte count 必须完整；
实际非空 RGBWSV 像素必须与 model/outerVarnish/support 统计闭合；
逐实例统计之和必须与全局统计一致；
scene summary/report identity、schema、固定路径和 package identity 必须一致。
```

## 3. 原子发布强化

scene report 在 staging 中与 TIFF、manifest、slice report、preview report 同时写入。JSON 输出现在
显式检查 write、flush 和 close；scene summary/report 写入后会重新读取并校验，再运行 RIP strict。

共享 package 替换流程已修正：

```text
新 staging 发布失败时恢复旧 package；
新 package 已发布后，旧 backup 清理失败不再删除新 package；
backup 清理成功后不再返回已不存在的 backup 路径；
任一前置或写入校验失败均清理 staging。
```

## 4. 固定协议

13B-06 未改变生产协议：

```text
schema/schemaVersion = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
sampleFormat = uint
polarity = black_is_print
printValue = 0
emptyValue = 255
storageMode = stripped / tiled
```

## 5. 测试覆盖

新增 `multi_model_package_writer_unit_tests`，覆盖：

```text
单实例和双实例 scene package；
每个全局 layerIndex 只有一个 TIFF；
RIP strict 接受可选 scene metadata；
旧单模型 writer 无 scene extension 兼容；
blocked composition；
scene identity/revision 不一致；
非法 scene schema/report path；
pipeline mode、grid/layer、protocol 不一致；
过期 transform evidence；
admission 状态与 buildVolume 矛盾；
compose 统计与实际 RGBWSV 像素不闭合；
fixture 冒充 production admission；
相同输入重复替换后的 manifest/report 业务字段确定。
```

## 6. 本轮验证证据

2026-07-28 实际执行：

```powershell
cmake --build build --config Debug --target multi_model_package_writer_unit_tests rgbwsv_production_package_writer_unit_tests rip_reader_test
ctest --test-dir build -C Debug -R "^(multi_model_package_writer_unit_tests|rgbwsv_production_package_writer_unit_tests|multi_model_layer_composer_unit_tests|scene_layer_adapters_unit_tests)$" --output-on-failure
cmake --build build --config Debug
.\build\Debug\rip_reader_test.exe --package <fixture-package> --summary
powershell -ExecutionPolicy Bypass -File scripts\run_ci_quick.ps1
```

结果：

```text
定向 CTest：4/4 PASS；
Debug 全量构建：PASS；
fixture RIP：PASS，8 x 4 x 2，635 x 600 DPI，bitDepth=8，RGBWSV，warnings=0；
Quick CI：PASS，包含 build、quick regression、golden、UI self-test 和 overlay-load-real。
```

## 7. 未完成范围

13B-06 不包含：

```text
13B-07 的 1/11/12/22 真实模型矩阵；
真实设备 buildVolume、原点和轴向；
22 实例正式性能预算；
跨实例联合支撑；
mixed-profile 或混合 Legacy/Global 场景；
Qt 一键多模型联合切片接线；
13C TIFF 原生统一预览。
```

## 8. 下一步

下一任务为 13B-07。功能矩阵开发可以继续，但正式 Stage 13B production GO 仍必须等待：

```text
设备 buildVolume；
设备原点和 X/Y 轴向；
22 实例性能预算/验收阈值。
```

因此 13B-07 必须把“功能 fixture PASS”和“正式 production GO”分开报告，不得用真实模型功能验证
替代设备输入。
