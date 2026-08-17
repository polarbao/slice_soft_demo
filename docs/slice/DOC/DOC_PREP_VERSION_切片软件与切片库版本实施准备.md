# DOC_PREP_VERSION 切片软件与切片库版本实施准备

> 日期：2026-08-17
> 状态：`PREPARATION_GATE = PASS`
> 对应专项：`VERSION-00..06`
> 上游裁决：`DOC_DECISION_VERSION_切片软件与切片库统一版本治理.md`

## 1. 当前代码事实

| 位置 | 当前状态 | 实施要求 |
|---|---|---|
| `CMakeLists.txt` | `project()` 无 VERSION | 严格读取根 manifest，核心三段映射到 CMake |
| `ModuleInfo.cpp` | 两处硬编码 `0.1.0` | 消费生成头，保持 JSON `.1` 结构不变 |
| `module.json.in` | 硬编码 `0.1.0` | 使用 CMake 派生值 |
| module schemas | `const: 0.1.0` | 保留 SemVer pattern，值由一致性测试约束 |
| Worker discovery | `engineVersion=0.1.0` | 消费切片库实现版本；不改算法身份 |
| Qt 软件 | 无版本展示/查询 | 设置 applicationVersion，增加无副作用查询和 UI 展示 |
| CLI | 无 `--version` | 显式短路，禁止误入默认切片路径 |
| Windows 制品 | VERSIONINFO 为空 | 从 manifest 生成 `.rc` |
| Runtime manifest | 无版本/source snapshot | 引入构建 manifest 并交叉校验 |

现有 `v0.1.0` 是历史稳定标签，不是当前 HEAD 的发布证明。

## 2. 实施结构

```text
version-manifest.json                         源码唯一事实源
cmake/SliceSoftVersion.cmake                 严格解析、SemVer 校验、target 接线
cmake/GenerateSliceSoftBuildManifest.cmake   构建时刷新 revision/dirty/config
cmake/SliceSoftVersion.h.in                  生成只读常量
cmake/SliceSoftVersion.rc.in                 Windows VERSIONINFO 模板
build/.../generated/<config>/                生成头、资源和 build manifest
```

构建时脚本只读取 Git 与 CMake 输入并写 build-tree，不回写源码 manifest。Git 查询失败写
`unknown`。多配置构建产物必须按配置隔离，避免 Debug/Release 互相覆盖。

## 3. 原子任务与文件所有权

| 卡号 | 文件所有权 | 验收重点 |
|---|---|---|
| VERSION-00 | 本决策、准备、任务清单、执行指令、索引 | 文档 Gate |
| VERSION-01 | `version-manifest.json`、`cmake/SliceSoftVersion*`、根 CMake | 单一事实源、fail-closed |
| VERSION-02 | `ModuleInfo.*`、`module.json.in`、Worker、相关 Schema/测试 | 11 导出和 `.1` 结构不变 |
| VERSION-03 | Qt `Main`、小型版本展示辅助、`HostMainWindow`、UI smoke | 软件/库均可见，不可用态真实 |
| VERSION-04 | `.rc.in`、target resource 接线 | EXE/DLL 文件属性一致 |
| VERSION-05 | Runtime/模块打包脚本、构建 manifest | 包内版本与哈希一致 |
| VERSION-06 | 定向测试、Debug/Release、报告和任务状态 | 六方一致与回归收口 |

已有脏工作区中的场景/变换/合同修改不属于本专项。对有既存修改的索引与 UI 文件只做最小
增量，不覆盖用户内容。

## 4. 测试矩阵

### 4.1 静态与配置

- manifest 缺字段、非法 SemVer、组件 ID 漂移、lockstep 版本不一致必须配置失败；
- 源码版本硬编码扫描只允许出现在历史文档/fixture 中；
- 生成头、module info、module.json、Worker discovery 与 manifest 一致；
- Git unavailable/dirty/clean 使用脚本 fixture 验证，未知状态不得伪装。

### 4.2 运行时

- `slicer_ui_host_sim --version` 和 `slicer_cli --version` 返回 0，只输出版本，不创建窗口或切片；
- Qt UI smoke 同时看到软件版本和切片库版本；模块缺失时软件版本仍可见且库为不可用；
- `pm_module_info` 在 `pm_create` 前可调用、多次字节一致；
- Worker `--contract-info` 保持 `file_contract_v1`，仅实现版本改为派生值。

### 4.3 冻结回归

- Stage 14C-05 module info/manifest；
- Stage 14C-06 SPI conformance 和精确 11 导出；
- Stage 14D-03 Worker contract；
- Stage 14E-02 Qt host self-test / missing module；
- Debug/Release `slicesoft_runtime` 构建；
- Runtime/模块包脚本与 SHA-256 校验。

## 5. 停止条件

出现下列任一情况必须停止该卡，不得用兼容性退让掩盖：

- 需要新增/删除公共导出；
- 需要修改 `p0.rgbwsv.2`、RGBWSV 或 Worker 文件合同；
- Module Info/Manifest 必须新增字段才能完成；
- 版本查询触发模块初始化或 Worker；
- Debug/Release 生成物互相覆盖；
- 稳定版本与 dirty/无 tag source 被混淆。

## 6. 准备结论

不需要新增第三方依赖；CMake 自带 JSON 解析、Git、Windows resource compiler、Qt 和现有
JSON 查询通道足以完成。`PREPARATION_GATE = PASS`，可按 VERSION-01..06 顺序实施。

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-17 | v1.0 | 完成代码触点、文件所有权、验证矩阵、停止条件和实施 Gate。 |
