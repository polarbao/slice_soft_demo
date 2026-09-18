# Git 分支与发布规范

> **本文是 `Git 版本发布流程 + 测试分支规划.md`（2026-08-05 初版）的修订版。**
> 初版描述的是一套教科书式 git-flow，但**它描述的仓库不是这个仓库**——照它第一步
> `git checkout develop` 在当时会直接失败——**2026-09-18 首次实测时本仓（含远端）一条 develop 都没有**。
> 修订依据是 2026-09-18 对仓库实际状态的逐条实测，每处更正都标注了证据。
> 初版保留在同目录，供对照；**以本文为准**。
>
> **2026-09-18 第二次修订**：用户裁定采纳 `develop`，并把 `main` 快进到产品线。
> 本文随之改写第一、二、三、五节——`develop` 不是照搬初版，而是拿到了一个**有实测依据的职责**
> （见第一节「develop 的职责」）。

---

## 一、实际分支模型（按实测，不是按理想）

**初版说核心是 `main` + `develop`。2026-09-18 首次实测时：`develop` 本地与远端各 0 条，`main` 落后真正的产品线 481 条。**（同日用户裁定采纳 develop 并把 main 快进，现状见下表；初版的问题不在于要不要 develop，而在于它当时**不存在**却被当作流程基础。）
本仓实际是「双产品线 + agent 工作分支」模型：

| 分支 | 作用 | 实际状态（2026-09-18） |
|---|---|---|
| `product/packaged-slicer` | **真正的产品线**，封装宿主 + 能力模块拓扑 | 与 origin 同步，发布基线 |
| `product/legacy-slicer` | 并行 legacy 线，手工运行时 | 领先 origin 2 条；有独立工作树且含未提交改动 |
| `develop` | **集成线**：功能分支合回此处，累积到里程碑再进产品线 | 2026-09-18 建于 `a0a4f742` |
| `main` | 仓库名义默认分支（`origin/HEAD` 指向它） | **2026-09-18 已快进至产品线**（`b5fc0fb3 → a0a4f742`，489 条落差归零） |
| `codex/feature-<专项>-<描述>` | agent 工作分支 | 用完合入 product 后删除 |
| `feature/<描述>` | 早期人工分支 | origin 上尚存 2 条历史遗留 |
| `archive/<原因>-<日期>` | 归档快照 | origin 上 2 条 + 同名标签 |

> ⚠ **`slice_soft_demo-logdump` 工作树不要删**（15GB，2026-09-18 已 detached）。
> 它看着像 P0FIX 合入后的冗余残留，**实际是待做的 LOGDUMP E01B 的活资产**：
> E01B 缺 P23 loader 尚未完成，而 E01A 验收报告引用的
> `runtime/logdump/Release/slicer_module.dll` 仍在该工作树原位，
> 是外部项目 `ry_print_demo/PrintSolution` 集成消费的那一个；另有 10 余份 LOGDUMP 文档引用该路径。
> **判断"某个工作树是否冗余"时，要查引用它的文档与未完成的专项阶段，不能只看分支是否已合入。**

> **入口已定（2026-09-18）**：保留 `main` 作 `origin/HEAD` 指向的入口，
> 但它**只跟随 `product/packaged-slicer`，永不直接提交**——由发布流程末尾的快进更新。
> 选它而不是改指 product 的理由有两条：改 GitHub 默认分支需要网页或 `gh`（本环境不可用），
> 而克隆者本来就预期默认分支叫 `main`。**代价是多一步快进**，写进第五节流程里。

### develop 的职责（为什么这次它不是空转的）

初版把 develop 当成「开发主分支」，那在本仓会空转——**单人 + AI agent 协作、无 CI**
（F-16 用户 2026-08-10 裁决暂缓），`gh` CLI 在本环境不可用，所以初版
「禁止直接提交、只能通过 PR 合并、需至少 1 人评审」**当前无法执行**。
照搬只会得到一条没人走的流程。

**这次给它的职责来自实测的闸门成本差**。三档验证耗时差了一个量级：

| 档 | 项数 | 实测 |
|---|---|---|
| `slicesoft-debug-core` | 155 | **12~18 秒** |
| `slicesoft-debug-fast` | 262 | 约 3 分钟 |
| `slicesoft-debug-full` | 270 | 约 16 分钟（另加全量重建与字节级基线） |

**于是三层分支对应三档闸门**：

```
claude|codex/feature-*   ── 核心档 12~18 秒 ──┐  自测
                                              ↓
develop                  ── 快集档 约 3 分钟 ─┐  集成线
                                              ↓
product/packaged-slicer  ── 全量档 约 16 分钟 ┘  发布线（+ 字节级基线）
```

**develop 的收益是可量化的**：几个功能可以在它上面累积、每次只付 3 分钟的快集闸门，
不必每合一个就付 16 分钟的里程碑闸门。这是省时间，不是走流程。

**反过来说，如果哪天有了 CI，develop 的这层职责就该交给 CI**——届时重新评估它是否还需要存在。

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

**功能分支从 `develop` 拉**，不是从 `main`（它只是入口，跟随 product）：

```bash
git fetch origin
git checkout -b codex/feature-p0fix-xxx origin/develop
```

**热修复例外**：从 `product/packaged-slicer` 拉，修完合回 product 与 develop 两侧——
否则下一次 develop 进 product 会把修复覆盖掉。

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

**能快进就快进，不要制造无意义的合并提交。** 分两步走：

```bash
# ① 功能分支 → develop（跑快集档）
git merge --no-ff develop            # 先把 develop 合进来，解冲突、跑验证
ctest --preset slicesoft-debug-fast  # 闸门
git checkout develop && git merge --ff-only codex/feature-xxx
git push origin develop

# ② develop → product（里程碑，跑全量 + 基线）
cmake --build <构建目录> --config Debug          # 全量重建，不加 --target
python scripts/CaptureSliceOutputBaseline.py --verify
ctest --preset slicesoft-debug-full
git checkout product/packaged-slicer && git merge --ff-only develop
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

   **已知抖动项另算一档**（2026-09-18 第一次真正执行本闸门后收窄）：

   | 项 | 抖动原因 | 处置 |
   |---|---|---|
   | `slicer_stage14d07_r2_engine_conformance_test` | E-07 取消期限余量仅 1%（实测 1824~2020ms vs 2000ms 阈值），见 F-53 | 隔离重跑确认后放行 |

   **未归因的新失败挡住合入；已归因的已知抖动项放行，但必须留痕。**
   「归因」有门槛，三样都要有：**① 隔离重跑**（无并发，至少 3 次，给出分布）；
   **② 指出具体判据**（哪一行、差多少，而不是「看起来是环境问题」）；
   **③ 说明为何与本次改动无关**（列出改动面，证明它碰不到失败路径）。
   三样缺一，就按未归因处理——**照字面再跑一遍 22 分钟只是赌抖动项这次落在阈值下方，那是仪式不是验证**；
   但没有门槛的「有理由就放行」会变成随意开口子。门槛就是两者之间的那条线。

### 一条刻意的豁免：纯文档改动只跑核心档

改动**完全落在** `*.md` / `docs/` / `analysis/` 之内时，合入 `develop` 只需核心档（12~18 秒），
不必跑快集档的 3 分钟。

**为什么要写下这条而不是"看情况"**：一条要求为改错别字跑 3 分钟测试的规则会被忽略，
而被忽略的规则比没有规则更糟——它让人以为有闸门。写明豁免边界，闸门才在真正需要时还被遵守。

**边界是"完全落在"，不是"主要是文档"**：只要有一个文件在 `src/` `apps/` `tests/` `scripts/`
`cmake/` `CMakeLists.txt` `CMakePresets.json` 里，就按正常档走。判据用
`git diff --name-only <base> HEAD` 自己看一眼，不靠印象。

**产品线那一跳不豁免**——`develop` → `product` 永远要全量档 + 字节级基线。
里程碑的意义就在于它不打折。

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

两跳：功能分支 → `develop`（快集闸门）→ `product/packaged-slicer`（全量闸门 + 基线）。
初版的 `release/vX.Y.Z` 这一跳**本仓不设**——发布就是给 product 上打标签，
再多一条冻结分支在单人协作下只会多一个要同步的地方。

```bash
# 1. 功能分支 → develop（见 2.4 ①）
# 2. 里程碑：develop → product（见 2.4 ②，必须过第三节三条闸门）

# 3. 发布时在 product 上打标签
git tag -a v0.2.0 -m "v0.2.0：<新增/修复/已知问题>"
git push origin v0.2.0

# 4. main 跟随 product（它是入口，不能落后）
git push . product/packaged-slicer:main   # push . 会强制要求快进，比 branch -f 安全
git push origin main

# 5. 删除工作分支（先 -d，被拒再复核后 -D）
git branch -d <工作分支>
```

**热修复**：从 `product/packaged-slicer` 拉 `hotfix/<描述>`，过第三节闸门后快进到 product，
**再合回 `develop`**——漏了这一步，下一次 develop 进 product 会把修复覆盖掉。

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
  一套通用模型而非这个仓库——六处与实际不符的地方（当时无 develop、main 非生产分支、
  引用不存在的目录、版本派生方式相反、不存在的维护分支、"运控 SDK" 来自另一项目），
  每一处单独看都很合理，合起来就不能用。
- 与 `AGENTS.md` 冲突时**以 `AGENTS.md` 为准**并在此标注更正，不要两边各写一套。
- 数字（项数、耗时、失败集合）都会过期。写数字时**一并写测得的日期与方法**，
  否则半年后没人知道该不该相信它。
