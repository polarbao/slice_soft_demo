# 分支模型迁移方案：`product/` 退场，`dev` 成为集成线

> **状态：待裁定，尚未执行。** 本文只提方案与实测依据，不含已完成的改动。
> 用户裁定（2026-09-18）：按 git 规则建 `dev` 分支，测试与发布走它，`feature` 承载功能实现，
> 因此 `product/` 分支改名。本文把这个意图落成可执行步骤，并标出其中需要先定的两个选项。
> 专项归属：**GITOPS**（见 `AGENTS.md`「各专项状态」）。

---

## 一、为什么这次改名是成立的

初次判断时我反对改名，理由是 `product/packaged-slicer` 是发布基线。
补测之后有两条事实支持改名，**我撤回原来的反对**：

**① `main` 与 `product/packaged-slicer` 已经是同一个提交。**

| 分支 | 提交 |
|---|---|
| `main` | `a0a4f742` |
| `product/packaged-slicer` | `a0a4f742` |

2026-09-18 把 `main` 快进到产品线之后，两者内容完全重合。
**保留两条指向同一提交、职责又相同的长期分支，本身就是冗余**——
这正是「改名/退场」要解决的问题，而不是我原先以为的「破坏发布基线」。

**② 没有任何脚本硬编码这个分支名。**

| 文件类型 | 引用该分支名的文件数 |
|---|---|
| `.md` | **36**（共 67 行） |
| `.ps1` / `.py` / `.json` / `.cmake` / `.txt` / `.yml` | **0** |

改名**不会造成任何功能性失效**，代价纯粹是文档更新。
这比我初判时假设的风险低得多。

---

## 二、目标模型

```
main                      发布线。origin/HEAD 指向它，只被快进，永不直接提交。
dev                      集成线。所有功能分支合回此处，测试与发布验证在此进行。
feature/<专项slug>-<描述>  功能分支。承载全部功能实现。
product/legacy-slicer     保留不动（见第四节「不动的部分」）。
```

`product/packaged-slicer` **退场**——不是改名成别的，而是删除，
因为 `main` 已经承载它的全部内容与职责。

### 闸门经济性保持不变

现行模型用两级闸门，这一点迁移后必须保留，否则等于放宽门禁：

| 跳转 | 闸门 | 耗时 |
|---|---|---|
| `feature/*` → `dev` | 快集档（纯 `.md` 改动可降为核心档） | 约 3 分钟 / 12~18 秒 |
| `dev` → `main` | 全量档 + 字节级基线，**永不豁免** | 约 16 分钟 |

原先这两级是 `feature → develop`（快集）与 `develop → product`（全量）。
迁移只是把终点从 `product/packaged-slicer` 换成 `main`，**级数与判据不变**。

---

## 三、需要你先定的两个选项

### 选项 A：`dev` 还是 `dev/<名字>`

你说的是「dev/ 分支」，带斜杠。这里有一条 git 硬约束，**已实测确认**：

```
$ git branch dev && git branch dev/x
fatal: cannot lock ref 'refs/heads/dev/x': 'refs/heads/dev' exists；cannot create 'refs/heads/dev/x'

$ git branch dev2/x && git branch dev2
fatal: cannot lock ref 'refs/heads/dev2': 'refs/heads/dev2/x' exists；cannot create 'refs/heads/dev2'
```

**`dev` 与 `dev/任何东西` 互斥，两个方向都不行。** 所以要二选一：

| | 形态 | 适用情形 |
|---|---|---|
| **A1（建议）** | 单条 `dev` | 只有一条集成线。本仓当前就是这样——`develop` 改名为 `dev` 即可 |
| **A2** | `dev/packaged-slicer` | 将来每条产品线各有一条集成线。代价：永远不能有裸 `dev` |

建议 A1：现在只有一条集成线，A2 是为一个尚不存在的需求付名字成本。

### 选项 B：功能分支的前缀

现行规范第 2.1 节定的是 `claude/feature-<专项slug>-<描述>` / `codex/feature-...`，
前缀区分「谁开的分支」。你的说法是 `feature` 承载功能实现。两种落法：

| | 形态 | 得失 |
|---|---|---|
| **B1（建议）** | `feature/<agent>-<专项slug>-<描述>`<br>如 `feature/claude-gitops-branch-model` | 顶层统一为 `feature/`，仍保留是谁开的 |
| **B2** | `feature/<专项slug>-<描述>` | 更短，但丢掉 agent 信息，多 agent 并行时不好认 |

建议 B1。注意它与 A1 不冲突（`feature/` 与 `dev` 是不同顶层名）。

---

## 四、不动的部分

**`product/legacy-slicer` 保留原名，不参与本次迁移。** 两条理由：

1. 它被**另一个工作树**占用（`slice_soft_demo_legacy_manual_runtime`），
   重命名被其它工作树检出的分支会破坏那个工作树。
2. 它领先 `origin/product/legacy-slicer` **2 条提交**且含未提交改动，
   是一条活的、独立的产品线，不是本次要整理的对象。

迁移后 `product/` 这个前缀下只剩它一条。这看起来不对称，但**对称性不值得用破坏工作树去换**；
等它自己的工作收口时再单独处理。

---

## 五、执行步骤

> 每一步都可停下。凡涉及 `origin` 的步骤单独标注，**需你另行确认后才执行**。

```bash
# ── 本地整理 ────────────────────────────────────────────
# 1. 功能分支合入集成线（已过快集档）
git checkout develop
git merge --ff-only claude/feature-gitops-branch-model

# 2. develop 改名为 dev
git branch -m develop dev

# 3. 删除已合入的功能分支
git branch -d claude/feature-gitops-branch-model

# ── 里程碑闸门（约 16 分钟 + 字节级基线）──────────────────
# 4. dev → main 前必须过全量档与基线，失败集合与基线比对
cmake --build build-slicesoft/main --config Debug -- -m
ctest --preset slicesoft-debug-full
python scripts/CaptureSliceOutputBaseline.py --verify

# 5. main 快进到 dev
git checkout main
git merge --ff-only dev

# ── 远程（需另行确认）─────────────────────────────────────
# 6. 推送 main 与 dev
git push origin main
git push origin dev

# 7. 删除远程已退场/已失效的分支
git push origin --delete product/packaged-slicer
git push origin --delete develop
git push origin --delete feature/12e-08c-mesh-repair
git push origin --delete feature/14-slicer-capability-package

# 8. 本地删除退场分支
git branch -D product/packaged-slicer
```

### 第 7 步删除清单的依据

| 分支 | 为什么可删 |
|---|---|
| `product/packaged-slicer` | 与 `main` 同提交（`a0a4f742`），职责由 `main` 承接 |
| `develop` | 已改名为 `dev`，远程旧名需一并退场 |
| `feature/12e-08c-mesh-repair` | 已完全并入 product，最后提交 2026-08-05，且命名带阶段号（违反 2.1「不用阶段号」） |
| `feature/14-slicer-capability-package` | 同上 |

**删除远程分支不可撤销**，所以第 7 步执行前我会把四条逐一列出再确认一次。

---

## 六、文档同步改动面

改名后需要更新 36 个 `.md` 文件里的 67 处引用。其中要逐字改写而不是批量替换的有三处：

| 文件 | 为什么不能批量替换 |
|---|---|
| `docs/git/Git 分支与发布规范.md`（10 处） | 第一、二、五节讲的是分支**职责**，改名后整段论证要重写，不只是换名字 |
| `AGENTS.md`（3 处） | 第 11 条的分支约定与闸门判据都按旧名写 |
| `analysis/04_问题清单与改动空间.md`（6 处） | 是历史记录，**不应改写**——它记的是当时的事实 |

其余 33 个文件（各 1~3 处）多为报告与任务清单里的路径提及，可批量替换后抽查。

**`analysis/04` 的 6 处保持原样**：改写历史记录会让「当时发生了什么」失真，
这与本文档第一节撤回原判断的做法一致——**新事实写新段落，不篡改旧记录**。

---

## 七、回退

第 1~3 步与第 5 步是本地操作，回退方式：

```bash
git branch -m dev develop                    # 撤销改名
git reset --hard <迁移前的提交>              # 各分支按下表复位
```

迁移前各分支位置（2026-09-18 记录）：

| 分支 | 提交 |
|---|---|
| `main` | `a0a4f742` |
| `product/packaged-slicer` | `a0a4f742` |
| `develop` | `ff7ec864` |
| `claude/feature-gitops-branch-model` | `36887ba5` |

第 6~7 步一旦推送/删除远程分支，**回退需要重新推送**，
而删除远程分支后若无人持有本地副本则无法恢复——这是执行前要单独确认的原因。
