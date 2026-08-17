# DOC_DECISION_VERSION 切片软件与切片库统一版本治理

> 日期：2026-08-17
> 状态：`APPROVED / INSERTED SUPPLEMENT`
> 适用对象：封装版切片软件 `slicer_ui_host_sim.exe`、切片库交付组件 `slicer_module.dll + slicer_worker.exe`
> 参考输入：`docs/reference/版本管理/*`，仅复用通用版本治理规则，不继承参考项目的组件名、分支模型或发布结论

## 1. 问题与证据

当前仓库没有统一的软件版本事实源。`0.1.0` 分散硬编码在模块自述、部署清单、Worker、Schema
和测试中；Qt 软件、CLI、Windows 文件属性和 Runtime 清单没有完整版本身份。

仓库已有 annotated tag `v0.1.0`，但当前封装版分支在该标签之后已经包含 Stage 14 能力包、
新版宿主等兼容新增。继续把新制品标成稳定 `0.1.0` 会让两个不同制品共享同一稳定身份。

## 2. 决策

### 2.1 版本对象必须分离

下列对象不得互相替代或联动升级：

| 对象 | 本专项规则 |
|---|---|
| 软件实现版本 | SemVer；由根 manifest 派生 |
| 切片库实现版本 | SemVer；由根 manifest 派生 |
| 完整构建标识 | 实现版本 + revision/dirty/config/runtime/triplet/关键变体 |
| 模块 SPI | 继续固定 `PM_SPI_VERSION=1` |
| Worker 文件合同 | 继续固定 `file_contract_v1` |
| 生产包协议 | 继续固定 `p0.rgbwsv.2` |
| Module Info / Manifest Schema | 继续使用 `.1` 结构，本专项只替换既有 `version` 值来源 |
| Profile、Workspace、Scene DTO Schema | 各自独立，不随 SemVer 改动 |
| 切片算法身份 | `legacy-scene-v1` 等继续表达算法实现，不冒充库 SemVer |
| 第三方版本 | 记录在构建/Runtime 清单，不并入实现版本 |

### 2.2 组件边界与发布策略

- 软件稳定组件 ID：`slicesoft-app`，当前制品为 `slicer_ui_host_sim.exe`。
- 切片库稳定组件 ID：沿用冻结模块 ID `slicer`；交付边界包含
  `slicer_module.dll`、同包 `slicer_worker.exe` 和 `module.json`。
- `slicer_base`、`slicer_engine`、`slicer_core` 是内部 target，本专项不为其建立独立对外 SemVer。
- 当前采用 `lockstep`：软件与切片库共享一次仓库级发布节奏，但仍保留两个独立版本字段，
  以便将来通过新裁决拆分发布。
- lockstep 期间稳定 tag 为 `vMAJOR.MINOR.PATCH`，预发布 tag 为
  `vMAJOR.MINOR.PATCH-rc.N`。稳定 tag 必须 annotated 且对应 clean source。

### 2.3 首个受控开发基线

首个统一事实源版本定为：

```text
软件实现版本：0.2.0-dev
切片库实现版本：0.2.0-dev
发布状态：development
```

理由：这是 `v0.1.0` 后的兼容功能新增，应升 MINOR；使用 `-dev` 明确当前不是稳定发布，
不把未打 tag 的开发制品伪装成 `0.2.0` 稳定版。

### 2.4 单一事实源与派生物

根 `version-manifest.json` 是实现版本唯一事实源。以下内容必须从它派生，禁止重新硬编码：

```text
CMake project/version variables
构建目录生成头
构建目录 build manifest
Windows VERSIONINFO
pm_module_info.version
module.json.version
slicer_worker --contract-info.engineVersion
slicer_cli / slicer_ui_host_sim 版本查询
Qt 软件标题与版本详情
runtime_manifest.json 版本快照
```

Schema 只约束 SemVer 形状和冻结结构；“值是否等于事实源”由交叉一致性测试负责。

### 2.5 完整构建标识

完整构建标识格式为：

```text
<implementation-version>+
<revision12>.<clean|dirty|unknown>.<debug|release>.
<runtime>.<triplet>.tiff-<backend>.openvdb-<on|off>
```

实际输出必须为一行合法 SemVer。Git revision 取 12 位；Git 不可用时必须显式写
`unknown`，不得伪装为 `0.0.0` 或 clean。dirty 允许开发构建，但稳定发布 Gate 必须拒绝。
构建配置、CRT、triplet、TIFF backend 和 OpenVDB 状态来自有效构建配置，不从目录名猜测。

版本不得从提交数量、构建时间、目录名、文件修改时间或发布次数推导。`generatedAt` 可作为
审计时间存在于 Runtime 清单，但不是版本组成部分。

## 3. 软件展示

- 窗口标题只显示软件短版本，避免堆叠诊断字段。
- 顶部状态显示软件版本、已加载切片库版本和 SPI；模块不可用时显示 `不可用`，不得显示
  假版本。
- “模块诊断”页增加软件完整构建标识，并保留原模块自述与自检报告。
- `slicer_ui_host_sim --version`、`slicer_cli --version` 必须无 GUI、无切片、无模块初始化副作用。
- 模块版本继续通过既有 `pm_module_info` 查询，不新增第 12 个导出。

## 4. 冻结边界

本专项不得改变：

```text
PM_SPI_VERSION=1
精确 11 个 pm_* 导出
冻结 15 项能力及其顺序
p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print
file_contract_v1
Module Info / module.json 的 .1 字段结构
Worker 结果中的切片算法身份语义
```

版本查询不得执行 `pm_create`、启动 Worker、读取模型、切片、写包或连接任何设备。

## 5. 发布 Gate 与回滚

稳定发布至少满足：源 manifest、生成头、模块自述、`module.json`、Worker discovery、
Qt/CLI 查询、Windows 文件属性、build manifest 和 Runtime manifest 一致；Debug/Release
分别生成自己的构建身份；Release source clean；tag 与实现版本一致；包内文件 SHA-256 完整。

回滚以完整 Runtime/模块包为单位，不允许只替换 DLL 或只修改 manifest。版本治理失败时回滚
生成/展示接线，不回滚或修改冻结生产协议。

## 6. 明确不采纳的参考规则

- 不复制参考项目的 `motion-runtime`、`MC_GetVersion*` 或硬件初始化流程。
- 不在本专项引入 `main/develop/test/release/hotfix` 分支改造。
- 不把 CPack 强加到现有 `PrepareSliceSoftRuntime.ps1` / `PackageSlicerModule.ps1` 主路径。
- 不使用缺 PATCH 位的 tag，也不使用提交数自动生成 SemVer。

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-17 | v1.0 | 建立双组件 lockstep 版本治理，裁决 `0.2.0-dev` 首个受控基线、单一事实源、展示方式、发布 Gate 与冻结边界。 |
