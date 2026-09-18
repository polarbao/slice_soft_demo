# Git版本发布流程 + 测试分支规划

# 一、核心分支模型
采用 2 个核心分支 + 4 类临时分支的模型，兼顾开发效率和发布稳定性：

## 1. 永久核心分支（永远存在，禁止直接提交）
|分支名|作用|保护级别|代码质量|
|---|---|---|---|
|main/master|生产环境分支，存放已发布的正式版固件|最高（只能通过 PR 合并）|100% 可生产|
|develop|开发主分支，存放下一个版本的最新开发代码|高（只能通过 PR 合并）|可编译，基本功能正常|

## 2. 临时分支（用完即删，生命周期短）
|分支类型|命名规范|从哪个分支拉取|合并到哪个分支|生命周期|
|---|---|---|---|---|
|功能分支|feature/xxx|develop|develop|单个功能开发周期|
|测试分支|test/xxx|develop/release/xxx|不合并，用完即删|单个测试周期|
|发布分支|release/vX.Y.Z|develop|main + develop|版本发布周期|
|热修复分支|hotfix/vX.Y.Z-xxx|main|main + develop|紧急 bug 修复周期|


# 二、测试分支详细规划
这是最容易混乱的部分，绝对不要在 develop 分支上直接测试，也不要所有测试都用同一个分支。按测试阶段拆分 4 个独立测试分支，实现环境隔离：

## 1. 测试分支整体原则
• 每个测试阶段对应独立的测试分支，测试环境与开发环境完全隔离
• 测试分支只进不出，测试不通过的 bug 在原开发分支修复后重新合并到测试分支
• 测试分支用完即删，下一轮测试重新拉取新分支，避免历史代码污染

## 2. 分阶段测试分支规划
### （1）单元测试 & 集成测试（开发自测阶段）
• 对应分支：feature/xxx（功能分支）
• 负责人：开发工程师
• 测试内容：单个模块功能测试、模块间接口测试
• 流程：
a. 开发完成后在功能分支上进行单元测试
b. 自测通过后提交 PR 合并到develop分支
c. 运行集成测试，失败则打回开发修复

### （2）系统测试（测试工程师阶段）
• 对应分支：test/system-vX.Y.Z
• 从哪里拉取分支：从develop分支拉取最新代码
• 负责人：测试工程师
• 测试内容：全功能测试、性能测试、稳定性测试
• 流程：
a. 每周一拉取新的系统测试分支，部署到测试环境
b. 测试工程师在该分支上进行测试，提交 bug 到缺陷管理系统
c. 开发工程师在对应的feature/xxx分支修复 bug，合并到develop
d. 每天晚上自动将develop的最新 bug 修复合并到test/system-vX.Y.Z
e. 所有 bug 修复完成后，进入预发布测试阶段

### （3）预发布测试（上线前最后验证）
• 对应分支：release/vX.Y.Z（发布分支）
• 从哪里拉取分支：测试通过后，从develop分支拉取
• 负责人：运维 + 测试
• 测试内容：生产环境模拟测试、固件烧录测试、回归测试
• 流程：
a. 拉取发布分支，冻结代码（只允许修复阻断性 bug）
b. 部署到预生产环境，进行最后一轮回归测试
c. 测试通过后，合并到main分支，打标签发布
d. 同时合并回develop分支，保证开发分支代码最新


# 三、完整版本发布流程（一步到位）
以发布v1.2.0版本为例，完整流程如下：

## 1. 开发阶段
# 1. 从develop拉取功能分支
git checkout develop
git pull origin develop
git checkout -b feature/encoder-optimization

# 2. 开发并自测
# ... 编写代码 ...
git add .
git commit -m "feat: 优化编码器多圈计数逻辑"

# 3.所有本地提交推送到远程仓库的对应分支
git push origin feature/encoder-optimization

# 4.代码评审通过后合并到develop,同步推送origin/develop
git checkout develop
git merge feature/encoder-optimization
git push

## 2. 系统测试阶段
# 1. 从develop拉取系统测试分支
git checkout develop
git pull origin develop
git checkout -b test/system-v1.2.0
git push origin test/system-v1.2.0

# 2. 部署到测试环境，测试工程师开始测试

# 3. 开发修复bug，合并到develop
# 操作流程参考开发阶段

# 4. 同步develop的bug修复到测试分支
git checkout test/system-v1.2.0
git pull origin develop
git push origin test/system-v1.2.0

## 3. 发布准备阶段
# 测试通过后，拉取发布分支
git checkout develop
git pull origin develop
git checkout -b release/v1.2.0
git push origin release/v1.2.0

# 冻结代码，只修复阻断性bug
# 预生产环境测试

## 4. 正式发布阶段
# 1. 合并发布分支到main
git checkout main
git pull origin main
git merge --no-ff release/v1.2.0 -m "chore: release v1.2.0"

# 2. 打版本标签（最重要！用于追溯历史版本）
git tag -a v1.2.0 -m "v1.2.0 正式版：优化编码器精度，缩短伺服整定时间"
git push origin v1.2.0

# 3. 合并回develop分支
git checkout develop
git pull origin develop
git merge --no-ff release/v1.2.0 -m "chore: merge release v1.2.0 back to develop"
git push origin develop

# 4. 删除临时分支
git branch -d release/v1.2.0
git branch -d test/system-v1.2.0
git push origin --delete release/v1.2.0
git push origin --delete test/system-v1.2.0

## 5. 紧急热修复流程（线上 bug）
# 1. 从main拉取热修复分支
git checkout main
git pull origin main
git checkout -b hotfix/v1.2-motor-bug

# 2. 修复bug并测试
git add .
git commit -m "fix: 修复电机正反转切换时丢步问题"

# 3. 合并到main发布
git checkout main
git merge --no-ff hotfix/v1.2-motor-bug -m "chore: hotfix v1.2"
git tag -a v1.2 -m "v1.2 热修复：解决电机丢步问题"
git push origin v1.2

# 4. 合并回develop
git checkout develop
git merge --no-ff hotfix/v1.2-motor-bug -m "chore: merge hotfix v1.2 back to develop"
git push origin develop

# 5. 删除热修复分支
git branch -d hotfix/v1.2-motor-bug
git push origin --delete hotfix/v1.2-motor-bug


# 四、版本号规范（语义化版本）

运控 SDK 是独立发布的 DLL，使用 SemVer 2.0：

```text
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

- `MAJOR`：不兼容的公开 API、数据契约或部署边界变化。
- `MINOR`：向后兼容的新功能。
- `PATCH`：向后兼容的缺陷修复或性能优化。
- `PRERELEASE`：`dev`、`alpha.N`、`beta.N`、`rc.N`。
- `BUILD`：Git revision 和 dirty 状态，只用于诊断。

版本只在仓库根目录 `version-manifest.json` 维护。禁止根据 Git commit 数量、分支名、目录名或
时间戳推导版本。完整规则和发布门禁见 `docs/reference/版本管理/`。

示例：

- `1.2.0-dev`：v1.2 开发版本。
- `1.2.0-rc.1`：v1.2 第一个候选版本。
- `1.2.0`：v1.2 稳定版本。
- `1.2.1`：v1.2 的兼容缺陷修复。
- `2.0.0`：存在不兼容公开 API 变化的稳定版本。


# 五、最佳实践与避坑指南
## 1. 分支保护规则（必须设置）
• main分支：禁止直接提交，只能通过 PR 合并，需要至少 1 人代码评审
• develop分支：禁止直接提交，只能通过 PR 合并
• 所有分支必须通过编译和单元测试才能合并

## 2. 测试分支避坑
• 不要在测试分支上直接修改代码，所有 bug 修复必须在开发分支完成后合并过来
• 不要重复使用同一个测试分支，每轮测试重新拉取新分支
• 不要把未完成的功能合并到测试分支

## 3. 标签管理
• 所有正式版本必须使用 `vMAJOR.MINOR.PATCH` annotated tag，标签与 manifest 稳定版本一致
• 标签必须包含详细的发布说明，包括新增功能、修复 bug、已知问题
• 历史版本标签永远不要删除，用于追溯和回滚

## 4. 多版本并行维护
• 如果需要维护多个历史版本，为每个大版本创建一个维护分支，如 `fix/v1.1.x`、`fix/v1.2.x`
• 单项缺陷从对应维护分支创建 `fix/v1.2-<issue>`，修复后通过评审合并回 `fix/v1.2.x`
• 热修复 bug 先合并到最老的受影响版本，再向上合并到新版本

当前 v1.2 冻结后的分支边界：

1. `develop/v1.1` 继续保留 v1.1 契约，不接收 v1.2 破坏性公开 API 改造。
2. `develop/v1.2` 指向 v1.2 冻结基线，仅接收已经在 v1.2 维护线验证通过的同步修复。
3. `fix/v1.2.x` 从 `v1.2.0` 创建，作为 v1.2.1、v1.2.2 等兼容补丁的长期维护线。
4. `v1.2.0` annotated tag 永久冻结，不在标签上继续提交或重写历史。
5. v1.3 新功能应另建 `develop/v1.3`，不得直接进入 `fix/v1.2.x`。


# 六、Git版本分支流程简化说明
1. 开发：从develop拉取功能分支feature/xxx；开发自测成功后；合并develop分支。
2. 测试：从develop拉取测试分支test/system-vX.Y.Z；系统功能测试通过后；合并develop分支。
3. 预发布：从develop拉取预发布分支release/vX.Y.Z；冻结代码,只修复阻断性bug；生产环境测试通过，进入发布阶段。
4. 正式发布：release/vX.Y.Z合并发布分支到main；打版本标签、合并回develop分支、 删除临时分支。
5. 紧急 bug 走热修复流程。
