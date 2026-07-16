# TASKS_12F Release 运行环境与切片性能优化任务清单

> 文档状态：R0 COMPLETE / R1-R5 PLANNED / NOT ACTIVE
> 日期：2026-07-16
> 规则：当前唯一主线仍由 docs/codex_task/README 指定；不得自动开始算法优化

## 12F-00 文档准入

状态：DONE

```text
Decision/PRD/DEV/Roadmap/Tasks 已建立；
12B 性能证据和 12C build lane 已纳入；
OpenVDB、RGBWSV 和当前 12D 边界已冻结。
```

## 12F-01 统一 Debug/Release Runtime

状态：DONE

变更：

```text
scripts/PrepareSliceSoftRuntime.ps1；
.vscode/launch.json；
.vscode/tasks.json；
ToolPaths sibling/config-aware resolution；
applicationDir portable resource-root resolution；
场景/Profile、模型/纹理和引用文档部署；
Profile 相对路径发布守门；
.gitignore runtime/；
用户运行环境说明。
```

验收：

```text
Debug NMake x64 build：PASS；
Release NMake x64 build：PASS；
Debug windeployqt：PASS；
Release windeployqt：PASS；
Debug UI self-test：PASS；
Release UI self-test：PASS；
Debug/Release CLI --help：PASS；
Release 场景/Profile 资源发布：30 scenarios PASS；
从非运行包工作目录执行 scenario-registry：PASS；
5 个稳定 Profile、4 份唯一配置 inspect-model：PASS。
```

## 12F-02 Release Benchmark 刷新

状态：TODO / NOT ACTIVE

目标：使用当前代码和 `build-slicesoft/Release` 刷新三真实模型 5 次中位数，输出新的 12F benchmark report。

禁止：不修改算法，不把历史 12B 数字冒充当前结果。

## 12F-03 支撑统计扫描融合

状态：TODO / REQUIRES EXPLICIT START

目标：移除或融合 support generation 后的重复全 volume 统计扫描。

验收：grid/model/support/type totals/hash 完全一致；三模型 Release before/after。

## 12F-04 Bottom Projection Range Candidate

状态：TODO / REQUIRES EXPLICIT START

目标：建立 backend-neutral support column range，不直接替换 production mask。

验收：逐层 support mask diff=0；失败自动保留 legacy materialization。

## 12F-05 Layer Compose 扫描融合

状态：TODO / REQUIRES EXPLICIT START

目标：融合 compose/channel stats，复用 layer buffer。

验收：RGBWSV layer hash、report totals 和 RIP strict 一致。

## 12F-06 Relief Occupancy Provider

状态：TODO / REQUIRES EXPLICIT START

目标：用 provider 包装 ColumnLayerRange，逐步减少完整 model mask 常驻。

验收：mask/support/channel diff=0，peak memory 有实际报告。

## 12F-07 增量缓存

状态：TODO / REQUIRES EXPLICIT START

目标：模型、姿态、DPI、层高和支撑配置 key 下复用 geometry/support intermediate。

验收：cold/warm 输出一致，缓存失效原因可诊断。

## 12F-08 Preview/I/O 解耦

状态：TODO / REQUIRES EXPLICIT START

目标：preview 按需/异步策略，不阻塞核心切片完成状态。

说明：该任务改善 end-to-end，不计入纯切片 KPI。

## 12F-09 最终收口

状态：TODO

```text
三真实模型 Release 性能矩阵；
Debug/Release runtime 回归；
内存、确定性、协议和 strict admission；
REPORT_12F；
决定是否继续 tile/layer parallel。
```
