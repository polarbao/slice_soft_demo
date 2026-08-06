# DOC_PREP_14C-05 模块自述与部署清单实施准备

> 日期：2026-08-06
> 状态：`PREPARATION_GATE = PASS`
> 对应任务：`14C-05`

## 1. 前置与边界

14C-01/02/03/04 已提供精确 11 导出、缓冲三态、句柄/TLS 及 15 项能力的实际承载。
本卡只实现模块自述和部署清单，不新增导出、能力、Worker 执行、TIFF 或 UI 行为。

初始模块版本以当前仓库发布标签 `v0.1.0` 为唯一输入，机器值固定为 `0.1.0`。模块身份固定为
`slicer`，SPI 固定为 1，生产协议固定为 `p0.rgbwsv.2`。

## 2. 两份合同

### 2.1 运行时 `pm_module_info`

必须通过 14C-02 `WriteOut()` 返回单一 UTF-8 JSON，对象至少包含：

```text
schema/id/name/version/spi/runtime/buildConfig/provides/produces/capabilities
```

- `schema = slicesoft.module_info.1`；
- `runtime = MSVC-x64-MD | MSVC-x64-MDd`；
- `buildConfig = Release | Debug`，首版不声明未验证的 RelWithDebInfo；
- `provides[]` 精确等于冻结 15 项能力且唯一；
- `produces[]` 只声明 `{contract:p0.rgbwsv.2, kind:package}`；
- `capabilities.maxConcurrentJobs = 1`、`cancelLatencyMs = 2000`；
- `syncCapabilities[]` 精确等于 14C-04 的 13 项；
- `workerCapabilities[]` 使用冻结能力 ID，允许 `geometry.preflight` 同时出现，并由请求
  `mode=fast|full` 决定承载，不得另造 `.fast`/`.full` 外部能力 ID。

该函数必须可在 `pm_create` 前调用，多次调用字节完全一致，不触发持久化或 Worker。

### 2.2 部署 `module.json`

必须包含：

```text
schema/id/name/dll/spi/version/runtime/buildConfig/provides/consumes/
produces/profileKeys/subprocess
```

- `schema = slicesoft.module_manifest.1`；
- `dll = slicer_module.dll`，`subprocess.exe = slicer_worker.exe`；
- `subprocess.protocol = file_contract_v1`；
- 路径必须是相对文件名，禁止绝对路径和 `..`；
- `profileKeys=[]` 表示宿主传递完整冻结 Profile 对象，首版不声明键级拆分所有权；
- 不引入历史 `delayLoad` 字段，加载策略属于宿主；
- Debug/Release 分别生成自己的 manifest，禁止一个文件谎报另一运行时。

## 3. Schema 与一致性

新增两个严格 Draft 2020-12 Schema：

```text
contracts/slicer_module_info.schema.json
contracts/slicer_module_manifest.schema.json
```

均使用 `additionalProperties:false`、SemVer、唯一能力、严格相对文件名和固定 SPI。测试必须交叉
校验两份对象的 `id/version/spi/runtime/buildConfig/provides/produces` 一致，并覆盖 runtime、
buildConfig、能力集合、版本、绝对路径和未知字段篡改负例。

## 4. 文件所有权

```text
src/slicer_module/ModuleInfo.h/.cpp
src/slicer_module/module.json.in
contracts/slicer_module_*schema.json
tests/stage14c_05/*
src/slicer_module/Exports.cpp       （14C-04 提交后串行修改）
CMakeLists.txt                      （主代理串行集成）
```

新头文件不超过 200 行，新实现文件不超过 500 行；不得链接 Qt/PrintSDK。

## 5. 验收门禁

- Debug/Release 构建 `slicer_module` 与 14C-05 测试；
- C-SPI-01/02/03：SPI、合法自述、运行时/构建类型篡改拒绝；
- 缓冲探测/差一/完整写入继续复用 14C-02；
- `dumpbin /EXPORTS` 仍精确 11 项，`/DEPENDENTS` 不含 Qt/PrintSDK；
- JSON Schema 正负例和 module-info/manifest 交叉一致性通过；
- 14C-04 同步能力真实运行时测试保持通过。
