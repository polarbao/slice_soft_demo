# DOC_PREP_14D-08-R3-02B 修复 Facade 与 Worker 执行准备

> 编制日期：2026-08-06
>
> 对应任务：`14D-08-R3-02B`
>
> 文档状态：`PREPARATION_GATE=PASS / IMPLEMENTATION=COMPLETE`

## 1. 审计结论

`14D-08-R3-02A` 已冻结 repair 请求、job-owned 输出和证据要求。`14A-04-R2` 已进一步冻结首版
仅接受和输出 OBJ，并批准项目内确定性 OBJ/MTL Writer；单模型 strict adapter 的输入身份复用
R3-01B 的 effective Profile 合同。因此本任务的方案和文件所有权准备已经完成。

实现顺序仍固定为先完成 R3-01B 的 Profile 身份与模型重建适配，再实现 Writer、strict adapter、
Facade 和 Worker；禁止把内存 hash、原文件路径或临时 JSON 冒充 repaired asset。

## 2. 当前能力缺口

1. `MeshRepairService` 可产生 `AdaptedTriangleMesh` 与修复证据，但没有生产资产序列化器。
2. 项目当前 importer 支持 OBJ/STL/3MF，不代表具备同格式 exporter。
3. `vcpkg.json` 声明 Assimp，但当前 CMake target 图没有 `find_package(assimp)` 或链接项，运行包也不
   分发 Assimp DLL；不能把它视为已接入 Writer。
4. OBJ 修复若保留 UV/材质，必须同步处理 MTL、纹理相对路径、material assignment 和资源 hash；
   只输出 `v/f` 会破坏 Stage 14 纹理合同。
5. `R3-01A` 是 committed scene + world instance 的权威服务；repair 是单 source asset。二者不能靠
   伪造 scene 或 identity transform 直接等同。

## 3. Writer 候选比较

| 候选 | CMake/vcpkg | 许可证与部署 | 优点 | 风险 | 结论 |
|---|---|---|---|---|---|
| 项目内最小 OBJ/MTL Writer | 不新增依赖；进入 engine target | 项目自有代码；无新增运行库 | 输出可控、确定性强、易做字节 hash | 首版仅 OBJ；必须自行维护 UV、material、MTL 和纹理复制合同 | **已批准为首版实现** |
| Assimp Exporter | 需新增 `find_package(assimp CONFIG REQUIRED)`、target link 和 vcpkg/runtime inventory | BSD-3-Clause，NOTICE 已准备；需复核 DLL/静态 triplet 与插件部署 | 多格式能力较广，复用成熟库 | 当前未链接；导出稳定性、材质映射、二进制部署和版本差异风险较高 | 暂不作为默认；可做后续对照实现 |
| 自建 3MF Writer | 需 ZIP/XML writer 与资源关系实现 | 无新增第三方或复用 miniz | 可保留 3MF package 语义 | 工作量和协议风险最大，超出本轮最窄路径 | 不进入 R3 首版 |

首版固定选择 OBJ：repair capability 仅接受 OBJ source；STL/3MF 返回稳定 unsupported/input
错误，而不是改扩展名或丢属性后成功。该范围收窄需要受控产品/合同确认。

## 4. 推荐首版资产合同

已冻结首版合同：

```text
source format        OBJ + optional MTL + textures
output geometry      deterministic OBJ
output materials     deterministic sibling MTL when source has materials
output textures      byte-identical copies in job-owned repair/resources
path references      normalized relative paths; no scope escape
float formatting     fixed locale and deterministic precision
ordering             source vertex/uv/material order, generated items stable append
```

strict-PASS no-op 也必须复制为 job-owned output，并生成 evidence；不得返回原始路径。

## 5. Strict recheck 适配

需要新增单模型严格复检适配器，固定执行：

```text
write staged repaired asset
  -> import staged asset with the same model-load Profile
  -> verify source/resource/geometry/attribute identities
  -> run complete model-level strict preflight
  -> require complete && strictPass
  -> atomically publish asset and evidence
```

该适配器可复用 `R3-01A` 使用的 topology/admission 组件，但不得伪造 committed scene，也不得遗漏
UV/material/resource evidence。后续如果 repair 由 scene 发起，再由调用方使用真实 committed scene 和
repaired source identity 运行 scene-wide full preflight。

## 6. API/Worker 补充字段

在 `14A-04-R2` 或独立受控 minor 修订中，repair 请求应补充：

```text
modelFormat
profile / profileHash
sourceResourceScope
repairOutputFormat
```

结果证据必须区分：`assetWritten`、`assetReimported`、`strictComplete`、`strictPass`、
`attributesPreserved`、`sourceDigest`、`outputDigest`。缺一不得报告 success。

## 7. 开发拆分

| 子卡 | 内容 | 前置 |
|---|---|---|
| `14D-08-R3-02B-W1` | Writer/格式决策与 fixture | `14A-04-R2` 已确认 |
| `14D-08-R3-02B-W2` | 确定性 OBJ/MTL Writer 与资源复制 | W1 已满足 |
| `14D-08-R3-02B-S1` | 单模型 strict recheck adapter | 01A 组件、01B Profile 身份合同 |
| `14D-08-R3-02B-F1` | ProductionRepairFacadeFactory | W2 + S1 |
| `14D-08-R3-02B-E1` | Worker executor、清理与结果封装 | F1 + 14D-05 发布边界 |

截至 2026-08-06，W2、S1、F1 与 E1 已全部完成。生产 Worker 已精确注册
`geometry.repair`，只接受 job-owned `repair/` 输出，成功时发布修复 OBJ、相邻资源和 strict
证据；取消、输入越界、Profile 漂移和发布异常均 fail-closed，并清理本作业 staging。

Writer 与 strict adapter 可并行；共享 DTO、CMake 和 Worker registry 串行集成。

## 8. 验收矩阵

- strict-PASS no-op 产生 job-owned、可重导入、digest 稳定的资产；
- 退化面/重复面保守修复后 strict PASS；
- UV、material assignment、MTL 和纹理字节保持；
- source 文件及相邻资源不变；
- self-intersection、属性冲突、unsupported format 显式失败；
- 取消/失败清理 staging，不残留可误认成功的资产；
- 双运行输出和 evidence 稳定；
- repair 不触发 slice，不生成生产 Package。

## 9. 门禁

```text
14D_08_R3_02A_PREPARATION_GATE=PASS
14A_04_R2_REPAIR_WRITER_DECISION=PROJECT_OWNED_DETERMINISTIC_OBJ_MTL
14D_08_R3_02B_PREPARATION_GATE=PASS
14D_08_R3_02B_IMPLEMENTATION=COMPLETE
14D_08_R3_02B_E1=COMPLETE
14D_08_R3_02B_IMPLEMENTATION=READY_AFTER_R3_01B_PROFILE_IDENTITY
NEXT_TASK_AFTER_R3_01B=14D_08_R3_02B
```

历史准备标记 `READY_AFTER_R3_01B_PROFILE_IDENTITY` 仅保留实施前依赖记录，不再代表当前状态。
