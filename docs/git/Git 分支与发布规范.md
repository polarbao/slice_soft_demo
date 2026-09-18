# Git 分支与发布规范

> **本文是 `Git 版本发布流程 + 测试分支规划.md`（2026-08-05 初版）的修订版。**
> 初版描述的是一套教科书式 git-flow，但**它描述的仓库不是这个仓库**——照它第一步
> `git checkout develop` 就会失败，因为本仓（含远端）**没有任何 develop 分支**。
> 修订依据是 2026-09-18 对仓库实际状态的逐条实测，每处更正都标注了证据。
> 初版保留在同目录，供对照；**以本文为准**。

---

## 一、实际分支模型（按实测，不是按理想）

**初版说核心是 `main` + `develop`。实测：`develop` 本地与远端各 0 条，`main` 落后真正的产品线 481 条。**
本仓实际是「双产品线 + agent 工作分支」模型：

| 分支 | 作用 | 实际状态（2026-09-18） |
|---|---|---|
| `product/packaged-slicer` | **真正的产品线**，封装宿主 + 能力模块拓扑 | 与 origin 同步，发布基线 |
| `product/legacy-slicer` | 并行 legacy 线，手工运行时 | 领先 origin 2 条；有独立工作树且含未提交改动 |
| `main` | 仓库名义默认分支（`origin/HEAD` 指向它） | **停在 2026-08-05，落后 product 481 条** |
| `codex/feature-<专项>-<描述>` | agent 工作分支 | 用完合入 product 后删除 |
| `feature/<描述>` | 早期人工分支 | origin 上尚存 2 条历史遗留 |
| `archive/<原因>-<日期>` | 归档快照 | origin 上 2 条 + 同名标签 |

> ⚠ **一处需要裁定的不一致**：`main` 是 `origin/HEAD` 指向的默认分支，却落后产品线 481 条。
> 新来的人会从 `main` 起步并拿到 8 月 5 日的代码。三个选项：
> ① 把 `main` 快进到 product 并以它为产品线；② 把 `origin/HEAD` 改指 `product/packaged-slicer`；
> ③ 明确 `main` 只是历史入口并在 README 写清。**这不是技术决策，未裁定前本文不擅自改。**

### 为什么不引入 develop

初版的四类临时分支（feature/test/release/hotfix）建立在 develop 之上。本仓的实际情况是
**单人 + AI agent 协作、无 CI**（F-16 用户 2026-08-10 裁决暂缓），而且 `gh` CLI 在本环境不可用，
所以初版「禁止直接提交、只能通过 PR 合并、需至少 1 人评审」**当前无法执行**。
强行照搬只会得到一条空转的 develop 和一堆没人走的 PR 流程。

**现状下等效的质量闸门是验证而非评审**——见第三节。

---

## 二、agent 分支约定（claude / codex 新建分支必须遵守）

### 2.1 命名

```
codex/feature-<专项slug>-<短描述>      # codex 开的工作分支
claude/feature-<专项slug>-<短描述>     # claude 开的工作分支
```

- `<专项slug>` 取 `AGENTS.md`「各专项状态」里的小写 slug（如 `p0fix`、`logdump`、`ripflow`）。
  **没有对应专项就先在 `AGENTS.md` 里立一条**——分支不该比专项先存在。
- `<短描述>` 用连字符英文小写，说清这条分支要解决什么，不用阶段号。
- 实例：`codex/feature-p0fix-contract-robustness`。

### 2.2 从哪里拉

**一律从 `product/packaged-slicer` 拉**，不是从 `main`（它落后 481 条）：

```bash
git fetch origin
git checkout -b codex/feature-p0fix-xxx origin/product/packaged-slicer
```

### 2.3 提交信息

必须与 `product/packaged-slicer` 的既有风格一致（`AGENTS.md` 第 10 条）：

```
type(专项slug): 【功能分类】中文摘要

正文块一，一句话说清做了什么或为什么。

正文块二。
```

**实测约束**（取自 `product` 近 25 条提交）：

- `type` ∈ `feat` / `fix` / `docs` / `test` / `build` / `merge`
- 标题 **32–44 字**（中位 36）
- 正文 **2–4 个块**，块间空行，每块 **19–77 字**（中位 46）
- **全角标点**，正文里不出现半角 `,` `.` `:` `;`
- 结尾按环境要求附 `Co-Authored-By`

### 2.4 合回产品线

**能快进就快进，不要制造无意义的合并提交**：

```bash
# 在工作分支上先把 product 合进来，解掉冲突、跑完验证
git merge --no-ff product/packaged-slicer
# 验证通过后，product 侧快进
git -C <product 工作树> merge --ff-only codex/feature-xxx
git push origin product/packaged-slicer
```

> 为什么反向先合：验证要在**产出过基线的那个构建目录**里做（本仓有多个构建目录，个别已坏）。
> 反向合完再让 product 快进，product 侧零冲突、零额外验证。

### 2.5 历史改写与备份

- **改写前必须建备份分支**：`backup/pre-<改写原因>-<日期>`。
- 改写后**必须用树比对验证零内容丢失**：`git diff --diff-filter=A --name-only HEAD <backup>`
  应为空（备份里没有 HEAD 缺失的文件）。
  **注意 diff 方向**：`git diff HEAD <backup> --diff-filter=D` 列的是 HEAD 有而备份没有的，
  方向搞反会得出相反结论。
- 合入 product **并推送成功**之后，备份才可删除（`git branch -D`）。
- **绝不 force-push 任何在 origin 上存在的分支。** 改写只允许发生在未发布的本地分支上
  （`AGENTS.md` 第 10 条）。

### 2.6 删除分支

- 已并入 product 的：用 `git branch -d`（安全模式），**不要直接用 `-D`**。
  `-d` 会在「已并入 HEAD 但未并入自身 origin 上游」时拒绝——那正是需要人看一眼的情形。
- 被 `-d` 拒绝而确实想删的：先算 product 的可达集合做包含判断，确认**每一条**提交都在其中，
  再用 `-D`。抽样检查不够。
- 工作树占用的分支删不掉；先 `git worktree list` 看清。

---

## 三、合入前的验证闸门（替代 PR 评审）

没有 CI、没有评审人，闸门就只能是可复现的验证。三档已实测定型，
对应 `CMakePresets.json` 的三个 testPreset：

| 档 | preset | 项数 | 实测 | 什么时候用 |
|---|---|---|---|---|
| 核心 | `slicesoft-debug-core` | 155 | **12–18 秒** | 改完想立刻看一眼 |
| 快集 | `slicesoft-debug-fast` | 262 | 约 3 分钟 | 提交前 |
| 全量 | `slicesoft-debug-full` | 270 | 约 16 分钟 | **合入产品线（本节闸门）** |

**合入 product 必须满足三条**：

1. **全量重建**（`cmake --build <dir> --config <cfg>`，不加 `--target`）。
   理由见 `AGENTS.md` 8b：版本 PATCH 由提交数派生、提交即变，只重建部分目标会让
   跨二进制比对版本的测试变红，**而那种红灯与真回归长得一模一样**。
2. **字节级基线 PASS**：`python scripts/CaptureSliceOutputBaseline.py --verify`
   （11 个用例 / 738 个产物逐字节一致）。
3. **失败集合与基线一致**。比**集合**不比**数量**（`AGENTS.md` 8c）——数量相同可能是
   「新增 N 条、消失 N 条」。当前稳定失败集为 **4 项**：
   `stage14f03_single_model_s1_gate`、`stage16c06_bounded_support_shape_unit_tests`、
   `scene_layer_adapters_unit_tests`、`slicer_stage14e02_qt_host_boundary_test`。

> 并行度已按实测定档：核心档 `jobs: 8`，快集与全量 `jobs: 4` 且带 `--repeat until-pass:2`。
> **不要把它们"对齐"成构建侧的 16**——起进程与 Qt 的测试在并行下墙钟膨胀 10~25 倍，
> 会撞自己的内部期限而假失败。理由写在每个 preset 的 `description` 字段里。

---

## 四、版本号规范

`versionScheme: semver-2.0.0`，格式 `MAJOR.MINOR.PATCH[-PRERELEASE]`。

> ⚠ **初版第 174 行写「版本只在 version-manifest.json 维护，禁止根据 Git commit 数量推导版本」。
> 这一条与构建系统直接矛盾，已按实际修正。**
> `cmake/SliceSoftVersion.cmake` 第 36–67、190 行：**PATCH 由
> `git rev-list --count <最近 v* 标签>..HEAD` 自动派生**，清单只维护 MAJOR.MINOR。
> 代码里写明了理由：MINOR 表示「新增功能」是人的判断，而 PATCH 表示「同一功能面上的修补」，
> 与提交数天然对应，交给 git 比手工更准也免于遗漏。**构建系统的立场更有理据，故以它为准。**

**由此产生的一条纪律**（`AGENTS.md` 8b）：**提交会改变版本号**，所以判定回归之前必须全量重建。
实测一次提交后的全量重建是 230 次重链、约 5.5 分钟，而 `.cpp` 编译为 0——时间全在链接上。

- MAJOR / MINOR：手工维护在 `version-manifest.json`。手工提升 MINOR 时**应同时打
  `v<MAJOR>.<MINOR>.0` 标签**，PATCH 即从该标签重新计数。
- PRERELEASE：`dev` / `alpha.N` / `beta.N` / `rc.N`。
- 当前状态：清单 `0.2.0-dev`，仓库唯一的语义化标签是 **`v0.1.0`**。

> ⚠ 初版第 175 行引用 `docs/reference/版本管理/`——**该目录不存在**，引用已删。

### 标签管理

- 正式版本用 `vMAJOR.MINOR.PATCH` 的 **annotated tag**（`git tag -a`）。
- 标签必须写清新增功能、修复缺陷、已知问题。
- **历史版本标签永不删除。**

> ⚠ 初版热修复流程里写 `git tag -a v1.2`——**两段版本号，与本节的三段式自相矛盾**，已修正为
> `v1.2.1` 这类三段形式。

---

## 五、发布流程

没有 develop，所以流程比初版短一节：

```bash
# 1. 工作分支验证通过（第三节三条闸门）
# 2. product 侧快进并推送
git -C <product 工作树> merge --ff-only <工作分支>
git push origin product/packaged-slicer

# 3. 发布时打标签
git tag -a v0.2.0 -m "v0.2.0：<新增/修复/已知问题>"
git push origin v0.2.0

# 4. 删除工作分支（先 -d，被拒再复核后 -D）
git branch -d <工作分支>
```

**热修复**：从 `product/packaged-slicer` 拉 `hotfix/<描述>`，修完走同一套闸门与快进。
不需要「合并回 develop」这一步——没有 develop。

---

## 六、初版中仍然有效、原样保留的部分

这些与仓库实际不冲突，是对的：

- **测试分支只进不出**：缺陷在原开发分支修复后重新合并过来，不在测试分支上直接改。
- **测试分支用完即删**，下一轮重新拉取，避免历史代码污染。
- **不把未完成的功能合并到测试分支。**
- **历史版本标签永不删除**，用于追溯和回滚。
- 多版本并行维护时按大版本建维护分支（本仓目前只有一条产品线，暂不需要）。

---

## 附：这份规范怎么维护

- 改动前**先实测**再落笔。初版之所以需要大改，不是因为写得不好，而是因为它写的是
  一套通用模型而非这个仓库——六处与实际不符的地方（无 develop、main 非生产分支、
  引用不存在的目录、版本派生方式相反、不存在的维护分支、"运控 SDK" 来自另一项目），
  每一处单独看都很合理，合起来就不能用。
- 与 `AGENTS.md` 冲突时**以 `AGENTS.md` 为准**并在此标注更正，不要两边各写一套。
- 数字（项数、耗时、失败集合）都会过期。写数字时**一并写测得的日期与方法**，
  否则半年后没人知道该不该相信它。
