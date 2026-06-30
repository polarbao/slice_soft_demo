# DEMO_00A_P0Demo增强实施方案

> 文档版本：v0.1  
> 文档状态：Draft / P0+ 实施方案  
> 输入依据：`REPORT_00_P0_Demo当前实现状态.md`

---

## 1. 实施目标

在当前 P0 已跑通的基础上，完成 P0+ 稳定化：

```text
Preview 正式化
Binary STL
OBJ MTL 基础统计
几何采样诊断
Reports 增强
测试样例集
```

---

## 2. 实施顺序

### Step 1：Preview PNG 输出

任务：

- [ ] 保留当前 PPM 输出
- [ ] 新增 PNG 输出
- [ ] 支持 `preview.format = png`
- [ ] 支持 `preview.channels`
- [ ] 支持 `preview.layerRange`
- [ ] 支持 `preview.onlyNonEmptyLayers`

验收：

```text
可以输出 model_rgb_*.png
可以输出 support_s_*.png
preview_report.json 记录每张图片
```

### Step 2：Binary STL

任务：

- [ ] 增加 binary STL 检测
- [ ] 解析 binary STL triangle
- [ ] 计算 bbox
- [ ] 写入 model_report

验收：

```text
cube_binary.stl 可导入
triangleCount 正确
inspect-model 可输出 bbox
```

### Step 3：OBJ MTL 基础统计

任务：

- [ ] 解析 mtllib
- [ ] 解析 usemtl
- [ ] 统计 material face/triangle count
- [ ] 写入 model_report

验收：

```text
0.3.obj 不被破坏
multi_material_stub.obj 可输出 material 统计
```

### Step 4：Contour Report

任务：

- [ ] 每层记录 segmentCount
- [ ] 每层记录 model/support 非零像素
- [ ] 记录 fill warnings
- [ ] 输出 contour_report.json

验收：

```text
contour_report.json 存在
异常层可定位
```

### Step 5：Slice Report 增强

任务：

- [ ] 增加每层 model/support/W/V 非零统计
- [ ] 增加总非零统计
- [ ] 增加空层范围统计

验收：

```text
slice_report.json 可用于判断切片是否合理
```

### Step 6：测试样例集

任务：

- [ ] 增加 cube_ascii.stl
- [ ] 增加 cube_binary.stl
- [ ] 增加 thin_plate.stl
- [ ] 增加 hole_test.obj
- [ ] 增加 multi_material_stub.obj

验收：

```text
samples/configs 下有对应测试配置
```

### Step 7：RIP Reader 错误测试

任务：

- [ ] 缺失 layer 测试
- [ ] 错误 channelOrder 测试
- [ ] 错误 bitDepth 测试
- [ ] layer 尺寸不一致测试

验收：

```text
rip_reader_test 能输出明确错误
```

---

## 3. 不做内容

```text
全彩纹理
3MF
glTF
OpenVDB 替换
Qt UI
VTK
Unity
```

---

## 4. 推荐 Codex 执行提示

```text
请阅读 docs/slicer/PRD_00A_P0Demo增强_预览导入与几何鲁棒性.md、
docs/slicer/DEV_00A_P0Demo增强_预览导入与几何鲁棒性设计.md、
docs/slicer/DEMO_00A_P0Demo增强实施方案.md。

不要修改 RGBWSV 通道顺序。
不要引入 Unity 或 VTK。
优先实现 Step 1：Preview PNG 输出。
完成后更新 REPORT_00_P0_Demo当前实现状态.md。
```

---

## 5. 完成标准

P0+ 完成后：

```text
真实模型调试更容易
preview 可直接查看
模型导入更可靠
几何采样可诊断
RIP 协议更可验证
```
