# DOC_PREP_14B 核心 Facade 与 Base/Engine 分层实施准备

> 文档状态：**PREPARATION COMPLETE / DEVELOPMENT READY**
> 版本：v1.0 ｜ 日期：2026-08-05
> 适用任务：14B-00、14B-01、14B-01A、14B-02、14B-03、14B-03A、14B-04、14B-05、14B-06
> 权威任务入口：`docs/codex_task/current/TASKS_14_切片能力包封装与打印软件集成任务清单.md`

---

## 1. 准备结论

```text
14B_PREPARATION_GATE = PASS
14B_IMPLEMENTATION   = NOT STARTED
FIRST_TASK           = 14B-00
PARALLEL_GUARD_TASK  = 14B-06
```

14A 切片侧合同和实现已收口，14B 可从分层可行性验证开始。14A-03 与
14A-04-R1 的打印侧书面回签仍需跟踪，但不阻塞切片侧 14B-00/01/06；它们阻塞的是
Stage 14 对外合同最终退出，不得被误写为已取得外部确认。

## 2. 目标与范围

### 2.1 目标

1. 建立 Qt-free 的 C++ Facade，隔离 CLI、DLL、Worker 与领域实现。
2. 把当前单一 `slicer_core` 拆为稳定的 `slicer_base` 与仅 Worker 使用的
   `slicer_engine`，形成 `engine -> base` 单向依赖。
3. 复用既有模型、场景、切片和包读取能力，不改变生产行为。
4. 提供真实纹理双视图 ViewData，禁止纹理模型静默降级为灰模。
5. 建立文件行数、依赖方向和新增文件归属门禁，防止分层回退。

### 2.2 非目标

- 不创建 `slicer_module.dll`，该工作属于 14C。
- 不创建或接线 `slicer_worker.exe`，该工作属于 14D。
- 不修改 `apps/slicer_debug_ui/`；14E 使用独立参考宿主。
- 不改变 `p0.rgbwsv.2`、RGBWSV 通道顺序、位深、极性或 TIFF 默认 Writer。
- 不启用 OpenVDB 默认路径，不新增 Qt3D 或新的 vcpkg 依赖。
- 不以目录搬迁为名重写算法；执行纪律为 wrap first、move later、rewrite last。

## 3. 当前代码基线

```text
CMake target          slicer_core STATIC
Facade directory      src/slicer_core/api/ 不存在
Base target           slicer_base 不存在
Engine target         slicer_engine 不存在
Module/Worker         14B 不创建
Qt dependency         slicer_core 必须继续为零
```

高风险切分点：

| 区域 | 风险 | 14B-00 必须回答的问题 |
|---|---|---|
| `model.cpp` / `importers/` | 导入、纹理、几何查询耦合 | `model.import` 能否留在 base；不能则改由 Worker 承载 |
| `geometry/` | 轻量查询与重分析混装 | bbox/基础拓扑/碰撞与 full preflight/repair 的文件级边界 |
| `output/` | Reader 与 Writer 同目录 | 包读取/预览进 base，生产写入进 engine 的依赖切点 |
| `reports/` / `diagnostics/` | 展示读取与生成逻辑混装 | 读取 DTO 与切片期生成器的归属 |
| `pipeline/` | 编排与计算混装 | Facade 只编排，计算仍留 engine 的最小边界 |

## 4. 固定依赖方向

```text
slicer_module.dll (14C) -> slicer_base
slicer_worker.exe (14D) -> slicer_engine -> slicer_base
slicer_cli              -> slicer_core compatibility target -> slicer_engine
tests / debug UI        -> slicer_core compatibility target -> slicer_engine

禁止：slicer_base -> slicer_engine
禁止：slicer_base / slicer_engine -> Qt
禁止：slicer_module.dll -> slicer_engine
```

14B-01A 迁移期允许保留名为 `slicer_core` 的 `INTERFACE` 兼容 target，统一转发到
`slicer_engine`，避免一次修改全部消费者。新 DLL 不得使用该兼容 target，只能显式链接
`slicer_base`。迁移完成后是否删除兼容 target 由 14C/14F 再评估，不在 14B 强制删除。

## 5. Facade 合同

### 5.1 通用规则

```text
命名空间       slicer_core::api
错误           ApiResult<T>；不得让异常越过 Facade
取消           耗时操作必须接受 ICancelToken
数据类型       STL/项目 DTO；禁止 Qt 与 ABI 结构
错误码         直接使用 PM-SLICER-*，禁止建立第二套映射
路径           由调用方传入；Facade 不决定用户目录
线程           接口本身不承诺 UI 线程安全；调用方负责调度
```

### 5.2 14B 能力归属

| Facade/Provider | 任务 | 目标 target | 关键出口 |
|---|---|---|---|
| 通用 DTO、`ApiResult`、`ICancelToken` | 14B-01 | base | Qt-free、错误稳定、取消可注入 |
| `ModelFacade` | 14B-02 | 由 14B-00 决定 | 导入/元数据/释放行为与既有 CLI 一致 |
| `PackageQueryFacade` | 14B-02 | base | verify/summary/layer/report/preview 读取 |
| `SceneFacade` | 14B-03 | base | revision、变换、碰撞、越界沿用现有权威求值 |
| `TexturedSceneViewDataProvider` | 14B-03A | base | top/three_d 真实纹理、多外观、独立 identity、预算降级 |
| `SliceFacade` | 14B-04 | engine | 提交、进度、取消；生产 TIFF 逐字节不变 |

## 6. 原子任务执行顺序

```text
14B-00  分层可行性验证
  -> 14B-01  Facade/DTO/ICancelToken
  -> 14B-01A base/engine 两库落地
       -> 14B-02 Model + PackageQuery
       -> 14B-03 Scene
       -> 14B-04 Slice
  -> 14B-03A TexturedSceneViewDataProvider（依赖 14B-02/03）
  -> 14B-05 CLI 改走 Facade（依赖 14B-02/03/04）

14B-06 行数与结构门禁可与 14B-00 并行，但必须在新增 api/module/worker 源文件前生效。
```

每张任务卡独立提交。不得把目录大搬迁、Facade 行为改造和 CLI 接线压进同一个提交。

## 7. 每张任务卡的实施合同

### 7.1 14B-00 分层可行性验证

产出必须包含：

1. 当前 CMake source 列表和 target 依赖图。
2. `model.cpp`、`geometry/`、`output/`、`reports/`、`diagnostics/` 的 include/symbol 证据。
3. 文件级 base/engine 归属表及跨界引用清单。
4. `model.import = base | worker` 的唯一结论和理由。
5. 最小编译探针或等价链接证据；不得只凭目录名判断。

停止条件：若出现不可消除的 base -> engine 循环，先记录最小接口抽取方案，不得通过把
整个 engine 链进 base 来“解决”。

### 7.2 14B-01 / 14B-01A

- 先定义接口与 DTO，再调整构建图。
- 两库骨架阶段保持全部既有消费者可构建。
- 迁移源文件时禁止同一 `.cpp` 同时编入两库，防止重复符号。
- OpenVDB、生产 Writer、切片/修复算法不得进入 base。
- `slicer_base` 的公开头必须可由不链接 Qt 的最小测试目标编译。

### 7.3 14B-02 / 03 / 04

- 只包装既有服务，不复制业务规则。
- `SceneFacade` 的碰撞/越界结果必须与 `scene/`、`layout/` 单测一致。
- `SliceFacade` 只在 engine 内实现；取消令牌至少覆盖阶段边界和逐层循环。
- 生产包验证包含 manifest、逐层六通道 checksum 与 RIP strict。

### 7.4 14B-03A

正例至少覆盖：

- OBJ + MTL + PNG；
- 3MF Texture2D checker；
- 两个不同 appearance 的同场景模型；
- top `surfacePreview` 与 three_d UV/material/texture 引用闭合。

负例至少覆盖：纹理文件缺失、解码失败、无 UV、材质引用越界、预算不足。声明纹理的模型
不得成功返回灰模；预算不足只能按合同返回可识别降级或稳定错误。

### 7.5 14B-05 / 06

- CLI 改走 Facade 后，命令行、退出码、进度行和产物保持兼容。
- 行数门禁采用显式白名单；既有 `MainWindow.cpp`、`UiSmokeTestRunner.cpp` 的豁免必须带
  “14E-05 完成后删除”到期条件。
- 新增 `api/`、`slicer_module/`、`slicer_worker/` 与参考宿主文件从第一天起不得进白名单。

## 8. 验证矩阵

每个 C++/CMake 任务至少运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tests/contracts/ValidateStage14BPreparation.py
git diff --check
```

涉及生产写包或 CLI 接线时增加：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
# 使用任务卡指定的真实/fixture 场景执行 RIP strict 和逐层 checksum 对比
```

14B-03A 增加 `D14-B-09..12`；14B-05 增加现有 CLI 黄金回归。Release 全量验证时间过长时，
可以先跑目标测试，但任务完成前必须补齐任务卡要求的完整门禁。

## 9. 回滚与停止条件

| 情况 | 处理 |
|---|---|
| 分层后生产 TIFF 漂移 | 回滚当前原子迁移；禁止更新 golden 掩盖漂移 |
| base 需要 engine 符号 | 抽取窄接口，或把对应能力改为 Worker；禁止反向链接 |
| model import 无法低风险留 base | 采用 14B-00 退路，更新承载表与 `syncCapabilities[]` 输入 |
| 新 Facade 复制业务规则 | 停止并改为调用既有权威服务 |
| 纹理失败被灰模掩盖 | fail-closed；不得放宽 14A-04-R1 |
| 外部回签仍缺失 | 可继续内部 14B；不得宣告 Stage 14 合同最终完成 |

## 10. 14B 准备度矩阵

| 项目 | 状态 | 证据 |
|---|---|---|
| 需求/设计/验收 | PASS | PRD_14 / DEV_14 / DEMO_14 |
| 任务依赖与顺序 | PASS | TASKS_14 §2、§7 |
| 能力 DTO / 三车道 / 取消合同 | PASS | `contracts/` 与 14A 合同测试 |
| ViewData / UI 显示合同 | PASS | DTO 1.2、`slicer_ui_view_spec.json` |
| 分层原则和退路 | PASS | INT_10/16/17、本文 §3..9 |
| CMake 迁移策略 | PASS | 本文 §4、§7.2 |
| 自动准备门禁 | PASS | `ValidateStage14BPreparation.py` |
| 外部打印侧回签 | PENDING | 不阻塞内部 14B，阻塞最终对外退出 |

**最终结论：14B 的准备工作已完成。下一开发任务为 14B-00；14B-06 可并行，但不得借此
提前进入 14C/14D/14E。**

