# LOGDUMP G 产品合入与复用交付准备

日期：2026-09-15。用户已明确要求完成切片侧日志/DUMP 专项并同步到 `product/packaged-slicer`，保持后续打印集成可复用。此次授权是实际本地合入，不再仅作合入判断；不包含推送、删除分支、修改原 P0FIX 工作树或建设打印端 P23 全业务。

## Implementation Plan

- Problem Type：已有功能产品合入、清洁源码构建、部署验证和交付状态收口。
- Layer(s) Involved：DLL 可选 C 日志接口、宿主诊断基础设施、部署与源码 SDK；不改变切片算法和 RIP 规则。
- Official Documents：LOGDUMP A00 接口定案、E02 SDK 交付与专项任务清单。
- Historical Documents：F 收口报告的 67 项 62 通过/5 失败是上轮证据，本轮不冒充重跑。
- AI Workspace Evidence：日志树干净，HEAD `1d29d97c`；product 为 `d28b6451`；原树 P0FIX 存在持续新增的用户修改，全部保留。
- Current Code Reality：日志分支已包含 product，8 个增量提交可快进合入；已有回调、宿主 sink、CMake targets、源码清单和真实打印后端组件验收。
- Current State：切片侧实现和任务提交完成，但尚未移动 product。使用手册/SDK 说明仍有 F01 已修复的 helper 缺口旧描述，需要纠正。
- Target State：在隔离工作树切换 product 并快进合入，记录清洁提交的构建身份；提供完整运行包与可直接消费的源码 SDK、生命周期说明。
- Historical State：F 阶段仅将 product 合入专项，不等于专项合入 product；旧 dirty 构建包不是本次产品构建。
- Pending Confirmation：E01B 等打印端几何 DLL loader；正式 GUI/干净机器/其他 ACP/物理打印仍未验。完整 Stage14 门禁的既有失败不通过本次合入放宽。
- Risk Points：只集成诊断功能，不携带 P0FIX 未提交或其他分支变更；回调注销失败须保活，打印宿主保留全局日志/转储所有权。默认日志不是无丢失承诺。
- Files To Change：本准备、专项状态、入口索引、SDK README、用户说明和产品交付报告；不增加新依赖或改接口。
- Verification Plan：提交前 diff 检查；快进后清洁源码 Release 全目标构建；诊断/转储/SDK/SPI/Worker/RIP/宿主相关回归，逐项报告既有红灯；新目录成套部署、中文路径受限 PATH 启动落盘自检；重导出 SDK 并核验清单。原运行包不覆盖。

## 本轮任务

1. LD-G01：前置复核、明确授权及复用文档更新。
2. LD-G02：本地 product 快进合入；原 P0FIX 树保留，功能分支不删除，远端不推送。
3. LD-G03：产品源码构建与独立运行包验证、SDK 交付及任务状态收口。

切片侧专项可完成，E01B 作为后续打印产品集成依赖继续显式保留，不改写为 COMPLETE。源码复用入口为 `sdk/diagnostics/README.md`，接口为 `contracts/slicer_logging.h`；打印适配器已在打印隔离分支交付，不重复复制打印私有实现。
