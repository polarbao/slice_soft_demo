# TASKS_PRESET 工艺可配置面收敛与自测用例收口专项任务清单

> 文档状态：**ACTIVE / PC-01..PC-04 COMPLETE（已过 2026-09-04 全量回归，6 失败全为既有）
> / PC-11 定责完成（其中 `stage14f05` 已修，Debug 失败集 6→5）
> / PC-05 方向已定待产品输入 / PC-06 已拆出 `TASKS_ADMISSION_*` 独立专项
> / PC-07..PC-10、PC-12 PROPOSED**
> 版本：v1.6 ｜ 日期：2026-09-06
> ⚠ PC-04 只解决了「可复现 + 转绿」，**没有解决耗时**：它仍占全量 992.5 s 中的 555.5 s（56%）。
> TIMEOUT 已按 §6.7 选项 a 抬至 1800 s（原 900 s 对实测 555/628 s 仅 1.43~1.62 倍余量）
> 定位：不占 Stage 编号的独立专项；本清单为该专项任务状态唯一真源
> 授权：`docs/slice/DOC/DOC_DECISION_TEST_PRESET_2026_09_03_自测用例去别名与T派生收口授权.md`
> 缘起：用户 2026-09-03 提出两问 ——「自测用例是否可删减」「常用工艺预设是否可统一合并」

---

## 0. 恢复点（2026-09-06 交接）

### 0.1 本专项当前状态：已收口，工作树零残留

```text
最后一次提交  cc04b2a
本专项提交    c05c9a6 12023f4 5fac4c0 aa83a45 ece3027 76f3d8e e1f9512 9b2842d 7d31402 cc04b2a
工作树        本专项【无任何未提交改动】
```

### 0.2 唯一未做的验证

```text
✘ 14F05 修复（7d31402）之后【未跑全量回归】。
  已验的是：定向跑原 6 项失败 → 5 失败 + stage14f05 转 Skipped；
            Debug 下 ctest -R stage14f05 报 ***Skipped 并列入「did not run」；
            PowerShell AST 解析 OK、ValidateSet 仍拒绝非法值。
  未验的是：全量 222 项的失败集是否确实由 6 降为 5、且无其他连带影响。
  这一项【应在分支合并完成之后】连同合并复验一起做，单独再跑一次 16.5 分钟不划算。
```

### 0.3 分支合并完成后必须复验的三点

`CMakeLists.txt` 是本专项与 `codex/memflow-bounded-streaming` **唯一的共同改动文件**。
`git merge-tree --write-tree` 判定该合并零冲突（返回单一 tree OID、退出码 0），
但零文本冲突不等于零语义影响，合并后需逐条复验：

```text
① CTest 注册条目数
   本专项收口后 Debug 全量为 222 项（全配置唯一名 234）。
   memflow 会新增 tests/stage16 的 BoundedSupport* 等目标，数字必然上升 ——
   要确认的是「上升量恰等于 memflow 新增的目标数」，而不是把我摘掉的
   9 个纯别名条目又带了回来。核对方式见 §3 的摘除清单。
② stage14f05_local_closure_gate 在 Debug 下仍报 Skipped 而非 Failed
   该行为依赖 CMakeLists 的 SKIP_RETURN_CODE 111 与脚本内
   $script:SkipReturnCode 两处数值一致，合并可能只带来其中一处。
③ hostflow_hd02_real_asset_matrix 仍读清单而非扫盘，且 TIMEOUT 为 1800
   memflow 分支的 tests/hostflow/HostThreeDCanvasTests.cpp 是合并前的旧版本，
   若合并策略偏向该侧，会把 PC-04 的清单驱动改动整体回退。
   判据：tests/hostflow/fixtures/render_ra02_asset_manifest.txt 必须仍存在，
         且该用例在 Debug 下 PASS 而非以 rendered=93 失败。
```

### 0.4 本次「无法合并」的事实记录

用户 2026-09-06 报告无法合并其他分支数据，并归因于本专项的改动。**实测不成立**：

```text
git merge-tree --write-tree HEAD codex/memflow-bounded-streaming
  → 单一 tree OID + 退出码 0，即文本合并零冲突
真实阻塞源  工作树里 2 个【未提交】文件同时出现在 incoming 改动集中：
              docs/slice/README.md
              src/slicer_core/pipeline/MultiModelProductionService.cpp
            git merge 会以「local changes would be overwritten by merge」拒绝。
            这 2 个文件属于并行会话的 HOSTFLOW H-F 工作（当时共 26 个未提交文件），
            与本专项无关；本专项的 10 次提交全部已落盘、工作树零残留。
处置        由 HOSTFLOW 一侧提交或 stash 这 26 个改动后即可合并，本专项无需回退任何提交。
```

留此记录是为了避免后来人在同类情形下先去回退 PRESET 的提交 —— 那不会解除阻塞。

### 0.5 恢复后的下一步

```text
优先  §0.2 的全量回归 + §0.3 的三点合并复验（应合并后一次做完）
其次  PC-05 —— 方向已定（不删 placement），只剩「宿主是否现在暴露 upper/both」
        这一个产品问题，出口已写成 §7.4 的 A/B 两条，等用户择一
其次  TASKS_ADMISSION 专项是否立项（PC-06 拆出，未开工）
其余  PC-07（口径待裁定）、PC-08（受 SHA256 冻结）、PC-09（待 MATVOL 裁定）、
      PC-10、PC-12（低优先级）
```

---

## 1. 固定边界

```text
不删除任何测试源文件、断言或二进制目标；只在证明「无自己的断言」时摘 CTest 注册行。
不改既有工艺预设的 id、显示名、描述与任何材质/纹理/支撑/matvol 字段。
不改 DefaultPresetId。
不改 p0.rgbwsv.2 / p0.rgbwsvt.1、通道顺序、uint8 位深、black_is_print 极性。
不放宽 SourceSizeGuard 与 14E-02 的任何阈值或白名单。
samples/configs/material_process/ 下 15 个工艺文件被 SHA256 钉住
  （HostTransferProfileTests::VerifyLegacyProcessProfileHashes），
  改动必须与该基线同步更新，且必须能跑回归才允许动。
工艺面的任何扩张走「先补预设入口、后合并维度」，不得先合并再补。
```

---

## 2. 状态表

| 卡号 | 任务 | 状态 | 依赖 | 完成日期 |
|---|---|---|---|---|
| PC-00 | 分析、授权文档与本清单 | **COMPLETE** | 用户 2026-09-03 授权 | 2026-09-03 |
| PC-01 | 摘除 9 个纯别名 CTest 条目 | **COMPLETE**（18:00 全量回归零新增失败） | PC-00 | 2026-09-03 |
| PC-02 | T 通道派生改策略单次加载 + 资格位，并补两条门禁 | **COMPLETE**（`matvol_t_host_profile` PASS） | PC-00 | 2026-09-03 |
| PC-03 | 补齐 `RgbWhiteVarnish` 工艺预设入口 | **COMPLETE**（Debug 构建零编译器诊断，定向 4/4 PASS） | PC-02 | 2026-09-04 |
| PC-04 | `hostflow_hd02_real_asset_matrix` 改清单驱动并重固化 | **COMPLETE**（选项 A；含 TIMEOUT 900→1800） | - | 2026-09-04 |
| PC-05 | 宿主 `support.placement` 接线 | **方向已定 / INPUT_OPEN**（取值分布实测排除「删一路」，剩范围待产品输入，见 §7.3-7.4） | - | - |
| PC-06 | 组合准入规则收成单一真源（实测 **29** 条，非 24 条） | **已拆出独立专项** `TASKS_ADMISSION_*`，未开工待立项 | PC-05 建议先收口 | - |
| PC-07 | `materialPolicy` 与 `materialProcessProfile` 交叉校验 | **PROPOSED / 开工前置已完成**（25 个文件中 5 个已不一致，其中≥2 个是误报，口径待裁定，见 §9.2） | - | - |
| PC-08 | 工艺文件 overlay 化与 top-N 参数化 | PROPOSED（受 SHA256 冻结约束） | PC-07 | - |
| PC-09 | W/V 对称预设合并；两条 materialvolume 候选工艺收敛 | PROPOSED / 待 MATVOL 裁定 | PC-06 | - |
| PC-10 | 8 个不在 CTest 内的验证脚本：入 CTest 或删除 | PROPOSED | - | - |
| PC-11 | 既有 6 项回归失败的归属与处置 | **定责 COMPLETE**；其中 `stage14f05` 已修（Debug 失败集 6→5），余 5 项分派各专项，见 §13 | - | 2026-09-04 |
| PC-12 | 断言实参求值顺序导致失败不报原因（全仓 **85 处**） | PROPOSED / **低优先级**（已证非机械改动，批量改写已试并回退，见 §15.2） | PC-04 | - |

---

## 3. PC-01 摘除 9 个纯别名 CTest 条目 — COMPLETE

判定口径：**同一可执行 + 同一参数 + 无区分性 test property**。

| 实际执行的二进制 | 保留 | 摘除 |
|---|---|---|
| `hostflow_hb05_slice_settings_tests` | `hostflow_hb05_slice_settings` | `he03_support_settings`、`he04_material_profile`、`he05_texture_profile` |
| `hostflow_hb08_workspace_state_tests` | `hostflow_hb08_workspace_state` | `he03_support_persistence`、`he04_material_persistence`、`he05_texture_persistence` |
| `stage14d06_public_worker_routing_tests` | 本名 | `stage14d05_r4b_public_worker_artifact_tests`、`stage14d04b_public_worker_cancellation_tests` |
| `hostflow_hb01_model_import_tests` | `hostflow_hb01_model_import` | `hostflow_he02_batch_import` |

**证据：** Debug 全量 231 → 222；全配置唯一名 243 → 234；两个 delta 均恰为 −9。
任务卡证据重映射见授权文档 §1.1。

**未摘除：** `tiff_writer_handwritten_alignment_known_failure_unit_tests` 与
`tiff_writer_alignment_conformance_unit_tests` 命令逐字相同但位于 `if/else` 两支，
互斥且后者带 `WILL_FAIL TRUE`，不是重复。它应随 handwritten 后端退场，属 TIFF 专项。

---

## 4. PC-02 T 通道派生收口 — COMPLETE

**改前的两个问题：**

```text
1) 部署目录 10 个 *_rgbwsvt.json 的 transferChannelPolicy 块逐字节相同，
   而宿主只读这一个块 —— 它们是同一条 T 策略的 10 份副本，
   把文件名写进调用方是虚假的精确；
2) 新增一条基线工艺就必须记得补一次调用。MO-11 时期按需补白正是这么漏掉的。
```

**改后：** `LoadDeployedPolicy()` 按文件名取第一个可严格加载的文件，策略只加载一次；
派生对象由 `hostprocesspreset::transfereligible` 决定，该位与基线工艺定义写在同一处。

**新增两条门禁：**

```text
VerifyDeployedTransferPolicyCopiesAgree
    全部可加载的 *_rgbwsvt.json 的 transferChannelPolicy 必须一致
    —— 「十份副本相同」是「按文件名取第一个」的成立前提，副本漂移属静默故障。
transferPresetCount == eligibleBasePresetCount
    T 工艺条数必须等于标了资格位的基线工艺条数 —— 漏派生会被直接指名。
```

**证据：** 18:00 全量回归 `matvol_t_host_profile` PASS，`transferPresetCount` 仍为 4。

---

## 5. PC-03 补齐 `RgbWhiteVarnish` 工艺预设入口 — COMPLETE

### 5.1 缺口事实

```text
HostMaterialStrategy 六个值中，RgbWhiteVarnish 是【唯一】没有工艺预设入口的一个：
  可在材料面板手工选出（HostMaterialSettingsPanel.cpp:27）
  被 workspace state 持久化（HostWorkspaceState.cpp:35/75）
  有测试覆盖（HostSliceSettingsTests.cpp:328、HostWorkspaceStateTests.cpp:96/189）
  但 8 条基线预设无一使用它

而切片侧早已把它当一等工艺：
  samples/configs/material_process/ 下 6 个 *rgb_white_varnish* 工艺文件
    nail_rgb_white_varnish_top1 / top2 / top3 / top2_regression、
    obj_mtl_texture_rgb_white_varnish、其 _regression、three_mf_texture_rgb_white_varnish
  全部被 VerifyLegacyProcessProfileHashes 的 SHA256 钉住
  samples/scenarios/slicer_scenarios.json 按路径在跑 top1 与 top3
```

### 5.2 已落地内容

新增第 9 条基线工艺，字段按 `obj_mtl_texture_rgb_white_varnish.json` 取：

```text
id           textured_nail_rgb_white_varnish_lower_support
显示名       彩色纹理｜RGB 表层 + 白墨与光油实体填充｜下表面支撑
strategy     HostMaterialStrategy::RgbWhiteVarnish
texture      enabled，top_surface_band（对应工艺文件的 topSurfaceLayers 1）
角色映射     rolemappingenabled = true
             （hostmaterialprocesssettings 的 mapwhitenames / mapvarnishnames 默认 true、
               defaultrole 默认 Rgb，与该工艺 rules_then_default 的三条规则一致）
光油层数     varnishtoplayers 默认 1，对应 top1；top2/top3 由用户在面板改
派生 T       否（与 rgbWhite / rgbVarnish 同因：rolemapping 与 T 的组合未经 MATVOL-T 评审）
插入位置     rgbVarnish 之后，与 RGB+X 家族相邻
```

### 5.3 已完成的静态确认

```text
✔ 不撞任何预设条数断言 —— 全仓仅 HostThreeDCanvasTests.cpp:578 有 count()==7，
  但那是 threeDCameraPresetCombo（相机视角下拉），与工艺下拉无关
✔ 不撞 transferPresetCount == 4 —— 新预设 transfereligible = false
✔ 宿主组合校验不拒绝该组合 —— HostSliceSettings.cpp:300 的按需补白门只在
  whitepolicy == WhiteUnderbase 时触发，新预设用默认 FailClosed；materialvolume 关闭
✔ SourceSizeGuard --self-test PASS，全仓扫描对 HostProcessPresetCatalog.cpp（265 行）零命中
✔ 14E-02 禁止子串（slicer_core / slicer_base / slicer_engine）零命中
```

### 5.4 验证结果（2026-09-03 实测）

```text
✔ Debug 构建退出码 0，编译器诊断 0 条（/W4 /WX）
✔ 定向回归 4/4 PASS
    hostflow_hb01_model_import ....... 0.17 s
    hostflow_hb05_slice_settings ..... 0.36 s   ← 含 VerifyPresetProfileHashClosure
    matvol_t_host_profile ............ 0.11 s   ← transferPresetCount 仍为 4
    hostflow_hb08_workspace_state .... 0.22 s
```

`hostflow_hb05_slice_settings` 通过即证明 9 条基线工艺逐条 Worker 同算法哈希闭合；
`matvol_t_host_profile` 通过即证明 `transferPresetCount == eligibleBasePresetCount == 4`
在新增第 9 条基线工艺后仍成立（新工艺 `transfereligible=false`）。

### 5.5 仍未做的确认

```text
✘ Release /W4 /WX 未跑（本次只验 Debug）
✘ UI Smoke 未做：工艺下拉应为 13 项，新项可选、切换后各面板取值正确
建议  用 model/obj 下任一带 white/varnish 命名材质的资产实跑一次，
      核对 W 与 V 通道确实分别按材料名落位
```

---

## 6. PC-04 `hostflow_hd02_real_asset_matrix` 改清单驱动并重固化 — COMPLETE

> 用户 2026-09-03 在 A/B/C 中选定**选项 A（清单驱动）**并授权执行。
> §6.1–6.3 为诊断记录，§6.5 起为实施与实测结果。

### 6.1 它是回归时长的唯一主导项

```text
hostflow_hd02_real_asset_matrix  实测 555.5 秒，占全量 992.5 秒的 56%
其余 221 项合计                  437 秒
⚠ 更正：本卡 v1.0 曾写「其余 221 项合计约 22 秒」。那是把 CTestCostData.txt 里的
  【平均】cost 求和得来的，而该文件对从未跑完的用例记 0，严重低估真实串行耗时。
  例：stage14d08_r2_slice_executor_tests 记 2.05 s 实为 87.5 s；
      hostflow_hd04_scene_refresh 记 1.00 s 实为 35.7 s。
  下方数字均改用 2026-09-04 全量 Debug 串行实测值。
CTestCostData 历史            7 次运行 cost 恒为 0。⚠ 那是 CTestCostData 对【失败】用例的
                              记法，不代表未执行 —— 本卡 v1.0 曾据此误判为「疑似从未执行」
```

### 6.2 根因：输入集是文件系统扫描，而断言是冻结数字

`tests/hostflow/HostThreeDCanvasTests.cpp` `VerifyRealAssetMatrix()`：

```text
输入  QDirIterator(model/obj, "*.obj", QDir::Files, QDirIterator::Subdirectories)
      —— 递归扫盘，逐个 ImportModel + Refresh
下限  Require(modelPaths.size() >= 36)
断言  Require(renderedCount == 22 && budgetRejectedCount == 0 && assetRejectedCount == 14)
实测  rendered=93 budget=0 asset=2
```

所以往 `model/obj/` 放任何 OBJ，都会**静默改变本用例的输入集**并按资产数线性增加耗时。

### 6.3 关键发现：该冻结基线无法从仓库复现

```text
冻结提交  03b08bb  2026-08-11  test(render): 【R-F-02预算重测】重固化真实资产显示基线
冻结期望  22 + 0 + 14 = 36 个模型
但 03b08bb 时 model/obj 下 git 跟踪的 .obj 只有 31 个
  → 差额 5 个必定来自当时工作树里【未提交】的资产
今天      93 个已跟踪 + 4 个未跟踪（gubao03/gb03.obj、gubao04/gb04.obj、
          finger_suoguo/b-厚度不同/00a.obj、00b.obj）= 97 个
```

**结论：** 这不是今天引入的回归，而是自 2026-08-11 冻结起就一直红的用例；
且因为冻结时的输入集含未入库文件，**任何人都无法从仓库重建那 36 个资产的清单**。
（`03b08bb` 时的 31 个已跟踪资产今天全部仍存在，缺的正是那 5 个从未入库的。）

### 6.4 曾提出的三个选项（用户选 A）

```text
选项 A  改为清单驱动 —— 【已采纳】
选项 B  按今天的 97 个资产重固化 22/0/14 —— 未采纳（每加资产都要再固化一次）
选项 C  移出默认回归 —— 未采纳（把红灯变成静默债）
```

### 6.5 实施内容

**① 输入改为仓库内的冻结清单**

```text
新增  tests/hostflow/fixtures/render_ra02_asset_manifest.txt
      31 行资产路径 + 文件头注释（记明清单来历与「原 22/0/14 为何不可复现」）
      格式：一行一个仓库相对路径；空行与 # 开头的行忽略
改写  VerifyRealAssetMatrix 不再用 QDirIterator 递归扫 model/obj，改读该清单；
      逐条 Require 条目存在；并把原 >= 36 的下限改成 == 31 的精确校验
清单内容取自冻结提交 03b08bb 当时【git 已跟踪】的 31 个 OBJ（今天全部仍存在）
保留  modelRoot 仅用于把证据 CSV 的路径写成相对 model/obj 的形式
```

**② 三元组一次性重固化：22 / 0 / 14 → 29 / 0 / 2**

```text
实测（31 个资产）  rendered=29  budget=0  asset=2   ← 合 31 ✓
```

顺带记录一处**语义变化**：同一批资产里有 7 个从 asset-rejected 变为可渲染，
即资产准入自 2026-08-11 起明显放宽（mesh repair / importer 侧的改进）。
这正是重固化必须留痕、而不能当作「修回去」的原因。

**③ 聚合步骤对齐 22 实例产品预算**

改①②之后暴露出一条此前从未执行到的断言失败：`R-A-02 aggregate import`。
根因是 `HostModelImportWorkflow::ImportModels`（`HostModelImportWorkflow.cpp:94`）
有 `m_instanceModels.size() + modelPaths.size() > 22` 的硬上限 ——
即**场景实例预算 22**，而这个 22 正是 AGENTS.md 记载的
「13B 尚未回签的 22-instance production budget」，不得为迁就用例而抬高。

```text
为什么以前没暴露  2026-08-11 冻结时 renderedCount 恰好就是 22，正顶在预算上；
                  准入放宽后可渲染数升到 29，聚合导入随即被预算拒绝。
                  而在①②修好之前，用例根本走不到聚合这一步（先在三元组处就红了）。
改法              聚合步骤只取前 22 个可渲染资产（kSceneInstanceBudget）。
                  用满预算而非用满资产才是这一步该测的 —— 真实场景放不下 29 个实例。
                  证据文件新增一行 aggregatePaths= 以便区分「可渲染数」与「入场景数」。
影响面            聚合步骤下游【没有任何冻结数字】，只有证据落盘与
                  Require(aggregateRendered) / Require(aggregateTopRendered) 两个布尔断言，
                  因此本改动不触及其他冻结期望。
```

**④ 顺带修一处让失败不报原因的缺陷**

```text
原写法  Require(client.Open(..., &err), err)
        Require(workflow.ImportModels(paths, &imports, &err),
                QStringLiteral("...: %1").arg(err))
问题    把调用写成 Require 的第一个实参、第二个实参又读同一个 err 变量，
        属未指定的实参求值顺序 —— 消息可能在调用写入 err【之前】就构造好，
        失败时打出一个空原因。
实证    首次跑到该断言时的输出正是「H-D-02 FAIL: R-A-02 aggregate import: 」，
        冒号后一片空白，而 ImportModels 明明设置了完整的中文错误消息。
改法    先把调用结果取到 const bool 局部变量，再断言。本函数两处都已改。
        ⚠ 该写法在本仓库其他用例中可能同样存在，值得单独扫一遍（见 PC-12）。
```

### 6.6 实测结果

```text
✔ Debug 构建退出码 0，编译器诊断 0 条
✔ hostflow_hd02_three_d_canvas ....... PASS   12.36 s
✔ hostflow_hd02_real_asset_matrix .... PASS  628.19 s   ← 自 2026-08-11 起首次转绿
```

### 6.7 未解决：耗时问题并没有被这张卡解决

```text
改前  FAIL @ 558 s        —— 从未跑到聚合步骤
改后  PASS @ 628 / 555 s  —— 两次实测（定向 628 s、全量 555 s），波动约 13%
                             不比改前快，因为现在真的执行了聚合导入 + 三维/顶视渲染
```

**收益是「可复现 + 转绿 + 新增资产不再静默改动它」，不是「变快」。**

全量 Debug 串行实测 992.5 秒，本条占 555.5 秒（**56%**）；前 10 项合计 867 秒（87%）：

```text
  555.46 s  hostflow_hd02_real_asset_matrix     ← 本卡
   87.51 s  stage14d08_r2_slice_executor_tests
   49.45 s  matvol_production_wiring_tests
   45.94 s  stage16_contact_leveling_diagnostic_tests
   35.71 s  hostflow_hd04_scene_refresh
   31.47 s  stage16_posture_matrix_tests
   17.44 s  matvol_t_production_matrix_tests
   15.77 s  matvol_reality_plan_tests
   15.30 s  matvol_rgbwsvt_legacy_package_tests
   13.13 s  textured_scene_viewdata_14b03a_unit_tests
  其余 212 项合计 125.3 s
```

即：想缩短回归，砍掉本条只能省一半；真正的分层策略应覆盖上面这 10 项（见下方选项 c）。

```text
⚠ 超时余量曾偏薄：两次实测 555 s 与 628 s，对原 TIMEOUT 900 s 只有 1.62~1.43 倍余量，
  且同机两次就有 13% 波动，较慢的机器上有假失败风险。已按下方 a) 抬至 1800 s。
已处置与后续可选：
  a) TIMEOUT 900 → 1800  ——【已执行】2026-09-04，用户批准。
     不削弱任何断言，只降低假失败率；代价是真卡死要多等一倍才暴露。
     鉴于它此前正因假失败而被当成「卡死」误判，这个取舍是划算的。
  b) 缩减清单规模 —— 属覆盖面决策，须 RENDER 专项裁定，不由本卡代劳。
  c) 分层跑：给耗时项打 label，日常回归 ctest -LE 排除、发布前全跑。
     ⚠ 只排除本条只能省 56%；要把 16.5 分钟压到 2 分钟以内，
       label 必须覆盖上面前 10 项（867 s / 87%），而其中 6 项属 matvol / stage16 专项，
       需各专项分别同意，不是本卡能单方面决定的。
```

---

## 7. PC-05 支撑维度双表达 — **方向已定（不删 placement）/ 范围待产品输入**

```text
切片侧同时存在两套支撑方向/范围表达，值域部分重叠：
  support.mode       bottom_projection / unsupported_only /
                     bottom_projection_plus_unsupported / full_vertical_projection
  support.placement  lower / upper / both / unsupported_only / full_vertical_projection
  由 support.placement_explicit 决定用哪套；不显式时 slicer.cpp:1830 走
  requested_placement = "legacy_mode"，即回落到 mode。

宿主只暴露 mode（HostSupportMode 五值与 mode 一一对应），
而 samples/configs 下 31 个工艺文件用的是 placement。
后果：宿主表达不了 placement 的 upper 与 both。
```

### 7.1 为什么本卡转 INPUT_OPEN 而不是直接开工

原完成标准写成「二择一」：
① 宿主接线 `placement` 暴露 upper/both，并明确两套的优先级；
② 判定只支持一套，删掉另一套并同步 31 个工艺文件（触及 SHA256 冻结）。

**这两条都不是工程判断能决定的**，它们取决于同一个未回答的产品问题：

```text
❓ 产品是否需要「上表面支撑」与「上下双面支撑」？
     需要 → 走①，宿主必须接线 placement，缺它就是真实能力缺口
     不需要 → 走②，placement 的 upper/both 是从未被产品要求过的多余维度，
              连同 placement_explicit 的双轨机制一起删掉才是收敛
```

在答案未知的情况下选任何一条都是赌：选①会为一个可能没人要的能力增加 UI 面与
互斥规则；选②会删掉一个可能正在被某台设备需要的能力，且要动 31 个被哈希钉住的工艺文件。

因此本卡按本仓库既有惯例（参照 13B 的 `buildVolume` / 22-instance 预算）
**标为 INPUT_OPEN，而不是留在 PROPOSED 让人误以为前置已满足**。

### 7.2 回答该问题需要的信息

```text
- 设备侧是否存在需要上表面支撑的打印姿态？（甲片类模型通常只需下表面）
- placement 的 upper/both 是否曾被任何真实工艺用过？
  已知：samples/configs 下 31 个文件用 placement，但未逐个核对取值分布 ——
  这一步可由本专项代做，属纯统计，不需要产品输入。见下方「可先做的准备」
```

### 7.3 取值分布已实测（2026-09-04）—— 方向因此收窄，②被排除

```text
含 support.placement 的工艺文件共 30 个，取值分布：
   23  lower
    3  both   support_placement_both.json
              support_outer_varnish_shell_2px_with_support.json
              cross_section_material_stack_real_obj.json
    2  upper  support_placement_upper.json
              support_upper_surface_outer_varnish_shell.json
    1  full_vertical_projection
    1  unsupported_only
```

**结论：`upper` / `both` 不是无人使用的多余维度**，因此**②（删掉 placement 一路）被排除**。
更关键的是这 5 个非 lower 用例里有 3 个与**外侧光油壳层**语义相关
（`*outer_varnish_shell*`、`cross_section_material_stack_real_obj`）——
上表面光油自然需要上侧支撑，这条能力与 12A 的材料/光油语义是配套的，不是孤立开关。

**顺带纠正一处我在本卡草拟时的误判：** 30 个文件里没有任何一个写 `placementExplicit`，
一度让我怀疑 placement 在运行期根本没生效（`slicer.cpp:1820` 要求
`placement_explicit` 为真才让 placement 驱动 lower/upper/both，否则回落 legacy）。
核实后**该怀疑不成立**：`config.cpp:486-488` 在 JSON 里出现 `placement` 键时
**自动**把 `placement_explicit` 置为 true —— 它不是 JSON 字段，而是「键是否出现」
的内部推导标志。因此这 30 个文件的 placement 全部生效。

### 7.4 收窄后仍未决的那一问

方向已定为①（不删 placement），但**范围**仍需产品输入：

```text
❓ 宿主 UI 是否需要现在就暴露 upper / both？
   生产甲片模型通常只需下表面支撑，而 upper/both 目前只出现在 samples/configs/support/
   的样例工艺里，尚无证据表明某台设备的生产工艺需要它。
   要 → A：宿主接线 placement，暴露 upper/both，并明确 mode 与 placement 的优先级
   不要 → B：保持宿主只暴露 mode，但在卡里明确记「宿主表达不了 upper/both 是
            已知且被接受的缺口」，而不是当作待修缺陷挂着
```

⚠ 无论 A 还是 B，都**不要**再把它写成「二择一的完成标准」然后搁置 ——
现状是它既没被判为缺口也没被判为接受，这种中间态正是本专项想消除的东西。

---

## 8. PC-06 组合互斥规则收成单一准入谓词 — **已拆出独立专项**

```text
「哪些组合合法」这一份知识现存三处：
  ① src/slicer_core/config.cpp   145 条 throw，其中约 24 条是组合互斥 —— 唯一权威
  ② apps/.../HostSliceSettings.cpp  宿主用中文重述（:311 按需补白、:331 matvol+角色映射）
  ③ HostProcessPresetCatalog.cpp    9 条基线预设 = 「已知可行组合」以数据形式再写一遍

②必然不全：核心 24 条，宿主只重述了 4 句。
结构上必然存在「宿主校验通过、切片期才被拒」的组合。
```

**完成标准：** 24 条互斥规则收成一个可查询的准入谓词，作为唯一表达；
宿主改为查询该谓词并在 UI 上置灰，而不是各自重述；
预设目录退化为「该谓词的若干命名解」。

**注意：** `config.cpp` 已在 SourceSizeGuard 的 G2 白名单内，本卡会继续增长该文件，
需在授权文档里说明，或借本卡把谓词抽到独立文件。

---

## 9. PC-07 `materialPolicy` / `materialProcessProfile` 交叉校验 — PROPOSED

```text
samples/configs 下 41 个文件有 materialProcessProfile、33 个有 materialPolicy、
25 个【同时有两个】，二者平行重述同一意图（rgb/white/varnish 的 enabled + mode + 值）。

分工是不对称的：
  只有 materialPolicy（或旧 materialRoleMapping）驱动产出；
  materialProcessProfile 是 report-only —— MaterialProcessReport.cpp:78 在 policy
    关闭时只发一条 warning；
  但 profile.validation.require*Pixels 确实驱动 report 的 pass/fail。

而 config.cpp 的校验（1166-1211 行）只逐块检查各自合法性，从不交叉比对。
即：policy 写 topLayers=1、profile 写 topLayers=2，配置校验全绿，
产出按 1 走、验收断言按 2 判，26 个文件全靠手工同步。
```

### 9.1 开工前置已完成：25 个文件中 5 个已存在不一致

```text
samples/configs/material_closure/real_model_diagnostic_template.json
    white.enabled  policy=false      profile=true
    white.mode     policy=disabled   profile=all_model
samples/configs/material_process/obj_mtl_texture_rgb_varnish.json
    white.mode     policy=disabled   profile=all_model
samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json
    white.enabled  policy=false      profile=true
    white.mode     policy=disabled   profile=unprintable_white_underbase
samples/configs/matvol_t/process_profiles/obj_mtl_texture_rgb_varnish_rgbwsvt.json
    white.mode     policy=disabled   profile=all_model
samples/configs/matvol_t/process_profiles/obj_mtl_texture_rgb_white_ondemand_rgbwsvt.json
    white.enabled  policy=false      profile=true
    white.mode     policy=disabled   profile=unprintable_white_underbase
```

### 9.2 该结果推翻了「两块是纯重复」的初判 —— 口径必须先改

两条 `*_ondemand` 的不一致**不是缺陷，是正确的**：
`config.cpp:1012` 明令 `texture.unprintableWhitePolicy=white_underbase`
**不允许** `materialPolicy.enabled=true`。所以按需补白工艺里白墨根本不由
`materialPolicy` 驱动，而是由 `texture.unprintableWhitePolicy` 驱动；
`materialPolicy.white` 必须关闭，而 report-only 的 `materialProcessProfile.white`
如实描述「白墨来自不可打印纯白底衬」。两块在此处**各说各的事，本就应当不同**。

因此 PC-07 不能是「把 profile 从 policy 派生」，而必须先分清：

```text
待裁定 A  哪些字段【必须】一致（两块都在描述同一驱动源时）
待裁定 B  哪些字段【本就独立】（白墨/光油由 texture 或 matvol 驱动时，
          profile 描述的是最终效果，policy 描述的是它自己不驱动该通道）
待裁定 C  两条 obj_mtl_texture_rgb_varnish 的 white.mode
          policy=disabled / profile=all_model 属于 A 还是 B —— 需逐个核实
          该工艺的白墨实际由谁驱动；若无人驱动则 profile 写错了
```

**完成标准：** 先产出 A/B 字段分类，再只对 A 类加交叉校验；
C 类 2 个文件逐个核实并修正（注意这两个文件在 SHA256 冻结名单内，见固定边界）。

**不要**直接加一条全字段交叉校验：按 9.1 的实测，那会立刻让 5 个文件变红，
其中至少 2 个是误报。

---

## 10. PC-08 工艺文件 overlay 化与 top-N 参数化 — PROPOSED

```text
机械重复（逐条已实测）：
  top1/top2/top3       三个 114 行文件，语义差异只有一个整数 topLayers（1/2/3），
                       且该整数在同一文件内要写两遍（materialPolicy 与 materialProcessProfile）
  *_regression         只差模型路径与 preview 开关；
                       nail_rgb_white_varnish_top2_regression 等价于「top1 + topLayers=2」
  *_rgbwsvt            纯叠加层：= 非 T 版 + channelOrder 追加 "T"
                       + packageProtocol: p0.rgbwsvt.1 + 一个 transferChannelPolicy 块
                       10 个各约 170 行的整体拷贝，表达的是一个 overlay
```

**受约束：** `VerifyLegacyProcessProfileHashes` 钉住 `material_process/` 下 15 个文件的
SHA256；`slicer_scenarios.json` 按路径引用 top1/top3。改动必须同步更新哈希基线，
且必须能跑回归才允许动 —— 见固定边界。

**完成标准：** 引入 base + overlay 的工艺表达，10 个 T 文件退化为 1 个 overlay；
top1/2/3 退化为 1 个模板 + 1 个参数；15 个 SHA 基线同步更新并跑通。

---

## 11. PC-09 预设合并 — PROPOSED / 待 MATVOL 裁定

```text
① W/V 对称的两对，形状完全一致、只差填充通道：
     textured_nail_rgb_white_lower_support ／ textured_nail_rgb_varnish_lower_support
       （均 TopSurfaceBand + rolemappingenabled=true）
     single_material_relief_white ／ single_material_relief_varnish
       （均 texture off + 实体单通道）
   可合为 2 条 + 一个「内部填充通道 W/V」选择器。W 与 V 在 materialPolicy 里是对称通道。
   代价：需给 UI 加通道选择器；会改被测试按 id 断言的预设，属工艺面改动。
   注：PC-03 落地后本组还多出 RgbWhiteVarnish 这一「W 与 V 同时」的第三态，
       合并时应一并纳入设计，避免把它又切成第三条独立预设。

② volumetric_nail_rgb_white_ondemand_lower_support 与
   multilayer_transparent_varnish_lower_support（两条候选、生产接线均未完成）：
   后者在四处严格更强 —— 命名自动推导优先级（而非 primary/secondary 两个名字槽手填）、
   由 MTL d 值判 V 通道、退化面阈值 1e-24、texture 开启。
   且前者 texture=false 的理由已由后者的代码注释记明「M2 落地后已不成立」。
   唯一真实阻碍：前者服务【不遵循 <素材名>-L<层号> 命名】的资产（手填 01/02 优先级）。
   开工前置：MATVOL 裁定这类资产的去向；MV-08 生产接线未完成。
```

---

## 12. PC-10 CTest 之外的验证脚本 — PROPOSED

```text
以下 8 个脚本不被任何 CMakeLists 引用，构建与回归都不会碰到，
只被 docs/slice/REPORT/ 下的报告当手工步骤引用 —— 记不起来就永远不跑：

  tests/contracts/RunSceneFacade14B03Tests.ps1
  tests/contracts/ValidateSceneFacade14B03.py
  tests/contracts/ValidateUiHostPortabilityManifest.py
  tests/stage14b_02/RunTests.ps1
  tests/stage14b_03a/RunIndependent.ps1
  tests/stage14b_03a/ValidateRealFixtures.py
  tests/stage14d_06/ValidateWorkerOnlyHeavyRouting.py
  tests/stage14d_07/RunEngineConformance.py
```

**完成标准：** 逐个判定「入 CTest」或「删除并同步报告文档」，不留中间态。

---

## 13. PC-11 既有回归失败的归属与处置 — **定责完成 / 处置分派各专项**

三次全量回归逐条比对（改前 231 项 → 收口后 222 项）：

| 失败项 | 09-03 10:57 改前 | 09-03 18:00 中途 | **09-04 10:29 收口** | 归属 |
|---|---|---|---|---|
| `slicer_stage14c04_sync_capability_safety_test` | FAIL | FAIL | **FAIL** | 既有 |
| `stage14f03_single_model_s1_gate` | FAIL | FAIL | **FAIL** | 既有 |
| `stage14f05_local_closure_gate` | FAIL | FAIL | **FAIL** | 既有 |
| `scene_layer_adapters_unit_tests` | FAIL | FAIL | **FAIL** | 既有 |
| `slicer_stage14e02_qt_host_boundary_test` | FAIL | FAIL | **FAIL** | 既有（`HostMainWindow.cpp` 502 行未入债务台账） |
| `slicer_stage14e04d_dual_view_contract_test` | FAIL | FAIL | **FAIL** | 既有 |
| `hostflow_hd02_real_asset_matrix` | FAIL | FAIL | **PASS** | 由 **PC-04** 修复 |
| `matvol_white_carrier_integration_tests` | — | FAIL | **PASS** | 非本专项，由 `5d3061a` 修复 |

**结论：7 → 6，唯一变化是 hd02 转绿；PC-01..PC-04 零新增失败。**
`ctest` 退出码 8（6 项失败），222 项中 216 通过，总耗时 992.5 s。

原 v1.0 曾把 stage14f03 / f05 / scene_layer_adapters / hd02 记为「cost 恒为 0，
疑似从未执行」。现已查明：cost 为 0 是 `CTestCostData.txt` 对**失败**用例的记法，
不代表未执行 —— 这四项都确实在跑（hd02 已转绿即为反证）。
下方完成标准据此修正。

**新增项根因已定位：** `MaterialVolumeReport.cpp:201` 加了第 13 个字段
`materialsWithoutPixels`（commit `186c14a`），而 `tests/matvol/MatvolWhiteCarrierTests.cpp:461`
的 `required` 仍是 12 项，第 470 行 `report.as_object().size() == required.size()` 是闭集校验。
该目标只编译 `tests/matvol/MatvolWhiteCarrierTests.cpp` + `slicer_core`，
与 PC-01/02/03 零源码交集。已于 2026-09-03 同步给 slice-soft-demo-0e 与 -54 两个会话，
并由对方于 18:21 以 `5d3061a fix(test): 【闭集登记】materialsWithoutPixels 补入报告必备字段集`
修复 —— 修法是把新字段登记进 `required` 并改为 13 项，而非放宽该闭集断言
（闭集校验的作用正是拦住悄悄溜进 schema 的字段，本次它抓对了）。
该提交经 `fa103b7` 合并已进入 `product/packaged-slicer`，故失败集应回到 7 项，
下一次全量回归可确认。

**结论：PC-01 与 PC-02 零新增失败。** PC-03 落地于本次回归之后，尚未被覆盖。

### 13.1 六项定责结果（2026-09-04 逐条实跑读输出）

**① `stage14f05_local_closure_gate` —— 零信号红灯，结构上永远不可能通过**

```text
Run14F05StageClosureGate.ps1 : Cannot validate argument on parameter 'Config'.
The argument "Debug" does not belong to the set "Release" specified by the ValidateSet attribute.
```

脚本给 `-Config` 加了 `[ValidateSet("Release")]`，而 CTest 注册时传的是 `$<CONFIG>`，
Debug 回归里就是 `Debug`。**它在任何 Debug 回归里都必然失败，且与被测对象无关。**
处置二择一（属 Stage 14F）：把该条只在 Release 配置下注册，或让脚本接受 Debug。
⚠ 它长期红着，等于让 Debug 回归的失败集里恒定多一条噪音，掩盖真实问题。

**② `stage14f03_single_model_s1_gate` —— 疑似门禁侧路径期望过时，产品侧看起来是好的**

```text
[H-A-03] slice.rgbwsv 100%   package.verify 100%   model.release 100%
HOSTFLOW_HA03_PASS sceneHandle=1 revision=3 layers=3
→ Run14F03SingleModelS1Gate.ps1:99  throw "Single-model flow did not publish manifest.json"
```

切片与验包全部走完并报 PASS，随后门禁在第 99 行找不到 `manifest.json`。
优先怀疑门禁脚本的输出路径期望与当前包布局不一致，而非产品缺陷。属 Stage 14F。

**③ `slicer_stage14e02_qt_host_boundary_test` —— 门禁只报 1 项，实际 5 项**

预测已验证：该门禁在首个失败处即中止。逐项核对
`tests/stage14e_02/HostSourceSizeDebtLedger.json` 与实际行数后：

```text
台账内【增长】而未同步下调记录值（台账规则为「只减不增」）：
  HostRipJobController.cpp   登记 1181 → 实际 1377  (+196)   27581fb 2026-08-28 ripflow 手动RIP
  Main.cpp                   登记  751 → 实际  915  (+164)   27581fb / c0b1cf3 matvol-t 合并
  HostSliceSettings.cpp      登记  571 → 实际  592  (+ 21)   931a50f 2026-09-02 hostflow ABI 四字段
台账外且超 500 行：
  HostMainWindow.cpp              502   ← 门禁唯一报出的那一项
  HostPackageReviewController.cpp 501   d0b48d5 2026-08-28
```

三处增长分别来自 RIPFLOW、MATVOL-T、HOSTFLOW 的提交，**均非本专项**；
共同点是改动时没有同步更新债务台账。这正是 MV07-Q2 台账机制想防的静默债，
而它之所以没被发现，是因为门禁首个失败即中止、且长期红着无人看后续。
处置：由各来源专项按 MV07-Q2 的口径补台账（属**门禁放宽，须出授权文档**）或缩减文件。

**④ `scene_layer_adapters_unit_tests` —— 真实窄缺陷，13 个子用例 12 过 1 失**

```text
FAILED: translation preserves local layer bytes and dimensions
FAIL:   legacy_adapter_applies_admitted_instance_transform
（其余 11 项含 orchestrator / global_adapter 全部 PASS）
```

平移后本应保持不变的 local layer 字节与尺寸发生了变化。范围窄、指向明确，
属 `src/slicer_core/pipeline/LegacySceneLayerAdapter.cpp` 一侧。

**⑤ `slicer_stage14e04d_dual_view_contract_test` —— fail-closed 违规**

```text
14E-04d FAIL: missing texture silently became a gray model
```

缺纹理的模型被静默降级成灰模，而非按 fail-closed 拒绝或显式标注。
在一个到处强调「失败即拒绝、不得静默降级」的仓库里，这条的性质比行数门禁严重。

**⑥ `slicer_stage14c04_sync_capability_safety_test` —— 同步能力安全合同被破**

```text
FAIL: scene.get_viewdata first poll was succeeded, expected failed
（随后打印了完整 viewdata，code=PM-SLICER-OK-0000）
```

`scene.get_viewdata` 的首次 poll 本应失败（该能力不得同步应答），现在直接成功返回。
需 Stage 14C 判定是实现回归、还是该期望本身已随异步改造过时。

### 13.2 处置建议与归属

| 失败项 | 性质 | 归属 | 建议 |
|---|---|---|---|
| `stage14f05_local_closure_gate` | 注册/参数 bug，零信号 | Stage 14F | **最先修**，它污染每次 Debug 回归的失败集 |
| `slicer_stage14e04d_dual_view_contract_test` | fail-closed 违规 | Stage 14E / RENDER | 次之，性质最重 |
| `scene_layer_adapters_unit_tests` | 真实窄缺陷 | 切片核心 pipeline | 范围窄，可直接查 |
| `slicer_stage14c04_sync_capability_safety_test` | 合同 vs 实现，需裁定 | Stage 14C | 先判定期望是否过时 |
| `stage14f03_single_model_s1_gate` | 疑似门禁侧路径过时 | Stage 14F | 与①一并处理 |
| `slicer_stage14e02_qt_host_boundary_test` | 静默债，5 项 | RIPFLOW / MATVOL-T / HOSTFLOW | 各自补台账，须授权文档 |

**本专项不代劳任何一项**：①②属 14F，③需各来源专项在自己的授权下补台账，
④⑤⑥分属切片核心与 14C/14E 的语义判断。本卡只负责把定责结论落到这里。

---

## 15. PC-12 断言实参求值顺序导致失败不报原因 — PROPOSED

由 PC-04 §6.5④ 引出。写法：

```cpp
Require(callThatWrites(&error), QStringLiteral("...: %1").arg(error));
//      ^^^^ 第一个实参写 error            ^^^^ 第二个实参读 error
// 两个实参的求值顺序在 C++ 里是未指定的：消息可能在调用写入 error 之前就构造好。
```

**影响：** 只影响**失败时的可诊断性**（打出空的或过期的原因），不影响判定对错。
但它正是本次多花时间的直接原因 —— PC-04 首次跑到聚合断言时输出的是
`H-D-02 FAIL: R-A-02 aggregate import: `，冒号后一片空白，
而 `ImportModels` 明明设置了完整的中文错误消息。

### 15.1 全仓扫描结果：**85 处**（不是初版说的 36 处）

```text
 16  tests/hostflow/HostSliceSettingsTests.cpp
 11  tests/hostflow/HostThreeDCanvasTests.cpp        （PC-04 已修其中聚合段的 2 处）
 10  tests/hostflow/HostSceneRefreshTests.cpp
  9  tests/stage14e_04d/Stage14E04DViewSwitchTests.cpp
  7  tests/stage14e_04/Stage14E04TopViewTests.cpp
  6  tests/stage14e_03/Stage14E03InteractionTests.cpp
  6  tests/stage14e_04c/Stage14E04CThreeDTests.cpp
  5  tests/hostflow/HostDragInteractionTests.cpp
  4  tests/hostflow/HostSliceJobTests.cpp
  3  tests/hostflow/HostTopViewCanvasTests.cpp
  2  tests/hostflow/HostSceneProfileRebindTests.cpp
  2  tests/stage14c_03/ModuleAbiTests.cpp
  1  tests/hostflow/HostModelImportWorkflowTests.cpp
  1  tests/hostflow/HostProfilePanelTests.cpp
  1  tests/hostflow/HostStlImportTests.cpp
  1  tests/matvol_t/HostTransferProfileTests.cpp
其中 4 处位于 || / && 短路链中，提升会改变求值时机。
```

⚠ 本卡 v1.2 曾写「36 处」。那次扫描用的正则只支持一层嵌套括号，
漏掉了实参里还有嵌套调用的站点（如
`topRenderer.Refresh(workflow.SceneHandle(), workflow.SceneRevision(), &frame, &error)`）。
上表改用括号配对扫描，为准。

### 15.2 已尝试批量改写并**回退** —— 它不是机械改动

本卡 v1.2 曾判定这是「纯机械改动，可一次性完成」。**该判断是错的**，
2026-09-04 实际尝试脚本化改写 81 处后回退，原因：

```text
① 需要保留断言的【第二个实参】。脚本把 Require( 到匹配 ) 之间整体替换成了
   新变量名，把失败消息一起吃掉了 —— 好在 Require 是两参签名，直接编译不过而非静默错。
② 变量命名无法机械生成。从「最后一个 identifier(」取名会得到
   clientQByteArrayLiteralOk、topRendererSceneRevisionOk 这类由
   QByteArrayLiteral / SceneRevision 派生的错名字；而同一函数里
   first.Load / restored.Load 这种同名调用还会撞名。
③ 4 处在短路链中，提升会让原本被短路跳过的调用变成无条件执行。
```

### 15.3 修正后的完成标准

```text
不要批量脚本化。按文件逐个手工改，每文件改完单独构建 + 跑该文件对应的用例。
改法见 PC-04 §6.5④：先把调用结果取到 const bool 局部变量，再断言。
优先级：低 —— 它只影响【失败时】的可诊断性，不影响判定对错。
建议按「该用例近期是否真的失败过」排序，而不是按处数多寡；
或者干脆改为「谁将来遇到空原因就地修一处」，不单独立项推平。
```

**注意：** 这些站点只在断言失败时才显形，因此「改完回归依然全绿」是预期结果，
不能作为改对了的证据 —— 必须挑一处临时改坏以确认原因确实被打印出来。

---

## 14. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-03 | v1.0 | 首版。固化 PC-01/PC-02 已完成事实与 18:00 回归证据；记录 PC-03 已落地内容与恢复回归后的确认清单；PC-04 给出「冻结基线无法从仓库复现」的完整证据链与 A/B/C 三个待裁定选项；PC-05..PC-11 列明各自的实测事实、完成标准与开工前置（含 PC-07 必须先扫 25 个文件测出既有不一致、PC-08 受 15 个 SHA256 冻结约束、PC-09 待 MATVOL 裁定）。 |
| 2026-09-06 | v1.6 | 新增 §0 恢复点：本专项已在 cc04b2a 收口、工作树零残留；记明唯一未做的验证是 14F05 修复后未跑全量回归（应与合并复验合并进行）；列出合并后必须复验的三点（CTest 条目数上升量、14F05 仍 Skipped、PC-04 清单驱动未被回退），因为 `CMakeLists.txt` 是本专项与 memflow 唯一的共同改动文件；并记录 2026-09-06「无法合并」的实测归因 —— merge-tree 判定零冲突，真实阻塞是 2 个未提交的 HOSTFLOW 文件与 incoming 改动集重叠，回退 PRESET 提交不会解除阻塞。 |
| 2026-09-04 | v1.5 | 三项后续落地：①`stage14f05_local_closure_gate` 已修 —— 改由脚本在非 Release 时以 `SKIP_RETURN_CODE 111` 跳过，CTest 报 Skipped 而非 Failed，Debug 失败集 6→5；不用 CTest 的 `CONFIGURATIONS` 属性是因为 CTest 4.3.1 下实测它已解析到该属性却仍执行，滤不掉，且「显式跳过」优于「静默缺席」。②PC-06 按建议拆出独立专项 `TASKS_ADMISSION_组合准入规则单一真源收敛专项任务清单.md`，规则条数更正 24→29 并逐条列出。③PC-05 取值分布实测（30 个文件：23 lower、3 both、2 upper、各 1 full_vertical/unsupported_only）**排除了「删掉 placement 一路」**，且非 lower 用例中 3 个与外侧光油壳层语义配套；同时纠正「placementExplicit 未写故 placement 未生效」的误判 —— `config.cpp:486-488` 依 JSON 键是否出现自动置该标志。剩下的是范围问题（宿主是否现在就暴露 upper/both），已写成 A/B 两个明确出口而非搁置的二择一。 |
| 2026-09-04 | v1.4 | PC-11 定责完成：6 项既有失败逐条实跑读输出。关键发现 —— `stage14f05_local_closure_gate` 的脚本给 `-Config` 加了 `ValidateSet("Release")` 而 CTest 传的是 `$<CONFIG>`，在任何 Debug 回归里【结构上永远不可能通过】，是一条零信号红灯，应最先修；`slicer_stage14e02_qt_host_boundary_test` 验证了「门禁首个失败即中止」的预测 —— 报 1 项而实际 5 项（3 项台账内增长未同步下调，来自 RIPFLOW/MATVOL-T/HOSTFLOW，另 2 项台账外超 500 行）；`slicer_stage14e04d_dual_view_contract_test` 是缺纹理静默降级为灰模的 fail-closed 违规；`stage14f03` 流程本身全绿、疑似门禁侧路径期望过时；`scene_layer_adapters_unit_tests` 为 13 过 12 失 1 的窄缺陷；`slicer_stage14c04` 需 14C 判定期望是否已过时。§13.2 给出性质、归属与优先级，并明确本专项不代劳任何一项。 |
| 2026-09-04 | v1.3 | PC-04 §6.7 选项 a 已执行：`hostflow_hd02_real_asset_matrix` 的 TIMEOUT 900→1800（用户批准；实测 555/628 s 对 900 s 仅 1.43~1.62 倍余量）。**PC-12 口径两处更正**：处数由 36 改为 **85**（原扫描正则只支持一层嵌套括号，漏掉实参含嵌套调用的站点）；并撤回「纯机械改动」的判断 —— 实际脚本化改写 81 处后已回退，三条失败原因记于 §15.2（吃掉第二个实参、变量名无法机械生成且会撞名、4 处在短路链中）。完成标准改为逐文件手工改、低优先级，或改为「遇到空原因就地修一处」而不单独推平。 |
| 2026-09-04 | v1.2 | 补 2026-09-04 10:29 全量回归证据：222 项 6 失败，7→6，唯一变化是 hd02 转绿，PC-01..PC-04 零新增失败。**更正 v1.0/v1.1 的耗时数字**：原「其余 221 项合计约 22 秒」系对 `CTestCostData.txt` 的平均 cost 求和所得，而该文件对失败用例记 0，严重低估；实测全量 992.5 s，hd02 占 555.5 s（56%），前 10 项占 867 s（87%），已列出前 10 名单，并据此把 §6.7 选项 c 改为「label 必须覆盖前 10 项、且其中 6 项属别的专项」。同步更正「cost 恒 0 = 疑似从未执行」的错误推断（cost 0 是失败用例的记法）。§13 完成标准改为逐项定责，并记明 14E-02 门禁在首个失败处中止、背后可能还压着别的违规。 |
| 2026-09-03 | v1.1 | PC-03 转 COMPLETE（Debug 零编译器诊断、定向 4/4 PASS，§5.4 落实测数据，§5.5 记明 Release 与 UI Smoke 仍未做）。PC-04 按用户选定的选项 A 执行完毕并转 COMPLETE：清单驱动、三元组 22/0/14→29/0/2 一次性重固化（含「7 个资产从 rejected 变为可渲染」的语义变化留痕）、聚合步骤对齐 22 实例产品预算、并修一处让失败不报原因的实参求值顺序缺陷；§6.7 明确耗时问题未解决且超时余量仅 1.43 倍，列出 a/b/c 三个后续可选项。新增 PC-12：该求值顺序写法全仓 36 处已扫出清单（§15）。 |
