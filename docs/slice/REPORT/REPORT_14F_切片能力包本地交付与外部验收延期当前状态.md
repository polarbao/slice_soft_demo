# REPORT_14F 切片能力包本地交付与外部验收延期当前状态

> 阶段状态：**SLICER PACKAGE READY / INTERFACES FROZEN / EXTERNAL ACCEPTANCE DEFERRED**  
> 切片侧状态：**COMPLETE**  
> 外部状态：**NOT RUN / DEFERRED BY USER**  
> 日期：2026-08-07

## 1. 收口结论

Stage 14F 已完成本仓库内的能力包打包、接口冻结、M1 本地接收、单模型 S1 联调、
S2 C1-C7 合同门禁和汇总收口。Release 闭环通过，冻结合同的 SHA-256 已写入机器证据。

该结论允许把当前切片能力包交付给打印侧继续集成，但**不等于生产发布授权**。
打印侧真实装载、目标 RIP 输出、`ChannelSplitter`、极性映射、干净机、并行打印、
长稳与实物工艺均未执行，不得写成 PASS。

## 2. 14F 任务结果

| 任务 | 本地结果 | 外部边界 |
|---|---|---|
| 14F-01 | Release `modules/slicer/` 打包、依赖闭包、哈希、NOTICE、包内纯 C 宿主闭环 PASS | 独立干净机未跑 |
| 14F-02 | M1 handoff、装载、15 能力、自检和缺 DLL 负例 PASS；接口冻结 | 打印侧 ModuleRegistry/进程证据延期 |
| 14F-03 | 公开 ABI 单模型 import→transform→Worker slice→S1 strict；正例 1/1、负例 7/7 PASS | 打印宿主 M2 延期 |
| 14F-04 | S2 C1-C7 机器合同；grayBits 正例 2/2、负例 7/7、CTest PASS | 目标 RIP、ChannelSplitter、极性映射延期 |
| 14F-05 | 重新生成能力包和 handoff，串联 14F-01..04 六步本地门禁并固化 12 项合同哈希 | 外部验收整体延期 |

## 3. 冻结面

以下内容在 Stage 14 结束时保持冻结：

1. `PM_SPI_VERSION=1`，精确 11 个 `pm_*` 导出。
2. 15 项能力，`slicer_capability_dtos.json` v1.4。
3. 三车道合同 v1.1；正常 Commit 不追加 snapshot。
4. Worker `file_contract_v1`、模块部署清单和退出码。
5. S1 `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`、0 打印、255 不打印。
6. S2 路径 D、W/S/V 量化上限、白区真实 W 材料、逐层多通道 TIFF 和 6:1 混合。
7. UI ViewData 与 `slicer_ui_view_spec.json` v1.0；不改变生产 TIFF。

任何破坏性调整必须新建受控修订，重新执行 ABI、Worker、S1、S2 与交付门禁。

## 4. 14F-05 实际验证

```text
Run14F05StageClosureGate.ps1:
  14F-01 package preparation       PASS
  14F-02 handoff preparation       PASS
  14F-01 isolated package gate     PASS
  14F-02 M1 local intake gate      PASS
  14F-03 single model S1 gate      PASS
  14F-04 S2 local contract gate    PASS
  frozen contract hashes           12/12

ctest -C Release -R stage14f05_local_closure_gate:
  1/1 PASS
```

机器证据：
`build-slicesoft/main/stage14f05_evidence/Release/stage14f05_closure.json`。

## 5. 外部验收恢复条件

未来恢复打印侧/RIP 联调时，应在不改写本报告历史结论的前提下补充：

1. 打印侧 M1 模块注册和进程模块证据。
2. 打印宿主 M2 单模型与多模型真实流程。
3. 目标 RIP 的 `rip_%06d.tif`、W/S/V 上限和白区语义证据。
4. RIP 与打印软件双边极性映射签字及负向样例。
5. 干净机、取消、并行打印、长稳、内存和实物工艺验证。

完成上述证据前，Stage 14 只能使用本报告顶部的三段状态，不得升级为
`EXTERNAL_ACCEPTED` 或 `PRODUCTION_READY`。
