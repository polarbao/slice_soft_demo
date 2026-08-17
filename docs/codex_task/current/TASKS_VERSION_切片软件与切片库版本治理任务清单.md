# TASKS_VERSION 切片软件与切片库版本治理任务清单

> 文档状态：**ACTIVE — IMPLEMENTED / REGRESSION PARTIAL**
> 版本：v1.1 ｜ 日期：2026-08-17 ｜ 激活日期：2026-08-17
> 定位：独立插入专项，不占 Stage 编号，不改变当前 Stage 16 主线结论
> 决策：`docs/slice/DOC/DOC_DECISION_VERSION_切片软件与切片库统一版本治理.md`
> 准备：`docs/slice/DOC/DOC_PREP_VERSION_切片软件与切片库版本实施准备.md`

## 1. 总体要求

```text
SemVer 2.0.0；软件和切片库双组件、当前 lockstep 发布
首个受控开发基线 0.2.0-dev
根 version-manifest.json 是实现版本唯一事实源
完整构建标识必须显式 revision/dirty/config/runtime/triplet/关键变体
UI、API、Worker、资源、构建清单、Runtime 清单必须同源
查询无初始化/切片/设备副作用
```

冻结边界：不改 SPI v1、精确 11 导出、15 项能力、`file_contract_v1`、
`p0.rgbwsv.2`、RGBWSV 顺序/位深/极性和 Module Info/Manifest `.1` 字段结构。

## 2. 任务状态

| 卡号 | 任务 | 状态 | 完成日期 | 实际验证 |
|---|---|---|---|---|
| VERSION-00 | 参考规则审计、组件/基线裁决、准备文档、执行要求与索引 | **COMPLETE** | 2026-08-17 | 5 份参考文档与当前代码审计；决策、准备、任务、执行指令和索引已建立 |
| VERSION-01 | 根 manifest、严格 CMake 解析、生成头与构建身份 | **COMPLETE** | 2026-08-17 | `slicesoft_version_contract_test` 与 clean/dirty/unknown/非法配置 fixture 在 Debug/Release 均 PASS |
| VERSION-02 | 模块自述、module.json、Worker discovery 同源 | **COMPLETE** | 2026-08-17 | Module Info、Manifest、Worker 合同与 SPI 精确 11 导出定向测试在 Debug/Release 均 PASS |
| VERSION-03 | Qt/CLI 版本查询与软件 UI 展示 | **COMPLETE** | 2026-08-17 | UI/CLI `--version`、Qt smoke、默认模块、缺模块不可用态和导入 UI smoke 均 PASS |
| VERSION-04 | Windows EXE/DLL VERSIONINFO | **COMPLETE** | 2026-08-17 | UI、CLI、Module、Worker 的 `FileVersion`、`ProductVersion`、`PrivateBuild` 与构建清单一致 |
| VERSION-05 | build manifest、Runtime/模块包版本快照与哈希 | **COMPLETE** | 2026-08-17 | 模块 Package/Test Gate PASS；当前 HEAD 的 Visual Studio Release clean build 与 Runtime 部署 PASS；staging 四制品属性、Worker/Module 冻结合同/内部版本与 SHA-256 Gate PASS |
| VERSION-06 | Debug/Release 回归、发布检查和专项收口 | **PARTIAL / BLOCKED_BY_EXISTING_14E02_BOUNDARY** | - | VERSION 定向 10/10 在 Debug/Release 均 PASS；完整定向集各 10/11，既存 `HostModelImportWorkflow.cpp` 516 行超过 14E-02 的 500 行门槛 |

## 3. 原子卡要求

### VERSION-00 文档与准入

- 区分实现版本、构建身份、SPI/协议/Schema/算法/依赖版本；
- 裁决组件 ID、lockstep、首个基线、tag 与 dirty/unknown 规则；
- 明确 UI、发布、回滚和冻结边界；
- 不修改 `docs/reference` 输入资料。

### VERSION-01 单一事实源与构建身份

- 新增机器可读 manifest，严格校验必填字段、ID、SemVer 和 lockstep；
- CMake `project(VERSION)` 只使用软件核心三段；预发布和 build metadata 单独保存；
- 生成头/manifest 写 build-tree，多配置隔离；Git 每次构建刷新，失败写 unknown；
- 不按时间、提交数或目录名推导版本。

### VERSION-02 切片库查询一致性

- 复用 `pm_module_info.version`，不新增导出；
- `module.json.version` 和 Worker discovery 同源；
- Schema 保留 `.1` 结构并由测试交叉校验事实源；
- `legacy-scene-v1` 继续作为算法身份。

### VERSION-03 软件展示

- UI 标题显示软件短版本；顶部状态显示软件/切片库/SPI；
- 诊断页显示软件完整构建标识和原模块自述；
- 模块不可用时显示真实 unavailable；
- Qt/CLI `--version` 必须提前短路且无副作用；增加 UI/CLI 自动化测试。

### VERSION-04 Windows 资源

- 软件、模块、Worker、CLI 的 FileVersion/ProductVersion/ProductName 从事实源生成；
- Windows 四段数字只映射 SemVer 核心三段并以第四段 0 补齐；字符串版本保留预发布；
- Debug/Release 不伪造不同实现版本。

### VERSION-05 打包与清单

- 每个配置生成 build manifest；Runtime manifest 引用同一版本快照；
- 模块包包含源版本/构建版本清单并进入 checksums；
- 关键构建变体取有效配置，不硬编码依赖静态/动态形态；
- 打包测试交叉核对 module info、module.json、Windows 属性和清单。

### VERSION-06 收口

- Debug/Release 构建及定向 Stage 14 Gate 全绿；
- 发布清单记录 clean/tag/SHA-256/回滚要求；
- 新增状态报告，回填本表状态、完成日期、实际命令与结果；
- 不把未执行的外部打印/RIP 验收写成 PASS。

## 4. 执行顺序

```text
VERSION-00 -> VERSION-01 -> VERSION-02 -> VERSION-03
                         \-> VERSION-04 -> VERSION-05 -> VERSION-06
```

用户已授权本插入专项在文档准备完成后进入功能开发；本次可从 `VERSION-01` 连续执行至
`VERSION-06`，但每张卡仍须在本表回填真实结果。遇到停止条件时必须保留未完成状态。

## 5. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-17 | v1.1 | VERSION-01..05 实现并验证完成；VERSION-06 因既存 14E-02 文件行数边界失败保持 PARTIAL，新增专项状态报告。 |
| 2026-08-17 | v1.0 | 建立 VERSION-00..06 插入任务；VERSION-00 文档与准备完成，VERSION-01 READY。 |
