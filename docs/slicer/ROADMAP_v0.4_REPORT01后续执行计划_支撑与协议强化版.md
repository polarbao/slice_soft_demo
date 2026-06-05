# ROADMAP_v0.4_REPORT01后续执行计划_支撑与协议强化版

> 文档版本：v0.4  
> 文档状态：Draft / 强化修订版  
> 适用阶段：REPORT_01 后  
> 建议提交目录：`docs/slicer/`

---

## 1. 当前状态

当前已完成：

```text
P0：基础切片闭环
00B：uint8 + black_is_print
00A：preview / report / 导入稳定化
00C：Relief V 光油 + S 下表面支撑
PRD_01：2.5D / Relief 正式样例路线
```

当前核心能力：

```text
closed_mesh_scanline
relief_heightfield
materialChannel = V / W / RGB / auto
support.mode = bottom_projection
RGBWSV uint8 TIFF
manifest / reports / preview
rip_reader_test
```

---

## 2. 当前仍缺失的基础能力

当前仍缺失，但在彩色纹理之前必须优先稳定：

```text
1. unsupported_only 支撑模式
2. bottom_projection + unsupported 组合模式
3. layer-to-layer overlap 承托判断
4. connected component island detection
5. SupportType metadata
6. 支撑报告与逐层统计标准化
7. manifest schema 固化
8. RIP Reader 负向测试包
9. 统一 printPixels / emptyPixels 命名
10. regression 脚本
```

---

## 3. 新执行路线

### 阶段 02：支撑生成、孤岛检测与 SupportType 扩展

目标：

```text
把当前单一 bottom_projection 支撑升级为可诊断、可组合、可回归的支撑系统。
```

输出文档：

```text
PRD_02_支撑生成孤岛检测与SupportType扩展_v0.2.md
DEV_02_支撑孤岛检测与SupportType设计_v0.2.md
DEMO_02_支撑与孤岛检测验证方案_v0.2.md
TASKS_02_支撑孤岛检测任务清单_v0.2.md
```

---

### 阶段 03：RGBWSV 协议固化与负向测试

目标：

```text
把当前 RGBWSV TIFF + manifest + rip_reader_test 固化为稳定输入契约。
```

输出文档：

```text
PRD_03_v0.3_RGBWSV协议固化与负向测试.md
DEV_03_v0.3_TIFFWriter_RIPReader协议固化设计.md
TASKS_03_v0.3_RGBWSV协议固化任务清单.md
```

---

### 阶段 04：彩色纹理模型切片

仅在 02 / 03 完成后启动。

目标：

```text
OBJ + MTL + Texture
UV sampling
RGB 真实纹理输出
```

---

## 4. 执行建议

推荐 Codex 执行顺序：

```text
1. 阅读 REPORT_01。
2. 阅读本 ROADMAP_v0.4。
3. 阅读 PRD_02 / DEV_02 / DEMO_02 / TASKS_02 v0.2。
4. 先实现 02。
5. 生成 REPORT_02。
6. 再执行 PRD_03 / DEV_03 / TASKS_03 v0.3。
7. 生成 REPORT_03。
```

不建议：

```text
PRD_02 和 PRD_03 同时大规模改代码
```

可以：

```text
PRD_03 文档先放入仓库，Codex 阅读但暂不执行。
```

---

## 5. 冻结项

后续所有阶段必须保持：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
black_is_print
R G B W S V
Model > Support > Empty
```

---

## 6. 结论

当前路线应从 Relief 正式化转入：

```text
支撑体系化
协议契约化
```

再进入彩色纹理。
