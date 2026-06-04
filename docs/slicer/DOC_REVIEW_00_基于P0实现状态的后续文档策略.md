# DOC_REVIEW_00_基于P0实现状态的后续文档策略

> 文档版本：v0.1  
> 输入依据：`REPORT_00_P0_Demo当前实现状态.md`  
> 适用阶段：P0 Demo 已跑通后的 P0+ 稳定化阶段  
> 结论：当前不建议立即进入全彩纹理 PRD/DEV，应先补齐 P0+ 稳定化文档与 RGBWSV 协议文档。

---

## 1. 当前实现判断

根据 Codex 生成的 `REPORT_00_P0_Demo当前实现状态.md`，当前 Demo 已经不只是空骨架，而是已经完成了 P0 数据闭环的核心能力：

```text
STL/OBJ 导入
配置读取
模型变换
autoOrient 自动摆放
三角面 Z 截面采样
scanline fill
下表面投影支撑
RGBWSV 六通道 layer compose
内部最小 TIFF writer/reader
manifest/reports
rip_reader_test
PPM preview
--inspect-model
--preview-only
```

这说明：

```text
P0 核心闭环已经基本成立。
```

但它仍然不是正式产品级切片器，主要短板集中在：

```text
preview 正式化
模型导入鲁棒性
几何采样鲁棒性
TIFF/RIP 协议文档化
测试样例和自动化验证
```

---

## 2. 后续文档生成策略

当前不建议直接生成并进入：

```text
PRD_01_彩色纹理模型切片
DEV_01_彩色壳体体素与纹理采样设计
```

原因：

1. 当前 P0 数据闭环刚跑通，仍需要稳定化。
2. TIFF/RIP 协议仍未正式文档化。
3. 模型导入仍不支持 binary STL、MTL、Assimp。
4. 几何采样对多轮廓、孔洞、退化面仍需增强。
5. Preview 当前是 PPM，不是 PNG，也没有完整通道统计。

因此下一阶段应定义为：

```text
P0+ 稳定化阶段
```

---

## 3. 建议新增文档

### 3.1 P0+ 稳定化文档

```text
PRD_00A_P0Demo增强_预览导入与几何鲁棒性.md
DEV_00A_P0Demo增强_预览导入与几何鲁棒性设计.md
DEMO_00A_P0Demo增强实施方案.md
```

目的：

```text
把当前 P0 Demo 从“能跑通”提升到“可稳定验证、可持续扩展、可交给后续模块使用”。
```

### 3.2 RGBWSV 协议文档

```text
PRD_03_RGBWSV多通道TIFF协议.md
DEV_03_TIFFWriter与RIPReader协议设计.md
```

目的：

```text
把已经跑通的 TIFF/manifest/rip_reader_test 数据契约固化为正式协议，避免后续彩色、支撑、白墨、光油扩展时协议漂移。
```

---

## 4. 文档编号原则

保留原路线中的：

```text
PRD_01 / DEV_01 = 彩色纹理模型切片
PRD_02 / DEV_02 = 支撑生成与孤岛检测
PRD_03 / DEV_03 = RGBWSV 多通道 TIFF 协议
```

新增 P0+ 文档使用 `00A` 编号，不占用 `01`：

```text
00  = P0 基础 Demo
00A = P0+ 稳定化
01  = 彩色纹理
02  = 高级支撑
03  = RGBWSV 协议
```

---

## 5. 推荐下一步执行顺序

```text
1. 生成并评审 PRD_00A / DEV_00A / DEMO_00A
2. 生成并评审 PRD_03 / DEV_03
3. 让 Codex 按 DEMO_00A 执行 Preview / 导入 / 几何鲁棒性增强
4. 让 Codex 根据 DEV_03 对 TIFF writer / rip_reader_test 做协议化增强
5. P0+ 稳定后，再进入 PRD_01 / DEV_01 彩色纹理模型切片
```

---

## 6. 当前决策

当前阶段建议冻结：

```text
不改变 RGBWSV 通道顺序
不改变 P0 单材料定位
不引入 Unity
不引入 VTK
不立即进入全彩纹理
优先增强 P0 Demo 的可验证性和输入鲁棒性
```
