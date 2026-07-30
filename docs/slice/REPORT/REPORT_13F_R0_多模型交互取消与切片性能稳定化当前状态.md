# REPORT 13F-R0 多模型交互、取消与切片性能稳定化当前状态

> 报告日期：2026-07-29
> 当前结论：R0 代码和自动化验证完成，R1 性能可观测性待实施

## 1. 已完成

```text
批量导入期间按 SceneDocument 当前实例数增量展示；
模型选择优先使用缓存 surface preview alpha 命中；
移除 SetCurrentInstance 前的冗余几何深拷贝；
ProcessRunner 支持 terminate + 1 秒后 kill；
控制器增加 Cancelling，真实进程退出前保持任务占用；
取消后禁止自动加载 package，进程结束后恢复 UI 操作。
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

仓库内 `model/obj/reality` 当前为空，未执行该组真实切片。截图中的外部模型只有约
1.3 万至 1.5 万面，排除“高面数直接导致一分钟”的解释。联合切片当前逐实例串行
执行完整核心计算，日志显示每个实例约增加 30 秒。下一步必须在相同有效 Profile
和 Grid 下记录单实例核心计时后再决定 raster 复用或有限并行。

## 3. 验证结果

```text
production_slice_route_process_tests：PASS；
scene_document_unit_tests：PASS；
scene_batch_import_controller_unit_tests：PASS；
scene_slice_action_controller_unit_tests：PASS；
UI Smoke scene-batch-import-three：PASS；
UI Smoke multi-model-list：PASS；
UI Smoke scene-slice-cancel：PASS；
git diff --check：无空白错误，仅存在仓库既有 LF/CRLF 提示。
```

## 4. 剩余风险

```text
尚未手工完成五个爱神真实模型的整批导入验收；
尚未对 Reality 模型执行切片，遵守每次最多一个的限制；
第三个爱神模型 MTL 引用 RGB.png，但目录实际贴图名不同，属于独立资源告警；
高面数 OBJ 的 parse/texture/preview/hash 分项耗时尚未上报；
联合切片仍按实例串行执行，尚未做缓存或受控并行。
```
