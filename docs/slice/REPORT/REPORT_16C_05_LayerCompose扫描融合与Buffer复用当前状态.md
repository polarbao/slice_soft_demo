# REPORT_16C-05 Layer Compose 扫描融合与 Buffer 复用当前状态

> 状态：**IMPLEMENTATION COMPLETE / PERFORMANCE RE-MEASURE PENDING**
> 日期：2026-08-17
> 对应任务：`16C-05`

## 1. 当前结论

五模型 0.021 mm 样本中，“场景合成”与“报告处理”包含多次对同一批 RGBWSV 内存层的完整
扫描。原生产链在 Composer 已完成源层闭合和最终层闭合后，服务、Scene Writer、Scene Report、
逐实例摘要和逐层统计仍会分别重新扫描，且 Orchestrator 还会复制全部实例 raster。

本次已完成代码优化：

1. Composer 在源层闭合扫描中同步累计逐实例六通道打印/空白计数和打印包围盒；
2. Composer 在最终层闭合扫描中同步累计每层六通道打印/空白计数；
3. 生产链使用 `ValidatedSceneLayerComposeResult` 传递不可伪造的闭合证据，报告与写包不再重复
   调用深扫描 `SceneLayerComposeResult::IsValid()`；
4. Orchestrator 使用同步 borrowed span 读取权威 raster，不再复制多 GB 实例层 buffer；融合统计形成
   后，生产服务在发布 Package 前释放实例 raster；
5. 持久化安全边界不放松：严格 RIP Reader 仍独立解码 TIFF，并核对 manifest 层统计与 TIFF
   实际统计；不一致返回 `E_LAYER_STATISTICS_MISMATCH`。

## 2. 影响边界

本次只改变统计证据的形成与复用位置，没有改变：

```text
p0.rgbwsv.2
R G B W S V
uint8
black_is_print
Model > Support > Empty
Legacy / S0 / P0 默认值
TIFF 压缩和字节写入规则
SPI v1、Worker 文件合同和 Profile hash
```

公共 `SceneLayerComposeRequest` 继续拥有实例 vector；borrowed span 只作为同步内部入口，避免把
非拥有生命周期暴露给普通调用方。

## 3. 功能验证

Release 定向 CTest：

```text
multi_model_layer_composer_unit_tests             PASS
multi_model_package_writer_unit_tests             PASS
multi_model_production_service_unit_tests         PASS
rgbwsv_production_package_writer_unit_tests       PASS
hostflow_hb05_slice_settings                      PASS
hostflow_hb05_settings_ui_smoke                    PASS
hostflow_hb06_slice_job                            PASS
hostflow_hb06_job_ui_smoke                         PASS
Total: 8/8 PASS
```

回归新增内容包括：Composer 融合统计精确值、报告统计复用、严格 Reader 校验和/通道统计、篡改
manifest 层统计后的稳定 fail-closed 错误，以及连续宿主计时。

对 H-F-08 的 2499 x 623 x 263 PackBits 历史包使用当前严格 Reader 运行三次，耗时分别为
14.912 s、13.999 s、13.617 s，3/3 PASS。该测量用于证明新增层统计核对没有破坏既有包；因系统
缓存和机器负载不同，不与 H-F-08 的 9.700 s 均值直接做性能回归结论。

补充 17 项矩阵为 16/17 PASS；唯一失败是既有
`scene_layer_adapters_unit_tests::legacy_adapter_applies_admitted_instance_transform` 对平移前后局部
layer 字节完全相等的断言。该用例不经过本次 validated evidence、融合统计或 Writer 路径，且相关
Adapter/测试源文件本次未修改，作为独立残留回归记录，不据此改写本卡实现。

## 4. Runtime 发布

`PrepareSliceSoftRuntime.ps1 -DeployOnly` 执行成功。由于运行目录当时存在被占用文件，脚本按既有
保护逻辑采用“保留 output、不可变载荷原位更新”，未删除用户输出。发布后以下四组构建/运行文件
SHA-256 全部一致：模块、Worker、严格 Reader、参考宿主；运行时 self-test 输出
`STAGE14E02_SELF_TEST_PASS spi=1 calls=6`。

## 5. 性能待确认

用户截图中的原始五模型作业为 0.021 mm、263 层、2499 x 623、PackBits，但 Worker 临时请求
在作业结束后按合同清理，当前仓库没有可直接重放的同请求快照。现存 0.038 mm 或不同场景包不能
冒充同输入 before/after。

因此代码与语义 Gate 已完成，但卡状态暂不标为最终 COMPLETE。下一次在参考宿主中以相同五个
模型、实例变换、Profile hash、635 x 600 DPI、0.021 mm、PackBits 运行后，应记录
`layerComposeMs`、`reportBuildMs`、`reportWriteMs`、`outputWriteMs`、Worker 总耗时和宿主总耗时，
再补齐本卡性能实测出口。
