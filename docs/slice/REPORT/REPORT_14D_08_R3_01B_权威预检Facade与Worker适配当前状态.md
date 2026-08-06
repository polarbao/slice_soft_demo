# REPORT_14D-08-R3-01B 权威预检 Facade 与 Worker 适配当前状态

> 日期：2026-08-06
>
> 状态：`COMPLETE`

## 1. 交付结果

本任务把 `14D-08-R3-01A` 的进程内全场景预检服务接入了稳定 Facade 和真实 Worker 路径。

- full 请求固定携带 committed scene、effective Profile、两份 hash、revision 和显式 targetMode；
- Facade 使用完整 Profile，仅替换每个 `ModelSource` 的路径和格式，复用生产 importer 重建几何；
- Worker 在 job 目录原子物化 scene/Profile，禁止扫描仓库配置或读取 UI 状态；
- topology、碰撞、越界等 blocker 返回成功执行后的 `admission=blocked`；
- 资源缺失、身份 stale、Profile 不匹配、取消和异常返回稳定 PM-SLICER 失败；
- 结果保留 scene、实例、transform、topology、bbox、issues、collisions 与 out-of-bounds 证据；
- 该能力不创建 RGBWSV Package、TIFF、manifest 或 preview。

## 2. 主要文件

```text
src/slicer_core/api/ProfileIdentity.*
src/slicer_core/api/SliceDtos.h
src/slicer_core/engine/ProductionPreflightFullFacadeFactory.*
apps/slicer_worker/preflight/WorkerPreflightExecutor.*
apps/slicer_worker/WorkerApplication.cpp
tests/stage14d_08_r3/WorkerPreflightExecutorTests.cpp
```

## 3. 已执行验证

```text
Debug  stage14d08_r3_worker_preflight_tests          PASS
Debug  stage14d08_r3_scene_preflight_tests           PASS
Release stage14d08_r3_worker_preflight_tests         PASS
Release slicer_worker --contract-info                PASS
ValidateCapabilityDtos.py                            PASS（15 capabilities）
ValidateFileContract.py                              PASS
git diff --check                                     PASS
```

## 4. 边界

- 未修改 SPI v1、11 个 `pm_*` 导出、15 项能力或 `p0.rgbwsv.2`；
- 未实现 repair Writer/Facade/Worker，该范围属于 `14D-08-R3-02B`；
- 未实现真实 `slice.rgbwsv` Worker executor，该范围属于 `14D-08-R2-02`；
- 未进入 14E UI 宿主开发。

## 5. 下一步

`14D-08-R2-02` 已具备准备文档，应先完成真实 Slice Worker executor，再继续 repair 与安全发布收口。
