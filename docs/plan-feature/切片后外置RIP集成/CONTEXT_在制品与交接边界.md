# CONTEXT：在制品与交接边界

> 采集日期：2026-09-22
> 用途：**让下一个动这条线的人知道哪些文件正被别人改着，不要覆盖。**

## 1. 工作树里有 9 个未提交改动，全部属于 RIPFLOW-D-10

`slice_soft_demo` 是**多方共用同一个工作树**。截至采集时刻，下列 9 个文件带着
未提交改动，改动时间戳 2026-09-21 01:19，内容对应 **RIPFLOW-D-10 RGBWSVT 输入适配**：

| 文件 | 改动量 | 作用 |
| --- | --- | --- |
| `src/rip_integration/RipInputValidator.cpp` | +13 −4 | 输入准入从只认六通道扩为协议与通道布局**成对**校验 |
| `src/rip_integration/RipInputValidator.h` | +2 | 同上的接口面 |
| `apps/slicer_ui_host_sim/HostRipJobController.cpp` | +28 −7 | 宿主侧同一判据的另一份实现 |
| `apps/slicer_ui_host_sim/HostRipJobController.h` | +1 | 同上 |
| `tests/rip_integration/RipIntegrationTests.cpp` | +33 −2 | 六、七通道正例与跨协议错配负例 |
| `tests/ripflow/ValidateRipflowWiring.py` | +10 −1 | 接线校验脚本同步 |
| `rip_module/README.md` | +6 | 模块侧说明 |
| `docs/codex_task/current/TASKS_RIPFLOW_…任务清单.md` | +32 −2 | D-10 卡与修订记录（v2.5 → v2.6） |
| `docs/user_guides/SLICE_RIPFLOW_切片后RIP设置与迁移说明.md` | +10 −4 | 用户手册同步 |

合计 **9 个文件，+116 −19**。

## 2. 交接规则

**不要动上面这 9 个文件。** 具体地：

| 动作 | 允许？ | 原因 |
| --- | --- | --- |
| 读它们 | ✅ | 本文的事实就是这么来的 |
| 编辑它们 | ❌ | 会与未提交改动冲突，且对方无从察觉 |
| `git add` 它们 | ❌ | 等于替对方提交一份自己没验证过的改动 |
| `git add -A` / `git add -- <整个目录>` | ❌❌ | 本仓工作树常态带着多条未跟踪路径，一扫就会把模型资产和别人的在制品一起带进提交 |
| `git checkout HEAD -- <其中任一个>` | ❌ | **直接销毁**，且不可恢复——这些改动只存在于盘上 |
| `git reset --hard` | ❌ | 同上，一次毁 9 个 |

这条规则也约束"顺手统一行尾"这类批量操作：
[文件行尾统一](../文件行尾统一/README.md) 专项把这 9 个文件**整体排除在转换范围之外**，
正是因为转换会改动每一行，与对方的未提交改动逐行冲突。

## 3. D-10 改了什么（供后续接手者理解，不替代真源）

在 D-10 之前，RIP 输入准入把协议**硬编码**成六通道 `p0.rgbwsv.2`。之后改为成对准入：

| 协议 | 允许的通道布局 |
| --- | --- |
| `p0.rgbwsv.2` | `R,G,B,W,S,V` |
| `p0.rgbwsvt.1` | `R,G,B,W,S,V,T` |

人工目录由首层固定 6 或 7 通道，后续层必须保持一致；
unsigned 8bit、contiguous、stripped、尺寸闭合与路径安全规则不变。
协议、manifest 通道顺序与真实 TIFF 通道数三者不一致时仍 fail-closed。

**边界**：只放开供方已经支持的七通道输入，**不改** T 通道生成语义、RIP 输出校验、
单色/彩色发布规则，也不升级外部验收状态。

## 4. 一个值得记住的归因

D-10 的实际结果里有一句关键的：

> 该结果证明原报错来自切片软件适配层的六通道硬编码，不是 0920 RIP 库拒绝 RGBWSVT。

也就是说，**报错文案指向了错误的一侧**。使用者看到的是"RIP 不接受这个包"，
真因却在自己这边的准入判据。这类"报错指不到真因"的情况在本仓不是第一次——
另有一处同类：宿主自带一份核心校验的副本，同一条配置判据有多处独立硬编码，
只改切片库会让工艺在 CLI 能跑、UI 却认不出来。

> 排查 RIP 相关报错时，**先确认判据在哪一侧、有几份**，再去怀疑供方库。

## 5. 采集方法（任何人都可复算）

```bash
git status --porcelain
```

```bash
git diff --stat -- apps/slicer_ui_host_sim src/rip_integration tests/rip_integration tests/ripflow rip_module
```

```bash
git diff -- "docs/codex_task/current/TASKS_RIPFLOW_切片后外置RIP集成专项任务清单.md"
```

第三条最有用：在制品的**意图**写在任务卡的 diff 里，比读代码 diff 快得多。
