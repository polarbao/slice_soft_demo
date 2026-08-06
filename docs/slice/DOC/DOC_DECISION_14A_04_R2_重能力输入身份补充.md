# DOC_DECISION_14A-04-R2 重能力输入身份补充

> 文档状态：ACCEPTED / USER AUTHORIZED
>
> 日期：2026-08-06（用户授权）
>
> 适用范围：`geometry.preflight.full`、`geometry.repair`、Worker file contract 映射
>
> 上游：`DOC_DECISION_14A-04-R1`、`DOC_PREP_14D-08-R2`、`DOC_PREP_14D-08-R3-01B/02B`

## 1. Context

Stage 14 已冻结 SPI v1、11 个导出、15 项能力和能力 DTO v1.2。R3-01A 实现后进行 Worker 适配
审计时发现：现有 full preflight 请求只有 scene，没有生产模型加载所需的 effective Profile 和显式
target mode；repair 请求也没有 Profile/资源范围/输出格式身份。

跨进程 Worker 若使用默认配置重导入，会检查或修复与 committed scene 不同的几何，且当前
resource hash 无法识别该差异。继续实现会制造“权威结果看似成功、实际对象错误”的高风险行为。

## 2. Decision（提案）

对既有能力做条件必需字段的 additive minor 修订：

### 2.1 `geometry.preflight` full 模式

新增并要求：

```text
sceneHash
expectedSceneRevision
profile
profileHash
targetMode = legacy | global_surface_shell
```

`buildVolume` 保持可选；出现时必须与 scene 精确一致。

### 2.2 `geometry.repair`

新增并要求：

```text
modelFormat
profile
profileHash
sourceResourceScope
repairOutputFormat
```

首版固定 `modelFormat=obj`、`repairOutputFormat=obj`，采用项目内确定性 OBJ/MTL Writer；
STL/3MF repair 显式返回不支持。后续增加格式必须另立受控修订。

### 2.3 不变量

```text
PM_SPI_VERSION = 1
公开 pm_* 导出 = 11
能力数量 = 15
file_contract_v1 major = 1
生产包 = p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print
```

## 3. Alternatives considered

### A. 只传 scene，Worker 使用默认 Profile

优点是改动最小；缺点是无法重建自定义 transform/auto-orient 几何，且 resource hash 不能发现错误。
**拒绝。**

### B. 把加载后的完整网格写入 Worker request

能消除重导入差异，但会显著扩大 file contract、复制大块几何并引入新的 mesh 资产格式和生命周期。
**不作为当前最窄路径。**

### C. 传 canonical effective Profile 与 hash

复用生产 scene loader，保持请求可审计、可物化、可复现；代价是 DTO v1.2 需要受控 minor 修订。
**推荐。**

## 4. Consequences

1. R3-01B 与 R3-02B 在机器合同更新和合同测试通过前保持阻断；本次用户授权已解除决策阻断。
2. Worker 物化器可复用 R2 scene/Profile 双 hash 和 job-owned staging。
3. 打印侧需要基于修订后的 DTO 重新 ACK；既有 ABI 不需要重新链接。
4. R2-02 slice executor 因依赖 full preflight，继续等待 R3-01B。
5. 本修订批准项目内确定性 OBJ/MTL Writer 作为 repair 首版，不引入 Assimp 运行依赖。

## 5. Validation

授权后必须更新并验证：

```powershell
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateFileContract.py
cmake --build build-slicesoft/main --config Debug --target stage14d08_r3_scene_preflight_tests
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "stage14d08_r3"
cmake --build build-slicesoft/main --config Release --target stage14d08_r3_scene_preflight_tests
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R "stage14d08_r3"
```

并增加 default/custom transform、auto-orient、target mode、scene/Profile/hash stale 的 Worker 负例。

## 6. Follow-up

```text
用户授权本 R2 提案
  -> 更新 slicer_capability_dtos JSON/MD 与合同测试
  -> 实现 14D-08-R3-01B
  -> 实现 14D-08-R2-02
  -> 独立确认 repair Writer 后实现 14D-08-R3-02B
```

本决策已获授权；机器合同实现由 `14A-04-R2` 原子提交完成，生产 Facade/Worker 仍按后续任务开发。
