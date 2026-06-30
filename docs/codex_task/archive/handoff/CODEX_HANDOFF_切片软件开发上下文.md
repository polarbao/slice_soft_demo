# CODEX_HANDOFF_切片软件开发上下文

## 1. 当前项目目标

本仓库是 UV / 彩色 / 多材料 3D 打印切片软件的工程化验证仓库。项目已经从 P0 Demo 演进到 OpenVDB 表面壳层纹理实验链路的生产准入前诊断阶段。

当前最新阶段：

```text
当前最新阶段：09B-R3 已完成
当前工作分支基线：spike/09B-R3-shell-production-readiness
下一阶段：09P OpenVDB 表面壳层纹理实验生产管线接入
```

09B-R3 已完成：

```text
narrow-phase triangle-triangle self-intersection
ValidationErrorCode / WarningCode
repeat/wrap texture fixture
Windows process peak working set
真实模型 topology production admission 策略
```

## 2. 当前生产安全边界

09B-R3 的产物只用于 OpenVDB 表面壳层纹理实验链路的诊断、报告、preview 和 benchmark。

必须明确：

```text
09B-R3 没有接入 production slicer_cli。
09B-R3 没有写 production RGBWSV TIFF。
09B-R3 没有修改 p0.rgbwsv.2。
09B-R3 没有修改 RGBWSV 通道顺序、uint8 位深和 black_is_print 极性。
真实 OBJ/3MF 当前仍不得直接视为 production-safe。
```

下一阶段 09P-R1 只做：

```text
experimental path
feature flag
diagnostic/report
service abstraction
```

09P-R1 不默认启用 OpenVDB，不替代 legacy production path，不直接写真实 OBJ / 3MF 的 production RGBWSV TIFF。

## 3. 当前固定协议

RGBWSV production package 协议仍保持冻结：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

后续 OpenVDB、纹理壳层、支撑、白墨、光油等能力都必须映射到该协议，不得隐式修改协议。

## 4. 和现有打印软件的关系

现有 UV 打印软件已经具备部分 RIP 后能力，例如：

```text
通道化处理
板卡控制
运动控制
打印任务执行
设备诊断
维护服务
```

本仓库当前仍负责上游切片、诊断、预览、报告和测试，不直接处理板卡、运动控制和打印执行。

## 5. Codex 接手顺序

新会话接手时优先阅读：

```text
AGENTS.md
docs/slicer/CODEX_TASKS_09P_R1.md
docs/slicer/REPORT_09B_R3_壳层纹理生产准入前诊断策略收口当前状态.md
docs/slicer/PRD_MASTER_SliceSoft_正式切片软件产品需求总览.md
docs/slicer/DEV_MASTER_SliceSoft_正式切片软件总体架构与实现路线.md
```

执行 09P-R1 时必须遵守：

```text
每次只执行用户明确指定的一个 Task。
开始前确认 git status --short 干净。
完成后运行 Task 指定验证命令。
验证通过后只提交当前 Task 相关文件。
不要自动执行下一个 Task。
不要 push，除非用户明确要求。
```

## 6. 下一步

当前下一步是按 `docs/slicer/CODEX_TASKS_09P_R1.md` 逐个执行 09P-R1 Task。

Task 01 只修正文档中的当前阶段基线；Task 02 才新增 09P 文档骨架；后续代码和 pipeline 接入必须等对应 Task 被用户明确指定后再执行。
