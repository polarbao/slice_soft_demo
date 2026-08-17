# CODEX_PROMPT_VERSION 切片软件与切片库版本治理执行指令

## 1. 开工入口

依次读取：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/build-and-test.md
docs/slice/DOC/DOC_DECISION_VERSION_切片软件与切片库统一版本治理.md
docs/slice/DOC/DOC_PREP_VERSION_切片软件与切片库版本实施准备.md
docs/codex_task/current/TASKS_VERSION_切片软件与切片库版本治理任务清单.md
```

执行前运行 `git status --short`。工作区已有用户修改时必须避让，不得回滚、覆盖或顺手整理。

## 2. 固定执行规则

1. 只从根 `version-manifest.json` 读取软件/切片库实现版本。
2. 生成物只写 build-tree 或 staging；禁止构建时回写源码事实源。
3. Git 不可用显式 `unknown`，dirty 不得伪装 clean。
4. 不新增第三方依赖，不新增公共导出，不修改冻结协议。
5. 每完成一张卡，立即更新任务表状态、日期和实际验证；失败保持未完成。
6. Debug/Release 证据分别记录，未运行不得写 PASS。
7. 最终执行 `git diff --check`，并报告与本专项无关的 dirty state。

## 3. 必跑验证

```powershell
cmake --build <build-dir> --config Debug --target slicesoft_runtime
cmake --build <build-dir> --config Release --target slicesoft_runtime
ctest --test-dir <build-dir> -C Debug --output-on-failure -R "version|stage14c05|stage14c06|stage14d03|stage14e02"
ctest --test-dir <build-dir> -C Release --output-on-failure -R "version|stage14c05|stage14c06|stage14d03|stage14e02"
```

随后按当前构建环境执行 Runtime/模块包定向脚本。若 Runtime 目录被正在运行的软件占用，
不得终止未知用户进程；记录阻塞并使用不覆盖现有 Runtime 的临时验证目录。

## 4. 收口输出

输出必须包含：文档裁决、实际改动、软件版本、切片库版本、完整构建标识、UI 展示位置、
实际构建/测试/打包结果、未完成或外部未验证边界。不得把参考项目规则描述为 SliceSoft 已实现事实。
