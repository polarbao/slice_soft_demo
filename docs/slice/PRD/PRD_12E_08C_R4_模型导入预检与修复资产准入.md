# PRD_12E-08C-R4 模型导入预检与修复资产准入

> 文档版本：v0.1
> 文档状态：PRD / PREPARED
> 日期：2026-07-21
> 对应决策：DOC_DECISION_12E_08C_R4_模型导入预检与修复资产准入插入专项.md

## 1. 产品目标

1. 模型导入后先完成可解释预检，再允许用户启动当前切片模式。
2. legacy 与 global 使用同一诊断事实，但按各自能力计算准入结果。
3. 正常模型可以继续验证 Texture Surface/Model Fill，不用等待损坏模型修复。
4. 三个 required 真实模型仍需修复后重新审计，不能被其他模型替换。
5. UI、CLI、report 使用同一预检结果，不产生相互矛盾的状态。

## 2. 用户故事

### US-R4-01 导入即知可用性

用户导入 OBJ/3MF 后，界面显示“检测中、通过、警告或阻断”，并列出中文原因、受影响数量和建议动作。

### US-R4-02 模式相关阻断

选择全局纹理壳层时，自相交、开放边、非流形和不完整 strict 证据必须阻止切片；选择传统切片时，只有
传统引擎无法安全处理的 fatal 问题阻断，兼容拓扑问题显示警告。

### US-R4-03 一键入口一致

“导入模型并切片”和“导入模型并 OpenVDB/全局候选切片”必须先调用相同 preflight facade。检测过期时
自动重跑；阻断时停止并保留诊断，不启动切片子进程。

### US-R4-04 正常模型推进功能

至少一个闭合彩色 OBJ 和一个 Texture2D 3MF 可用于验证 0.10mm、中过渡宽度和 allTexture 三个点，证明
Texture Surface 增长、Model Fill 缩小并最终为零。

### US-R4-05 修复资产可审计

外部修复后的 required OBJ 必须登记原身份、新哈希、修复来源、尺寸/姿态变化和 UV/材质/纹理差异；只有
post-strict 与属性验证通过，才能进入 global full chain。

## 3. 功能需求

### FR-01 两阶段检测

```text
Fast Import Check：解析、空模型、非有限坐标、资源可达性和基本数量；
Full Transformed Preflight：最终 transform/autoOrient 后的 topology、自相交、属性和 backend capability。
```

### FR-02 新鲜度

模型文件、MTL/贴图、变换、缩放、姿态、模式、预检选项或算法版本变化后，旧结果必须标记 stale。

### FR-03 模式准入

报告必须同时输出 `legacyAdmission` 和 `globalAdmission`。UI 只依据当前所选模式决定按钮是否可用。

### FR-04 错误提示

每个问题包含稳定 code、severity、count、summary、recommendation 和可选位置摘要。不得只显示“切片失败”。

### FR-05 正常模型与 required 模型治理

新增 clean fixture 可以作为正向功能与 UI 验收；三个 required OBJ 的 caseId 不变，修复版本通过新的
source hash 关联，不能删除原始失败证据。

### FR-06 复杂重建边界

R4 只接收外部修复/独立审计重建结果，不在生产 core 中自动做不唯一的复杂自相交重建。

### FR-07 Texture Surface 宽度

```text
推荐 base minimum = 0.10mm；
UI/config step = 0.01mm；
effective minimum = max(0.10mm, 2 * classificationResolutionMm)；
最大值 = 当前模型动态 allTextureThresholdMm；
达到最大值时 TextureSurface=Model、ModelFill=0。
```

### FR-08 Model Fill 材料

UI 应支持白墨、光油、RGB/自定义和由工艺 Profile 提供的材料角色。C/M/Y/K 作为材料角色显示和保存，
由 `MaterialProcessProfile` 解析到现有 RGBWSV 值；当前协议没有 C/M/Y/K 独立通道，未配置映射时对应选项
必须禁用并说明原因。

### FR-09 输出边界

R4 不新增 writer。legacy 成功仍按当前路径输出 TIFF；global 在 08D 前只输出诊断 report/preview，
`productionOutputWritten=false`。

## 4. UI 要求

```text
导入区显示模型名称、资源状态和预检状态；
诊断面板按“阻断/警告/信息”分组并支持展开；
当前模式被阻断时主切片按钮禁用或点击后停在预检；
提供“重新检测”命令，不提供“忽略 global 错误继续”；
legacy 警告继续时必须显示有效模式为传统切片；
正常模型通过后才启用全局 width 分析；
宽度控件显示推荐下限、有效下限、动态上限和 allTexture 状态；
Model Fill 材料显示用户名称和 resolved material role。
```

## 5. 验收标准

1. 所有一键入口均无绕过 preflight 的路径。
2. global 对四类严格拓扑错误稳定阻断，且不写生产 TIFF。
3. legacy 对同一问题给出兼容警告或 fatal 阻断，并在报告中可追溯。
4. clean OBJ/3MF 正向矩阵证明最小宽度至全纹理的互补不变量。
5. C/M/Y/K 选择不改变 RGBWSV 通道数，并能在 effective config 中看到解析结果或不可用原因。
6. 修复资产未通过属性/post-strict 时不能解除 required-case blocker。

## 6. 非目标

```text
不实现通用 CAD/网格重建器；
不自动上传模型到外部服务；
不引入新 TIFF 通道或改变协议；
不让普通用户选择 OpenVDB/CPU backend；
不以正常 fixture 替换 required 真实模型；
不自动从 global 回退 legacy。
```

