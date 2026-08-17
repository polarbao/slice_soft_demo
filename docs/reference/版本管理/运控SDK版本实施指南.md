# 运控 SDK 版本实施指南

## 1. 当前映射

| 能力 | 实现位置 |
| --- | --- |
| 机器可读事实源 | `/version-manifest.json` |
| manifest 校验与 CMake 变量 | `/cmake/MotionSdkVersion.cmake` |
| 构建期 Git 状态刷新 | `/cmake/GenerateMotionSdkSourceState.cmake` |
| 配置差异化构建 manifest | `/cmake/GenerateMotionSdkBuildManifest.cmake` |
| 公开静态版本宏 | 构建目录 `include/motioncontrolsdk/motioncontrolsdk_version.h` |
| 运行时构建来源头 | 构建目录 `include/motioncontrolsdk/motioncontrolsdk_source_state.h` |
| 本地构建 manifest | `motionControlSDK-package/manifest/<Config>/motion-sdk-build-manifest.json` |
| CPack 发布 manifest | ZIP 内 `manifest/motion-sdk-build-manifest.json` |
| 结构化查询 | `IMotionControlSDK::MC_GetVersionInfo()` |
| 简化查询 | `IMotionControlSDK::MC_GetVersion()` |
| Windows DLL 属性 | `src/sdk/motionControlSDK.rc.in` |
| 变更记录 | `/CHANGELOG.md` |

## 2. manifest 字段

| 字段 | 当前值 | 说明 |
| --- | --- | --- |
| `schemaVersion` | `1` | manifest 自身结构版本 |
| `component.id` | `motion-runtime` | 永久稳定组件 ID |
| `component.releasePolicy` | `independent` | SDK DLL 独立发布 |
| `component.versionScheme` | `semver-2.0.0` | 版本规则 |
| `component.version` | `1.2.0` | 不含 prerelease/build 的核心版本 |
| `component.preRelease` | 空字符串 | 当前为稳定版本；开发和候选阶段分别使用 `dev`、`rc.N` 等值 |
| `component.contractVersion` | `motion-sdk.api.v1.2` | 当前公开 API 契约 |
| `component.status` | `available` | 组件可用状态 |
| `source.revision` | `unknown` | 源 manifest 不缓存 Git 状态，构建时覆盖到构建 manifest |
| `compatibility.contracts` | API v1.2、协议 v1.1 | 当前明确支持的契约集合 |

必需字段缺失、格式非法、策略不是 `independent` 时，CMake 配置必须失败，不能回退到伪版本。

## 3. 构建期来源状态

每次构建先由 `motion_sdk_source_state` 读取源码状态，再由 `motion_sdk_build_manifest` 按当前
CMake 配置生成制品身份：

```powershell
git rev-parse --short=12 HEAD
git status --porcelain --untracked-files=normal
```

生成文件位于 build tree，并通过 `copy_if_different` 避免内容不变时触发无意义重编译。

| 场景 | `fullBuildVersion` |
| --- | --- |
| Release、第三方静态、源码干净 | `1.2.0+g<revision>.release.thirdparty-static` |
| Debug、第三方静态、源码有修改 | `1.2.0+g<revision>.dirty.debug.thirdparty-static` |
| Release、第三方动态、源码干净 | `1.2.0+g<revision>.release.thirdparty-dynamic` |
| Debug 且源码状态不可读取 | `1.2.0+source.unknown.debug.<dependency-variant>` |

Debug 和 Release 共享同一个 `implementationVersion`，因为构建优化级别不改变 API 的 SemVer
优先级；二者通过 build metadata、`buildConfiguration` 和 `runtimeLibrary` 明确区分。Windows
资源中的 `ProductVersion` 保持实现版本，`FileVersion` 字符串分别增加 `-debug` / `-release`，
并记录 `BuildConfiguration` 与 `RuntimeLibrary`。

当前自动登记的差异维度为构建配置、MSVC 运行库、vcpkg triplet 和第三方依赖链接方式。
以后新增影响 ABI、协议或运行行为的 CMake 选项时，必须同时扩展
`GenerateMotionSdkBuildManifest.cmake`、`MotionSdkVersionInfo`、版本测试和本指南，不能只增加
一个未登记的编译宏。

## 4. 查询接口语义

`MC_GetVersion()` 返回实现版本，适合简单展示。`MC_GetVersionInfo()` 返回：

```cpp
MotionSdkVersionInfo version = sdk->MC_GetVersionInfo();
```

字段包括组件 ID、显示名、发布策略、实现版本、完整构建标识、API 契约、状态、source revision、
dirty 标志、构建配置、MSVC 运行库、vcpkg triplet、第三方依赖链接方式和来源是否已知。
两个查询均只读取编译期常量，可以在 `MC_Init()` 前调用。

`MC_GetBuildTime()` 为兼容和诊断保留，但不得用于版本比较、发布判断或制品兼容判断。

## 5. 发布包要求

发布包至少包含：

```text
bin/Release/motionControlSDK.dll
lib/shared/Release/motionControlSDK.lib
include/motioncontrolsdk/*.h
manifest/version-manifest.json
manifest/motion-sdk-build-manifest.json
docs/PACKAGE_README.md
```

源码 manifest 表示发布意图，构建 manifest 表示具体制品身份。离开开发机后应仍能通过
`MC_GetVersionInfo()` 和构建 manifest 查询同一版本快照。

本地多配置 build tree 会同时保留 `manifest/Debug/` 和 `manifest/Release/`，避免后一次构建覆盖
前一次构建身份。CPack 生成单配置 ZIP 时，只把当前打包配置对应的构建 manifest 放入 ZIP 根
`manifest/`。ZIP 文件名同样包含配置，例如：

```text
motionControlSDK-1.2.0-debug-win64.zip
motionControlSDK-1.2.0-release-win64.zip
```

## 6. 修改版本的正确步骤

1. 根据公开变化判断 MAJOR/MINOR/PATCH。
2. 只修改 `version-manifest.json` 中的核心版本和 prerelease。
3. 更新 `CHANGELOG.md`。
4. 删除旧 build tree 不是必需步骤；重新配置后 CMake 应自动读取新 manifest。
5. 执行 Release 构建、测试、发布包和 consumer 验证。
6. 稳定发布时创建 annotated tag，并核对 tag 与 manifest。
