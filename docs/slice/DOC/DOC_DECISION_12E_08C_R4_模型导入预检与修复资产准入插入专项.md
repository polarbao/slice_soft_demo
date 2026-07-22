# DOC_DECISION_12E-08C-R4 模型导入预检与修复资产准入插入专项

> 决策状态：ACCEPTED / PREPARED
> 日期：2026-07-21
> 插入位置：12E-08C-R3-04 与 12E-08D 之间

> 2026-07-22 修订：固定文件身份条款已由
> `DOC_DECISION_12E_08C_R4_06_真实模型族准入替代规则.md` 更新为爱神/玫瑰/梯田三个模型族准入。

## 1. 背景

12E 的业务目标仍是完整实现 `Texture Surface Layer` 与 `Model Fill Layer` 的互补分区、宽度连续调节、
材料选择和最终生产 TIFF。R3-04 的 NO-GO 只说明当前三个 required OBJ 不能进入严格全局流水线，不代表
12E 的分区算法或 UI 目标被取消。

当前证据同时说明：

```text
三个真实 OBJ 存在 confirmed self-intersection；
现有保守修复器没有尝试复杂自相交重建；
闭合 Texture2D 3MF 已完成 strict/global 诊断链；
legacy 仍可兼容处理部分非闭合或自相交输入；
导入模型后，用户缺少统一、模式相关的可切片状态提示。
```

## 2. 决策

在 12E-08D 前插入 `12E-08C-R4 模型导入预检与修复资产准入` 专项。

该专项采用两条并行但不可互相替代的路线：

```text
正向开发路线：使用 strict PASS 的闭合 OBJ/3MF fixture 继续验证宽度、全纹理和材料分区；
真实准入路线：保留 aishen/meigui/titian required family 身份，接收同族 strict PASS 原始资产、外部人工
              修复或独立审计重建资产，重新执行 hash、属性、完整自相交、post-strict 和 global full chain。
```

跨族正常模型可以推进功能开发，但不得替代三个 required 真实模型族取得生产 GO。

## 3. 不立即实现通用复杂重建器

本专项不在 core 中直接实现通用“复杂自相交自动重建”。原因：

```text
自相交区域不存在唯一修复结果；
体素重建或布尔重建可能改变薄壁、尖角、尺寸和组件关系；
UV、triangle material、Texture2D provenance 很难在重建后无损保持；
引入 CGAL/libigl/商业修复 SDK 会扩大依赖、许可证和部署风险；
当前更紧迫的产品缺口是用户不知道模型为何不能切片。
```

若外部修复流程无法满足真实资产吞吐，再单独建立 `12E-08C-R5 复杂自相交重建预研`，至少比较
CGAL、libigl/自研 BVH 重建和体素表面重建方案后再决定依赖；R4 不预先引入第三方库。

## 4. 模式相关准入

预检问题必须根据当前 `slicePipeline.mode` 计算严重级别：

| 问题 | legacy | global_surface_shell |
|---|---|---|
| 文件无法解析、空模型、非有限坐标、资源缺失且无 fallback | BLOCK | BLOCK |
| confirmed self-intersection | WARN，保留兼容路径并明确风险 | BLOCK |
| boundary/non-manifold/opposite duplicate/winding ambiguity | WARN 或按既有 fatal 规则 | BLOCK |
| strict closed + 属性完整 | PASS | PASS |
| 检测未完成、结果过期或预算不足 | BLOCK 当前启动动作 | BLOCK |

“检测项出现错误则停止”解释为：对当前所选模式，`admission=blocked` 时切片按钮不得继续。相同拓扑问题
在 legacy 中可以是兼容警告，在 global 中必须是错误；UI 不得把 global 错误静默降级为 legacy。

## 5. 12E 产品目标同步

R4 不修改以下既定目标：

```text
Texture Surface widthMm 推荐请求下限 0.10mm，步长 0.01mm；
effectiveMinimum=max(0.10mm, 2 * 最粗分类分辨率)；
宽度可到动态 allTextureThreshold，终点只有 Texture Surface、Model Fill=0；
Model Fill 材料由 Profile 解析为 white、varnish、RGB/custom 或 material role；
C/M/Y/K 在现有 RGBWSV 协议下是工艺材料角色，不新增 TIFF 通道；
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print 不变。
```

## 6. 08D Gate 更新

12E-08D 的前置条件由“等待外部模型”细化为：

```text
R4-01..05 完成预检、准入和正向 fixture；
R4-06 完成三个 required family 候选接收合同，且每族至少一个真实候选通过时才解除 family blocker；
R4-07 四 case strict/global/Release 全链通过；
R4-08 输出 GO；
Quick CI baseline 已解决或显式批准；
用户再次明确授权 production adapter。
```

## 7. 后果

正向结果：用户在导入时即可知道模型是否能走当前模式；正常模型可继续推动 12E UI/分区；生产 Gate
不再依赖口头判断。代价是 12E-08D 继续延后，且三个真实模型仍需外部修复资产或未来独立重建专项。
