# 分支模型迁移记录：`product/` 退场，集成线改名 `develop/packaged-slicer`

> **状态：已全部执行完毕（2026-09-18 17:5x）。** 第五节记录了每一步的实际结果。
> 用户裁定（2026-09-18 第三次）：`product/` 分支退场，集成线用 `develop/<名字>` 形态，
> 功能分支统一为 `feature/<agent>-<专项slug>-<描述>`。
> 专项归属：**GITOPS**（见 `AGENTS.md`「各专项状态」）。

---

## 一、为什么这次改名是成立的

初次判断时我**反对**改名，理由是 `product/packaged-slicer` 是发布基线。
补测之后有两条事实支持改名，**我撤回原来的反对**：

**① `main` 与 `product/packaged-slicer` 已经是同一个提交。**

| 分支 | 提交 |
|---|---|
| `main` | `a0a4f742` |
| `product/packaged-slicer` | `a0a4f742` |

2026-09-18 第二次修订把 `main` 快进到产品线之后，两者内容完全重合。
**保留两条指向同一提交、职责又相同的长期分支，本身就是冗余**——
这正是改名要解决的问题，而不是我原先以为的「破坏发布基线」。

**② 没有任何脚本硬编码这个分支名。**

| 文件类型 | 引用该分支名的文件数 |
|---|---|
| `.md` | **36**（共 67 行） |
| `.ps1` / `.py` / `.json` / `.cmake` / `.txt` / `.yml` | **0** |

改名**不会造成任何功能性失效**，代价纯粹是文档更新。比我初判时假设的风险低得多。

**③ 附带收益：少一步快进。** 原流程第 4 步是 `git push . product/packaged-slicer:main`——
`main` 退场后自己就是发布线，这一步连同它的同步成本一起消失。

---

## 二、目标模型（已采纳）

```
main                                   发布线。origin/HEAD 指向它，只被快进，永不直接提交。
develop/packaged-slicer                集成线。功能分支合回此处，测试与发布验证在此进行。
feature/<agent>-<专项slug>-<描述>       功能分支。agent 取 claude 或 codex。
product/legacy-slicer                  保留不动（见第四节）。
```

`product/packaged-slicer` **退场**——不是改名成别的，而是删除，
因为 `main` 已经承载它的全部内容与职责。

### 闸门经济性保持不变

| 跳转 | 闸门 | 耗时 |
|---|---|---|
| `feature/*` → `develop/packaged-slicer` | 快集档（纯 `.md` 改动可降为核心档） | 约 3 分钟 / 12~18 秒 |
| `develop/packaged-slicer` → `main` | 全量档 + 字节级基线，**永不豁免** | 约 16 分钟 |

原先这两级是 `feature → develop`（快集）与 `develop → product`（全量）。
迁移只是把终点从 `product/packaged-slicer` 换成 `main`，**级数与判据一字未动**。
**换名字不是放宽门禁。**

---

## 三、两个选型的裁定与依据

### 选型一：集成线的形态 —— 定为 `develop/packaged-slicer`

这里有一条 git 硬约束，**已实测确认（两个方向都报错）**：

```
$ git branch dev && git branch dev/x
fatal: cannot lock ref 'refs/heads/dev/x': 'refs/heads/dev' exists; cannot create 'refs/heads/dev/x'

$ git branch dev2/x && git branch dev2
fatal: cannot lock ref 'refs/heads/dev2': 'refs/heads/dev2/x' exists; cannot create 'refs/heads/dev2'
```

即**裸名与同名命名空间互斥**。用户选了 `develop/<名字>` 形态，
因此本仓**永远不会有裸 `develop` 分支**——这是该选型的代价，需记住。

好处是将来 `product/legacy-slicer` 若也需要集成线，可以直接叫 `develop/legacy-slicer`，
与产品线一一对应。

改名本身可原子完成，已实测：

```
$ git branch -m develop develop/packaged-slicer      # 退出码 0
```

### 选型二：功能分支前缀 —— 定为 `feature/<agent>-<专项slug>-<描述>`

改名前是 `codex/feature-<专项slug>-<描述>`（agent 在最前）。
改成顶层统一 `feature/`，理由是顶层前缀应表达「这是什么分支」而不是「谁开的」；
**agent 信息没有丢，只是移到第二段**，多 agent 并行时仍能区分。

实例：`feature/codex-p0fix-contract-robustness`。

---

## 四、不动的部分

**`product/legacy-slicer` 保留原名，不参与本次迁移。** 两条理由，均已核实：

1. 它被**另一个工作树**占用——`git worktree list` 显示
   `E:/__Code/__Work/slice_test_demo/slice_soft_demo_legacy_manual_runtime` 检出了它。
   重命名被其它工作树检出的分支会破坏那个工作树。
2. 它领先 `origin/product/legacy-slicer` **2 条提交**，
   是一条活的、独立的产品线，不是本次要整理的对象。

迁移后 `product/` 前缀下只剩它一条。这看起来不对称，
但**对称性不值得用破坏工作树去换**；等它自己的工作收口时再单独处理。

---

## 五、执行步骤与进度

### 已执行（本地，2026-09-18）

```bash
# ✅ 1. 功能分支合入集成线（已过快集档：262 项 7 失败，与基线 4 项 + F-53 已归因 3 项精确吻合）
git checkout develop
git merge --ff-only claude/feature-gitops-branch-model

# ✅ 2. 集成线改名
git branch -m develop develop/packaged-slicer

# ✅ 3. 删除已合入的功能分支（-d 安全模式通过）
git branch -d claude/feature-gitops-branch-model

# ✅ 4. 文档同步：本规范、AGENTS.md 第 11 条 a/b/c、本记录
```

~~**遗留待处理**：`develop/packaged-slicer` 仍追踪 `origin/develop`。~~
**已解决**：`git push -u` 时一并重设，现追踪 `origin/develop/packaged-slicer`。

### 已执行：里程碑闸门（2026-09-18 17:22~17:50）

跑在最终树 `97ee3dab` 上——**中途补了 5 个文档文件之后重跑了一次**，
因为闸门必须覆盖实际要合入的那棵树，「只差几个 .md」不是豁免理由。

| 项 | 结果 |
|---|---|
| 全量重建 Debug（不加 `--target`） | `BUILD_EXIT=0`，0 个错误 |
| 字节级基线 | **PASS**，11 个用例 / 738 个产物逐字节一致 |
| 全量档 270 项 | 7 失败 = 基线 4 项 + F-53 已归因 3 项，**无未归因新失败** |

失败集合与同日快集档那轮**完全一致**，且字节级基线逐字节通过——功能面零变化。
按第三节判据放行。

### 已执行：合并与推送

```bash
# ✅ 5. main 快进：a0a4f742 → 97ee3dab
# ✅ 6. git push origin main
# ✅ 7. git push origin --delete develop        ← 必须在第 8 步之前，见下
# ✅ 8. git push -u origin develop/packaged-slicer
# ✅ 9. git push origin --delete product/packaged-slicer
#      git push origin --delete feature/12e-08c-mesh-repair
#      git push origin --delete feature/14-slicer-capability-package
# ✅ 10. git branch -d product/packaged-slicer   （-d 安全模式通过，未用 -D）
```

> **执行中的发现：推送顺序不能按原计划。** 原计划先推 `develop/packaged-slicer` 再删远程 `develop`，
> 结果被远程拒绝：
>
> ```text
> ! [remote rejected] develop/packaged-slicer -> develop/packaged-slicer
>   (cannot lock ref 'refs/heads/develop/packaged-slicer': 'refs/heads/develop' exists;
>    cannot create 'refs/heads/develop/packaged-slicer')
> ```
>
> **第三节那条「裸名与同名命名空间互斥」的约束在远程同样成立**，我只在本地验过就以为够了。
> 正确顺序是**先删旧名、再推新名**。删除前已验证 `origin/develop`（`ff7ec864`）
> 是 `origin/main` 的祖先、独有提交 0 条，不丢任何提交。

#### 删除前的核验与安全网

四条分支删除前都验过「独有提交数为 0」（对照已推送的 `origin/main`），SHA 记录如下：

| 分支 | 删除前 SHA |
|---|---|
| `origin/product/packaged-slicer` | `a0a4f7427726ea808490a99de9ec5aa203c38f9b` |
| `origin/develop` | `ff7ec8641895747c9d5d9f979706387dc23e050a` |
| `origin/feature/12e-08c-mesh-repair` | `4fd009784ac8a6da66ab1847acff592adb6975fa` |
| `origin/feature/14-slicer-capability-package` | `4fd009784ac8a6da66ab1847acff592adb6975fa` |

（两条历史遗留 feature 分支指向同一个提交。）

### 迁移后的实际状态

```text
本地                                        远程
  develop/packaged-slicer  97ee3dab  ──→  origin/develop/packaged-slicer
  main                     97ee3dab  ──→  origin/main   （origin/HEAD 指向它）
  product/legacy-slicer    d538a843  ──→  origin/product/legacy-slicer（领先 2 条，未动）
                                          origin/archive/*  2 条归档，未动
```

`main` 与集成线当前停在同一提交——这是里程碑刚合完的正常状态，
下一个功能进集成线后就会分开；与退场前 `main`/`product` 那种**两条都是发布级**的重合不同。

#### 第 8 步删除清单的依据

| 分支 | 为什么可删 |
|---|---|
| `product/packaged-slicer` | 与 `main` 同提交（`a0a4f742`），职责由 `main` 承接 |
| `develop` | 已改名为 `develop/packaged-slicer`，远程旧名需一并退场 |
| `feature/12e-08c-mesh-repair` | 已完全并入产品线，最后提交 2026-08-05，命名带阶段号（违反 2.1「不用阶段号」） |
| `feature/14-slicer-capability-package` | 同上 |

**删除远程分支不可撤销**，所以第 8 步执行前会把四条逐一列出再确认一次。

---

## 六、文档同步改动面

### 已改（治理文档，需逐字改写而非批量替换）

| 文件 | 处理 |
|---|---|
| `docs/git/Git 分支与发布规范.md` | 第一、二、五节按新模型改写；残留 7 处旧名**全是叙述退场本身**，是对的 |
| `AGENTS.md` 第 11 条 a/b/c | 命名、拉取/合回、闸门映射三条同步；**判据与豁免范围一字未动** |

### 刻意不改

| 文件 | 为什么 |
|---|---|
| `analysis/04_问题清单与改动空间.md`（6 处） | 历史记录。改写它会让「当时发生了什么」失真 |
| `AGENTS.md`「各专项状态」（第 45/52/67/91/92 行） | 同上，记的是各专项当时的分支事实 |

### 已逐条判断的其余 32 处

判据是**时态**：陈述「当前在哪条分支 / 适用哪条产品线」的句子在退场后会变成**假命题**，必须改；
记录「已合入 X」「分叉自 X」「从 X 新建」「X 从 a 快进至 b」的是当时的事实，改了会让历史失真。

**改了 5 个文件 6 处**（全部是现在时陈述）：

| 文件 | 原句要害 |
|---|---|
| `PRODUCT_LINE.md` | 「本分支是 `product/packaged-slicer`」——文件本身将位于 `main` |
| `.agents/docs/build-and-test.md` | 「On `product/packaged-slicer`, the runtime directory contains」——给 agent 的说明 |
| `docs/user_guides/QT_DEBUG_UI_操作手册.md` | 「新版 `product/packaged-slicer` 已改为⋯」 |
| `docs/user_guides/SLICE_PRODUCT_PACKAGED_新版切片软件使用手册.md` | 「适用产品线：`product/packaged-slicer`」 |
| `docs/codex_task/current/TASKS_USER_GUIDE_双分支切片软件使用手册任务清单.md` | 「适用分支」与「以当前 `product/packaged-slicer` 源码为准」 |

**其余 32 处一律不动**，包括 `docs/slice/REPORT/` 的 9 处收口报告、
`docs/slice/DOC/` 的 5 处设计与可行性分析、`docs/codex_task/current/` 的历史执行记录、
`analysis/04` 的 6 处、以及 `AGENTS.md`「各专项状态」里的分支事实。
逐条看过语境后确认它们全是过去时记录——
例如 MATOPQ 任务清单里那行 `--base-ref product/packaged-slicer` 是**验证结果记录**
（「改前基线 exit 0；改后 exit 0⋯PASS」），不是待执行指令。

**旧名到新名的映射靠本文档承担**：读到历史记录里的 `product/packaged-slicer` 时，
从这里可以查到它退场的时间与去向，不需要把每份报告都改一遍。

---

## 七、回退

第 1~4 步是本地操作，回退方式：

```bash
git branch -m develop/packaged-slicer develop     # 撤销改名
git branch claude/feature-gitops-branch-model 7253f4d0   # 恢复被删分支
```

迁移前各分支位置（2026-09-18 记录）：

| 分支 | 提交 |
|---|---|
| `main` | `a0a4f742` |
| `product/packaged-slicer` | `a0a4f742` |
| `develop`（改名前） | `ff7ec864` → 合入后 `7253f4d0` |
| `claude/feature-gitops-branch-model`（已删） | `7253f4d0` |

第 7~8 步一旦推送/删除远程分支，**回退需要重新推送**，
而删除远程分支后若无人持有本地副本则无法恢复——这是执行前要单独确认的原因。
