# REPORT 13F-R0/R1 多模型交互、取消与切片性能稳定化当前状态

> 报告日期：2026-07-30
> 当前结论：R0 完成；R1-06 Reality 单模型异常耗时已修正并验证；R1-01..05 待实施

## 1. 已完成

```text
批量导入期间按 SceneDocument 当前实例数增量展示；
模型选择优先使用缓存 surface preview alpha 命中；
移除 SetCurrentInstance 前的冗余几何深拷贝；
ProcessRunner 支持 terminate + 1 秒后 kill；
控制器增加 Cancelling，真实进程退出前保持任务占用；
取消后禁止自动加载 package，进程结束后恢复 UI 操作；
自动定向启用时，即使无需旋转也把有效模型归一化到构建平台；
Reality 单模型 Release 核心基准、完整写包和 RIP 严格校验完成。
```

## 2. 根因结论

### 爱神后两个模型

后两个模型约为 11.4 万面和 28.3 万面，加载和俯视纹理预览构建明显慢于前三个。
旧 UI 又冻结了批次可见前缀，因此表现为“前 3 个正常、后 2 个不显示”。本次修正
展示冻结问题；高面数模型导入本身仍需在 R1 做分项计时和预览精度优化。

### 选择卡顿

旧路径在 UI 线程扫描全部投影三角形，并在实例切换时重复拷贝大型几何。R0 已把
常规命中改为缓存 alpha 查询，并去除一次冗余拷贝。剩余一次工作副本加载可在后续
以不可变共享几何进一步收口。

### 取消失效

旧控制状态先结束、外部进程后结束，两个生命周期不一致。R0 以 `Cancelling` 串联
取消请求和真实退出，并给 Windows 控制台进程增加强制退出兜底。

### Reality 耗时

仓库内五个 Reality OBJ 已纳入可追溯测试资产。首个模型仅 `8150` 个顶点、
`14774` 个三角面，物理尺寸约为 `25.977 x 12.105 x 6.975 mm`，因此高面数不是
一分钟耗时的原因。

根因是源 OBJ 已经趴放，但整体悬在 `Z=303.731..310.706 mm`。旧逻辑认为高度
`6.9752 mm` 已满足 `maxHeightMm=9`，直接保留源 `identity`，而切片器使用
`ceil(maxZ/layerThickness)` 计算层数，按 `0.038 mm` 层厚会得到约 `8177` 层。
修正前单模型核心诊断运行超过 `184 s` 仍未结束，进程工作集约 `6.49 GB`，随后按
测试边界终止，没有生成可用基准结果。

修正方式不是旋转模型，而是在自动定向启用时将最终 `identity` 候选也下移到
`minZ=0`。显式关闭自动定向时仍保留源坐标，避免改变该模式的既有合同。

## 3. Reality Release 复测

| 项目 | 修正后结果 |
|---|---:|
| 有效包围盒 | `Z=0..6.9752 mm` |
| Grid | `650 x 286 x 184` |
| 核心计算 | `1348.5702 ms` |
| 峰值工作集 | `285294592 bytes` |
| 完整写包总耗时 | `3719.056 ms` |
| TIFF 保存 | `820.021 ms` |
| Preview 保存 | `564.010 ms` |
| TIFF 数量 | `184` |
| Preview 数量 | `38` |
| RIP Reader | `PASS` |

相同核心基准下，爱神参考模型为 `1544.8859 ms`。Reality 修正后没有表现出异常慢；
本次问题与 XY 排版位置无关，也不是最终图片保存主导。完整写包中输出保存约
`1574.365 ms`，其余为模型加载、支撑、层计算、合成和报告构建。

## 4. 验证结果

```text
production_slice_route_process_tests：PASS；
scene_document_unit_tests：PASS；
scene_batch_import_controller_unit_tests：PASS；
scene_slice_action_controller_unit_tests：PASS；
UI Smoke scene-batch-import-three：PASS；
UI Smoke multi-model-list：PASS；
UI Smoke scene-slice-cancel：PASS；
auto_orient_unit_tests：PASS；
model_preflight_service_unit_tests：PASS；
Reality Release 完整写包：PASS；
Reality package RIP Reader strict：PASS；
git diff --check：无空白错误，仅存在仓库既有 LF/CRLF 提示。
```

## 5. 当前完成度与剩余风险

```text
13F-R0：4/4 COMPLETE；
13F-R1：1/6 COMPLETE，13F 整体尚未完成；
尚未手工完成五个爱神真实模型的整批导入验收；
第三个爱神模型 MTL 引用 RGB.png，但目录实际贴图名不同，属于独立资源告警；
高面数 OBJ 的 parse/texture/preview/hash 分项耗时尚未上报；
联合切片仍按实例串行执行，尚未做缓存或受控并行；
Reality 其余四个模型未运行切片，继续遵守每次最多一个的限制；
本次未验证硬件打印，只验证 Release 包、TIFF 协议和 RIP Reader。
```
