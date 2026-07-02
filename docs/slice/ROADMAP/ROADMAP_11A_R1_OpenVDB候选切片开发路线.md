# ROADMAP_11A_R1_OpenVDB候选切片开发路线

> 文档版本：v0.1  
> 文档状态：ROADMAP / Stage 11A-R1  
> 生成日期：2026-07-02

---

## 1. Phase Position

11A-R1 位于 11A 与 Stage 12 之间。

```text
11A：确认 OpenVDB 仍是 diagnostic / candidate 前置；
11A-R1：实现 OpenVDB candidate 写包与 preview；
12：基于候选验收结果决定是否产品化双引擎或进入替换路线。
```

---

## 2. Milestones

### M1：入口与防误用

```text
新增 candidate CLI flag；
legacy path 明确拒绝 surface_shell_from_sdf 误用；
candidate 未完成时输出明确错误 / report；
默认 OFF 轨道不变。
```

### M2：strict_closed PASS fixture

```text
新增 closed textured OBJ fixture；
新增 candidate config；
OpenVDB ON 下可通过 topology/admission。
```

### M3：candidate layer buffer

```text
shell/interior/support -> per-layer masks；
SurfaceTextureTransfer -> surface RGB；
MaterialChannelComposer -> RGBWSV buffer；
per-layer stats。
```

### M4：candidate package writer

```text
manifest；
layers TIFF；
reports；
preview；
rip_reader_test PASS。
```

### M5：UI candidate

```text
新增 OpenVDB candidate 按钮；
成功加载 package；
失败加载 report；
显示 admission/blocker。
```

---

## 3. Replacement Decision Gate

OpenVDB 能否替换当前 legacy，至少需要：

```text
closed fixture PASS；
真实 OBJ/3MF 样例 PASS 或有 repair 方案；
RIP summary PASS；
UI preview PASS；
texture fidelity 指标可接受；
性能和内存可接受；
OpenVDB OFF 默认轨道不退化；
连续回归稳定。
```

未满足前，OpenVDB 只作为 Candidate，不作为默认生产引擎。

