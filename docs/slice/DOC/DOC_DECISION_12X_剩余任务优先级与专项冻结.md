# DOC_DECISION_12X 剩余任务优先级与专项冻结

> 文档状态：ACCEPTED EXECUTION ORDER / IMPLEMENTATION NOT STARTED
> 版本：v1.0
> 日期：2026-07-27
> 决策范围：12E-09A、12E-10、12F、纹理载体与白色分色专项、Stage 13

## 1. 背景

12A、12B、12C、12D 已完成当前范围；12E 已完成双模式生产写包、Qt 生产入口和 X/Y DPI
兼容。当前剩余工作同时包含：

```text
12E-09A Diagnostic UI；
12E-10 单模型双引擎最终收口；
12F Release 性能工程；
新增但产品策略尚未确认的纹理载体/白色分色/RIP 铺底专项；
Stage 13 模型俯视、实例变换、多模型联合切片和 TIFF 原生预览。
```

如果只按编号继续执行，会让 09A 的配置身份继续绑定单模型，也会让 09A-05/12E-10A
重复建设即将被 Stage 13C 替换的 preview PNG 数据链。

## 2. 当前剩余任务计数

### 2.1 Stage 12 必须收口项

| 工作流 | 未完成原子任务 | 数量 | 当前状态 |
|---|---|---:|---|
| 12E-09A | 09A-02..06 | 5 | 09A-02 等待 13B-01 scene identity |
| 12E-10 | 10A..10D | 4 | 概念级准备；10A 等待 09A-05 和 13C-03 |
| 12F | 12F-02..09 | 8 | 已规划但未激活 |

结论：

```text
只计算 12E 生产语义和诊断收口：剩余 9 个原子任务；
把 12F 性能工程也计入 Stage 12：剩余 17 个原子任务；
12F 不应与 Stage 13 或材料语义改造混在同一个原子任务中。
```

### 2.2 冻结专项

`DOC_PREP_12E_纹理载体与白色分色专项准备.md` 中的 R0..R6 是候选实施路线，不是当前待执行
的 Stage 12 任务。该专项暂按 `12G-TCWS` 候选代号记录：

```text
候选阶段数：7；
当前激活任务数：0；
当前状态：FROZEN / PENDING PRODUCT AND RIP DECISIONS；
不计入 12E 或 12F 的未完成原子任务数量。
```

### 2.3 Stage 13

Stage 13 近程任务为：

```text
13A-01..05：5 个；
13B-01..07：7 个；
13C-01..05：5 个；
合计：17 个近程原子任务。
```

`13A-R2`、`13A-R3`、`13B-R4` 是中长期 Epic，尚未拆成执行级原子任务，不计入上述 17 个。

## 3. 冻结决策

纹理载体、白色分色与 RIP 铺底专项立即冻结，原因是以下产品和设备事实尚未确认：

```text
纯白纹理是 opaque white 还是 transparent knockout；
Texture Carrier 的材料和空间位置；
RGB/W/V 同像素允许的设备组合与打印顺序；
RIP 自动铺底的覆盖来源、关闭方式和证据载体；
pre-RIP TIFF 是否必须自包含全部材料语义；
ripContractId 的 owner、版本兼容和 fail-closed 规则。
```

冻结期间：

```text
不新增配置字段；
不新增 Qt 控件；
不修改 composer、TIFF 协议或 RIP Reader；
不使用 RGB=254 等哨兵补丁；
不把当前白墨/光油行为宣布为该专项最终方案；
允许继续普通 RGB、现有 W/S/V、Legacy/Global 和 Stage 13 工作。
```

解冻必须满足原准备文档 G1..G8，并由用户单独授权成立正式专项。

## 4. 优先级判断

### P0：Stage 13 身份基础

```text
13A-01 ModelTransform/ModelInstance；
13B-01 MultiModelScene/Scene Effective Config。
```

原因：

```text
09A-02 的 subject identity 会被多模型场景直接影响；
模型俯视、实例变换、联合切片都依赖同一 transform/scene revision；
先冻结合同可以避免 UI、preflight、report 和 package 各自发明身份字段。
```

### P1：12E-09A-02

13B-01 完成后立即执行 scene-aware 09A-02。它不应先于 Stage 13 identity，也不应从开发序列删除。

### P1：Stage 13 产品 P0

```text
13A-02..05：单模型俯视、精确变换、镜像和 post-transform preflight；
13B-02..07：模型列表、11x2 排版、幅面/碰撞、联合切片和单 package；
13C-01..03：TIFF Layer Source、材料合成和统一生产预览。
```

### P1：12E-09A-03..06 与 12E-10

```text
09A-03..04 可在 09A-02 后执行；
09A-05 必须等待 13C-03；
09A-06 完成 Diagnostic UI 收口；
12E-10A..D 最后收口当前单模型双引擎基线。
```

12E-10 优先级不是低，而是依赖尚未满足。提前执行只会产生需要重做的 preview 和 config
证据。

### P2：12F 性能工程

12F-02 Release benchmark 可作为独立测量任务插入，但 12F-03..08 涉及生产核心优化，应在 Stage 13
共享 Raster/联合切片边界稳定后执行，避免对即将变化的单模型扫描链做过早优化。

### P3：冻结专项与中长期 3D

```text
12G-TCWS：等待产品/RIP 决策解冻；
13A-R2/R3：等待 13A-R1 和 3D backend Spike；
13B-R4：等待规则排版和联合切片完成后另立自动 nesting 专项。
```

## 5. 推荐执行顺序

单贡献者、单工作树推荐：

```text
1. 13A-01
2. 13B-01
3. 12E-09A-02
4. 13A-02..05
5. 13B-02..07
6. 13C-01..03
7. 12E-09A-03..06
8. 13C-04..05
9. 12E-10A..D
10. 12F-02 基线刷新
11. 根据 benchmark 逐项授权 12F-03..09
12. 产品/RIP 决策完成后再评审是否解冻 12G-TCWS
```

如果需要优先完成 Stage 12 收口，可在第 3 步后先执行 `13C-01..03 -> 09A-03..06 ->
12E-10A..D`，再返回 13A/13B 产品功能；但不能跳过 13A-01/13B-01，也不能让 09A-05 复制旧
preview PNG 合成路线。

## 6. Gate 与停止条件

```text
13A-01 未完成：不得创建多模型 scene schema；
13B-01 未完成：不得执行 scene-aware 09A-02 或多模型生产路由；
设备 buildVolume 未定义：允许场景草稿，不允许多模型 production ready；
13C-03 未完成：不得执行 09A-05 和 12E-10A；
09A-05 未完成：不得把 12E-10 标为 COMPLETE；
12G-TCWS 未解冻：不得实现其配置、resolver、composer、UI 或 RIP contract；
任何任务不得修改 p0.rgbwsv.2、RGBWSV 顺序、uint8 或 black_is_print。
```

## 7. 影响

正向影响：

```text
避免 09A 和 Stage 13 重复建设配置身份；
避免生产预览继续依赖重复 PNG；
保持 12E-10 的证据基线稳定；
把未讨论清楚的材料/RIP 策略从当前代码主线隔离。
```

代价：

```text
12E-10 不再是编号上的立即下一任务；
Stage 13B 的生产 Gate 仍需要设备幅面和性能预算；
12F 算法优化延后到场景/Raster 边界更稳定后。
```

