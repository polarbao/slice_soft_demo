# REPORT_14D-08-R1-01 Worker 请求解析与作业身份当前状态

> 更新日期：2026-08-06
>
> 任务状态：`COMPLETE`
>
> 准备门：`PASS`
>
> 下一任务：`14D-08-R1-02 result.json 原子写入与稳定退出映射`

## 1. 本轮目标

实现 `file_contract_v1` 的严格 `request.json` 读取、语义校验与不可变作业身份，作为
DLL 启动 Worker 和独立调试入口共同使用的基础。本任务不接入算法 executor，不创建生产
package，也不宣称三项重能力可执行成功。

## 2. 已完成内容

1. 新增 `slicer_worker_runtime` 私有静态库，保持 Worker 私有实现与公共 SPI 隔离。
2. 新增 `WorkerRequestParser`：
   - 只接受绝对、存在、普通且文件名为 `request.json` 的路径；
   - 拒绝空文件、UTF-8 BOM、损坏 JSON 和非对象根；
   - 严格校验 `file_contract` v1.0、作业身份、三项精确 capability 和有限超时；
   - 按 capability 校验 `scene/profile/output` 或 `input` 必填分支；
   - 保留原始 `scene/profile/input/output` 对象及同 major 的未知可选字段。
3. 新增 `WorkerJobIdentity`：冻结 normalized request、job 目录、`result.json`、
   `result.json.tmp` 和 `cancel.requested` 路径；`jobId` 不参与路径拼接。
4. 新增只读 `WorkerRequestEnvelope`，执行链只能通过 const getter 访问身份和原始业务对象。
5. 新增 Debug/Release 单元测试和运行时边界脚本，证明 parser 不依赖切片、修复、Qt、
   module 或 package writer。

## 3. 关键边界

- 本任务没有修改 `WorkerApplication::HandleSpiRequest()`；命令入口接线属于 R1-03。
- 本任务没有写 `result.json`；原子结果封装属于 R1-02。
- 本任务没有注册任何 fake 或 production executor。
- 未修改 SPI v1、11 个 `pm_*` 导出、15 项能力、TIFF、RGBWSV 或材料语义。
- 解析成功仅表示合同和身份可信，不表示 capability 执行成功。

## 4. 验证结果

Debug 与 Release 均执行：

```text
file_contract_v1_test                                PASS
slicer_stage14d01_worker_shell_contract_test         PASS
slicer_stage14d08_r1_worker_runtime_boundary_test    PASS
stage14d03_worker_contract_unit_tests                PASS
stage14d08_r1_worker_runtime_tests                   PASS
```

两套配置均为 `5/5 PASS`。定向构建目标
`stage14d08_r1_worker_runtime_tests` 与 `slicer_worker` 均通过 `/W4 /WX`。

## 5. 后续准备状态

`14D-08-R1-02` 的输入身份、结果路径、稳定错误分类和原子替换边界已经冻结，准备状态由
`PREPARED` 提升为 `READY`。R1-03 仍等待 R1-02 完成后接线。父任务 14D-08 仍保持
`BLOCKED`，不得因本任务完成而声明真实重能力可用。

```text
14D_08_R1_01_STATUS=COMPLETE
14D_08_R1_02_PREPARATION_GATE=PASS
14D_08_R1_02_STATUS=READY
14D_08_R1_03_STATUS=PREPARED
14D_08_PARENT_GATE=BLOCKED
```
