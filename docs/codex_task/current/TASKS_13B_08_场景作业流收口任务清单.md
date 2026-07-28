# TASKS 13B-08 场景作业流收口任务清单

> 文档状态：APPROVED / IN PROGRESS
> 版本：v1.2
> 日期：2026-07-28
> 当前入口：13B-08-02 READY

## 1. 目标

把“多模型可查看、可排版但不能从当前场景切片”的断点收口为完整作业流：

```text
批量导入 -> 场景排版/变换 -> 场景预检 -> 切片当前场景
-> 单一 RGBWSV Package -> 自动加载 TIFF 生产预览。
```

## 2. 原子任务

### 13B-08-01 批量导入队列

状态：`COMPLETE / GATE PASS（2026-07-28）`

范围：

```text
QFileDialog 多文件选择；
1..remaining 容量预判，超过 22 整体阻断；
串行加载队列和 batchId/generation；
成功项保留、失败项继续、用户取消可解释；
批次结束后只执行一次规则排版；
模型列表“添加”和主入口复用同一控制器；
单元测试和 batch-import UI Smoke。
```

停止点：不得顺带实现场景切片 CLI、重新布局整个 MainWindow 或修改生产协议。

### 13B-08-02 场景生产服务与 CLI

状态：`READY / PREP COMPLETE`

范围：

```text
无 Qt MultiModelProductionService；
显式 slicer_cli --scene-config 入口；
effective config 回读、identity/revision/buildVolume/mode Gate；
复用 13B-05..07 编排器、合成器和单 Package writer；
稳定错误、退出码、正负向 CLI/核心单测；
不把 multi_model_scene_matrix 作为产品入口。
```

### 13B-08-03 Qt 当前场景主切片动作

状态：`PREPARED / SEQUENCE WAIT 13B-08-02`

范围：

```text
始终可见的“切片当前场景”主按钮；
SceneActionBar 和 SceneSliceActionController；
冻结快照、预检、进程、验证、回载状态机；
阻断原因和进度摘要；
成功后自动加载单一 Package；
stale/cancel/no-fallback UI Smoke。
```

### 13B-08-04 真实模型矩阵与阶段收口

状态：`PREPARED / SEQUENCE WAIT 13B-08-03`

范围：

```text
1/3/11/12/22 实例；
OBJ/3MF、纹理、部分导入失败、碰撞/越界和容量负向；
Debug/Release、RIP strict、Quick CI；
用户操作说明和 REPORT_13B_08；
设备输入未关闭时只宣称 functional PASS，不宣称 production GO。
```

## 3. 固定边界

```text
Qt 不进入 slicer_core；
不重新启用旧单模型入口消费当前 SceneDocument；
不静默截断批量文件；
不静默跳过失败项；
不从 Global 回退 Legacy；
不修改 p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print；
不做自动 nesting、跨模型联合支撑或 mixed-profile；
不在 13B-08 做 13D 全窗口重排。
```

## 4. 每任务验证

最低公共 Gate：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

Qt 任务增加对应 `--ui-smoke-test`；写包任务增加 `rip_reader_test --summary`；阶段收口增加 Release
定向矩阵。实际未运行的命令不得写为 PASS。

## 5. 顺序 Gate

```text
13B-08-01 PASS -> 13B-08-02 READY；
13B-08-02 PASS -> 13B-08-03 READY；
13B-08-03 PASS -> 13B-08-04 READY；
13B-08-04 functional PASS -> 13C-03 恢复为唯一推荐入口。
```
