# DOC_DECISION_11_多模型切片处理范围决策

> 文档版本：v0.1
> 文档状态：DOC_DECISION / Stage 11
> 生成日期：2026-06-30

---

## 1. 决策

11 阶段只新增一个阶段，不额外拆出 12 阶段。

多模型能力纳入 11 阶段，但范围限定为：

```text
能力评估；
数据模型设计；
UI 表达设计；
fixture / report 验证；
必要时实现最小 experimental scene 读取。
```

11 阶段不承诺完整多模型 production 切片输出。

---

## 2. 理由

多模型不是单纯“导入多个文件”：

```text
需要 build volume 和坐标系；
需要 transform / scale / placement；
需要碰撞和重叠诊断；
需要材质、纹理、UV 资源隔离；
需要按 modelId / instanceId 追踪 report；
可能影响支撑、光油、白墨、纹理 transfer；
可能影响输出包和 layer stats。
```

如果过早单独开 12 阶段，会在 09P/10/11 未收口前扩大范围。先放进 11 做决策，更利于保持路线紧凑。

---

## 3. 允许事项

```text
定义 SceneModel / ModelInstance / ModelTransform；
UI 展示多模型列表；
读取多模型 fixture；
生成 multi-model capability report；
评估顺序切片 vs 联合切片；
评估是否需要后续独立阶段。
```

---

## 4. 禁止事项

```text
不默认启用多模型 production 输出；
不修改 p0.rgbwsv.2；
不绕过 geometry admission；
不做复杂自动排版；
不做跨模型支撑联合优化；
不把多模型未验证路径标记为 production-safe。
```

---

## 5. 后续决策点

REPORT_11 必须回答：

```text
多模型是否只保留为 UI/scene 能力；
是否需要后续独立阶段；
是否可以先支持顺序切片；
是否需要联合切片；
是否需要 build volume / nesting / placement 子系统；
是否需要修改输出 package metadata。
```

