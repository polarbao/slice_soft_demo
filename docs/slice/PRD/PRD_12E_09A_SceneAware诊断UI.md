# PRD 12E-09A Scene-aware 诊断 UI

> 文档版本：v1.0
> 文档状态：Formal PRD / 09A-03 COMPLETE / 09A-04..06 PREPARED
> 日期：2026-07-29
> 前置：09A-01、13A-01、13B-01 COMPLETE

## 1. 背景

09A-01 已提供只读 Texture/Fill/Partition Diagnostic Facade，但当前诊断入口仍以单一模型和临时配置为
中心。Stage 13 已冻结 `ModelInstance`、`MultiModelScene` 和 Scene Effective Config，诊断 UI 必须在
新增多模型交互前同时理解 `single_model` 与 `scene`，避免继续把 `modelPath` 当作唯一身份。

## 2. 产品目标

```text
保存当前诊断请求和派生值，不覆盖正式 Profile 或 fixture；
single_model 与 scene 使用同一诊断入口；
scene 诊断明确选择一个 current instance，不把整个场景误写成单模型；
配置、分析结果和预览绑定稳定 identity/revision；
提供中文参数、状态、取消和同层语义预览；
诊断结论不得冒充 production admission。
```

## 3. 用户故事

### 3.1 保存诊断配置

用户调整 Texture Surface Layer 宽度和 Model Fill 材料后，可保存到当前 session 的
`slice_config.diagnostic.effective.json`，重新打开后看到 requested、derived、effective 和来源 Profile。

### 3.2 诊断单模型

旧单模型 session 继续工作，身份使用 `subjectType=single_model`、model identity、config hash 和
source Profile。历史 fixture 不强制迁移到 scene schema。

### 3.3 诊断场景实例

scene session 必须显示 sceneId、sceneRevision、current instanceId、transformRevision 和 modelId。
切换实例或 revision 变化后，旧结果标记 stale，不得跨实例复用。

### 3.4 查看同层语义

用户在生产 TIFF 底图上查看 Texture Surface、Model Fill、Partition、Support 和 Varnish 的同层诊断
语义。缺少诊断证据时显示“未提供/未评估”，不从 TIFF 反推业务语义。

## 4. 09A-02 范围

```text
Diagnostic Effective Config schema；
single_model/scene subject identity；
scene current-instance 选择合同；
requested/derived/effective 诊断字段；
原子保存、回读、回退、取消和 stale；
负向配置与稳定错误；
不新增 Qt 参数控件，不启动分析 Worker，不实现预览合成。
```

## 5. 后续范围

```text
09A-03：中文参数控件、状态和 tooltip；
09A-04：可取消异步分析 Worker；
09A-05：基于 13C TIFF 数据源的同层语义叠加；
09A-06：UI smoke、回归、用户文档和阶段收口。
```

## 6. 非目标

```text
不执行多模型排版或联合切片；
不开放新的 Global 生产 Profile；
不修改 09B 产品模式选择；
不把 OpenVDB 作为第三种产品模式；
不修改 RGBWSV TIFF 协议；
不从生产 TIFF 猜测 Texture/Fill/Partition 诊断语义；
不覆盖 samples/configs、模型、scene draft 或 Profile。
```

## 7. 09A-02 验收

```text
single_model 保存/回读/回退 PASS；
scene + current instance 保存/回读/回退 PASS；
sceneId、sceneRevision、instanceId、transformRevision、sceneHash 可审计；
切换 instance 或 revision 后旧配置 stale；
subject/scene/instance/Profile 不匹配 fail-closed；
取消和写失败不留下半成品；
旧单模型 Production Effective Config 回归 PASS；
无 Qt core、无 package/TIFF 写入、无 silent fallback。
```

## 8. 固定协议

```text
p0.rgbwsv.2 不变；
R G B W S V 不变；
uint8 不变；
black_is_print 不变；
Legacy 默认；
OpenVDB 默认关闭。
```
