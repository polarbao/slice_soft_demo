# DOC PREP HOSTFLOW H-B-04 Profile 目录与能力查询准备

> 状态：**PREPARATION COMPLETE / IMPLEMENTATION BLOCKED BY HQ-08**
> 日期：2026-08-07
> 任务：HOSTFLOW H-B-04
> 范围：Profile 来源、模块能力查询、生产安全等级和参考宿主选择入口。

## 1. 准备结论

H-B-04 的 UI、主干 A/B 基线和模块能力查询入口均已定位，但当前冻结公开合同**没有 Profile
目录载体**，因此尚不能按卡面“经 ABI 查询可用 Profile 与能力集”进入编码。

已确认事实：

1. `pm_module_info` 仅返回 15 项能力、运行时、输出合同和并发/取消信息；没有 Profile 列表、
   材料能力标签或生产安全等级。
2. `contracts/slicer_module_info.schema.json` 设置 `additionalProperties=false`，宿主不能假设存在
   未声明的 `profiles` 字段。
3. 15 项能力中没有 `profile.list`，新增该能力会违反冻结的“15 项能力”边界。
4. 主干 `ScenarioRegistry` 直接读取 `samples/scenarios/slicer_scenarios.json`；HOSTFLOW 执行指令
   已明确禁止参考宿主沿用这条内部资产路径。
5. 主干 `ProductionModeCatalog` 是编译期宿主目录，但依赖 `slicer_core` 类型，不能直接复制到
   只允许公开 SPI 的参考宿主。

因此，任务清单中“H-B-04 无前置、可立即开发”的旧判断不完整。准备工作已完成，但必须先关闭
HQ-08 的 Profile 所有权与发现协议决策。

## 2. 候选方案

| 方案 | Profile 来源 | 模块能力来源 | 合同影响 | 判断 |
|---|---|---|---|---|
| A. 宿主 Profile 目录 | PrintApp/宿主注入 `IProfileCatalog`；参考宿主使用公开 fixture provider | `pm_module_info.provides/capabilities` | 不改 SPI/DTO/module_info | **推荐** |
| B. `pm_module_info.profileCatalog` | 模块自述只读 Profile 描述 | 同一 `pm_module_info` | 修改冻结 module_info schema 与返回值 | 可行，但须受控修订和回签 |
| C. 新增 `profile.list` | 新能力请求 | 新能力请求 | 第 16 项能力，违反冻结边界 | 拒绝 |
| D. 读取 `slicer_scenarios.json` | 仓库内部 JSON | 本地推断 | 打印侧不可移植，违反执行指令 | 拒绝 |

### 2.1 推荐方案 A

Profile 是打印应用面向设备、材料和用户的业务选择；切片模块负责声明自己支持哪些能力并校验
提交的有效 Profile。参考宿主应定义不依赖 `slicer_core` 的 `HostProfileDescriptor` 与
`IHostProfileCatalog`，由宿主提供候选 Profile，再与 `pm_module_info` 的能力集合做交集：

```text
宿主 Profile 候选
  + Profile.requiredModuleCapabilities
  + pm_module_info.provides
  -> 可用 / 不可用 + 缺失能力 + productionSafety 展示
```

这样既不读取内部场景 JSON，也不把打印软件的 Profile 生命周期错误地下沉给切片 DLL。它需要
对 H-B-04 卡面“Profile 列表必须经 ABI 查”做一次受控语义修订：**Profile 目录归宿主，模块能力
经 ABI 查询，可用性由二者求交得到。**

### 2.2 备选方案 B

若产品明确要求 Profile 由能力包交付，则可在 `slicesoft.module_info.1` 增加只读
`profileCatalog`。该方案不新增导出和能力，但会修改：

- `contracts/slicer_module_info.schema.json`；
- `src/slicer_module/ModuleInfo.cpp`；
- 14C-05 module_info 正负例；
- 可移植清单和打印侧 ACK 基线。

由于 `additionalProperties=false` 且 Stage 14 已冻结，该方案未经用户授权不得实施。

## 3. H-B-04 实现计划（HQ-08 关闭后）

1. 增加不依赖 `slicer_core` 的 Profile 描述 DTO 和目录 Provider。
2. 结构化解析 `ModuleClient::ModuleInfo()`，禁止字符串包含式推断能力。
3. 根据 required capabilities 计算 Profile 可用状态、缺失能力和生产安全等级。
4. 在参考宿主顶部/右侧增加 Profile 选择与能力摘要；不可用项禁用并显示原因。
5. Profile 切换只更新宿主 session 草稿，不触发切片；H-B-05 接管参数与有效 Profile 生成。
6. 增加缺失能力、重复 id、未知安全等级和空目录负例。
7. 执行 Debug/Release、模块边界和主干 `ProductionModePanel` A/B smoke。

## 4. 文件所有权

预期 H-B-04 修改范围：

```text
apps/slicer_ui_host_sim/HostProfileCatalog.*
apps/slicer_ui_host_sim/HostProfilePanel.*
apps/slicer_ui_host_sim/ModuleClient.*
apps/slicer_ui_host_sim/HostMainWindow*.cpp
tests/hostflow/HostProfilePanelTests.cpp
```

若选择方案 B，另需修改 `contracts/` 与 `src/slicer_module/ModuleInfo.*`，必须单独建立受控合同
修订任务，不得偷渡进 H-B-04 UI 提交。

## 5. 停止条件

- HQ-08 未关闭前不得实现 H-B-04。
- 不得新增第 16 项能力或第 12 个导出。
- 不得读取 `samples/scenarios/slicer_scenarios.json`。
- 不得复制依赖 `slicer_core` 的 `ProductionModeCatalog` 到参考宿主。
- 不得把候选 Profile 显示为可用，除非其能力要求已与模块自述求交验证。
- H-B-05..08 依赖 H-B-04 的稳定选择结果，当前不得越过该 Gate 并行编码。
