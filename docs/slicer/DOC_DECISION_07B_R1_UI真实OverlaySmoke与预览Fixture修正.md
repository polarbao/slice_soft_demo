# DOC_DECISION_07B_R1_UI真实OverlaySmoke与预览Fixture修正

> 文档版本：v0.1  
> 文档状态：Decision / 小版本修正决策  
> 适用阶段：REPORT_07B 之后，进入 08 之前  
> 建议提交目录：`docs/slicer/`  
> 主题：07B 已完成主体收口，但建议先补一个 07B-R1 小修正，再进入 08 支撑形态优化

---

## 1. 判断结论

07B 主体可以收口，但不建议马上进入 08。

建议先执行一个轻量修正阶段：

```text
07B-R1：UI 真实 Overlay Smoke Fixture 与 ConfigDiff 小收口
```

这是小版本修复，不是新的大阶段。

---

## 2. 为什么需要 07B-R1

`REPORT_07B_UI自动化SmokeTest与配置编辑器收口当前实现状态.md` 显示：

```text
1. build 通过；
2. self-test 通过；
3. startup/load-package/save-as-config/chart-load/overlay-load/compare-profiles smoke test 通过；
4. quick regression 通过；
5. 真实 3MF 输出包通过 rip_reader_test；
6. preview 伪彩图修复已完成；
7. preview_report.json 已支持 p0.preview_report.1。
```

但是当前仍有一个关键限制：

```text
overlay-load 在 NailRgbWhiteVarnishTop3 上是 graceful-empty-preview，
原因是该样例 preview.enabled=false。
```

这意味着：

```text
UI overlay 的 smoke test 覆盖了“无图不崩溃”，
但还没有覆盖“真实 RGB+W/V/S overlay 图像加载与切换”。
```

07B 的目标是 UI 自动化 smoke test 收口，因此这个缺口建议在进入 08 前补掉。

---

## 3. 07B-R1 范围

07B-R1 只做：

```text
1. 增加 preview.enabled=true 的 UI smoke fixture；
2. 新增 overlay-load-real 或增强 overlay-load；
3. 自动验证 RGB+W / RGB+V / RGB+S 至少一种真实 overlay image；
4. 可选增强 ConfigDiff：复制路径 / 导出 diff JSON；
5. 生成 REPORT_07B_R1。
```

---

## 4. 07B-R1 不做什么

不做：

```text
支撑形态算法修改；
设备通信；
RIP 半色调；
ICC / CMYK；
OpenVDB；
新的切片算法；
生产级任务系统；
完整 3D viewport。
```

---

## 5. 冻结项

必须保持：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
MaterialPolicy 语义不变
MaterialRoleMapping 语义不变
MaterialProcessProfile 语义不变
Support pipeline 语义不变
```

---

## 6. 完成后路线

07B-R1 完成后再进入：

```text
08：支撑形态与工艺优化
```

原因：

```text
08 将依赖 UI overlay / chart 来观察支撑小岛、支撑断裂、支撑膨胀与材料 profile 的关系。
```
