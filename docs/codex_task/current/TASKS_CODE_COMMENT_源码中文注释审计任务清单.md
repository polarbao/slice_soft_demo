# TASKS_CODE_COMMENT 源码中文注释审计任务清单

> 文档状态：**IMPLEMENTATION COMPLETE / STATIC PASS / BUILD PASS**
> 版本：v0.3 ｜ 日期：2026-08-14
> 定位：用户授权插入的独立代码质量专项，不占 Stage 编号，不改变产品协议或运行行为。
> 状态唯一真源：本任务卡。

---

## 1. 任务边界

本专项以当前分支与 `product/legacy-slicer` 的共同祖先
`b5fc0fb3fcb7d13c3c554e01c81b05fe4cd9d461` 为审计基线，只检查相对该基线新增或重命名后仍存在的
`.c/.h/.cpp/.hpp` 文件。当前清单共 366 个文件：`apps` 148 个、`contracts` 1 个、`src` 144 个、
`tests` 73 个。

不修改被删除的旧产品线文件，不修改第三方源码、生成文件、协议字面量、错误码、类型名、函数名和
Doxygen 指令。当前工作区已有未提交实现修改，审计必须保留这些修改并只调整注释。

## 2. 注释规则

1. 人类可读的说明性注释使用中文；必要的 API、ABI、SPI、RGBWSV、Qt、Worker 等技术名词可保留英文。
2. 公共 API 和稳定模块边界使用简洁 Doxygen，`@brief/@param/@return` 标签保持原样，说明文字使用中文。
3. 不添加逐句复述代码的注释；优先解释非显然原因、失败策略、线程或所有权、协议限制和数据不变量。
4. 拆分到多个源文件的模块应在文件级或关键入口说明本文件职责、与相邻文件的边界以及不得越权的行为。
5. 注释修改不得改变可执行代码、公开合同、测试期望和构建依赖。

## 3. 原子任务

### CODE-COMMENT-01 新增源码中文注释审计

**状态：IMPLEMENTATION COMPLETE / STATIC PASS / BUILD PASS（2026-08-14）**

**输入：** §1 冻结的 366 个新增/重命名源码文件。

**工作项：**

- 将英文自然语言注释改为中文，保留必要技术标识；
- 复核 Doxygen、非显然关键逻辑和错误边界注释；
- 复核多文件模块的职责与边界说明；
- 对保留的英文注释给出可审计原因；
- 仅做注释变更，并验证代码 diff 无行为变化。

**完成 Gate：**

- 新增源码范围完成逐文件或自动化辅助审计；
- `git diff --check` 通过；
- 注释修改涉及的目标完成编译，定向测试通过；
- 本卡记录实际修改文件数、保留项、验证命令与结果，并追加修订记录。

**实际结果：**

- 已审计 §1 冻结的 366 个文件，在 187 个文件中完成注释中文化、Doxygen 术语修正或职责/边界补充；
- 英文自然语言注释解析扫描为 0，生硬译法禁用词扫描为 0；
- 保留 `@brief/@param/@return/@copydoc`、namespace 尾注释、API/ABI/SPI/Profile/Worker/ViewData、
  协议字面量、错误码、符号名和头文件保护宏；
- `git diff --check` 返回 0，仅有仓库既有 LF 转 CRLF 提示；
- Debug 编译早期曾因 `cmake -> MSBuild -> cl.exe` 无输出而三次超时；环境恢复后重新执行
  `cmake --build build-slicesoft/main --config Release --parallel 8`，完整 Release 构建通过；
- `hostflow_hb07_package_review` 与 `hostflow_hb07_result_ui_smoke` 定向测试 2/2 通过；
- 结论记录于 `docs/slice/REPORT/REPORT_CODE_COMMENT_源码中文注释审计当前状态.md`。

## 4. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-14 | v0.3 | 环境恢复后完成完整 Release 构建，定向 Host Package Review 测试 2/2 通过；状态由 BUILD BLOCKED 更新为 BUILD PASS。 |
| 2026-08-14 | v0.2 | 完成 366 文件审计并修订 187 个文件；英文自然语言与禁用译法扫描均为 0，diff check PASS；Debug 编译因 MSBuild/cl 无输出挂起而 BLOCKED，未声称构建或测试通过。 |
| 2026-08-14 | v0.1 | 用户授权插入独立中文注释审计任务；冻结比较基线、366 文件范围、中文/Doxygen/关键逻辑/多文件职责规则与完成 Gate，CODE-COMMENT-01 开始执行。 |
