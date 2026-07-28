# DOC PREP 13B-08-04 真实模型作业流矩阵与收口准备

> 文档状态：IMPLEMENTED / GATE PASS
> 版本：v1.1
> 日期：2026-07-28
> 对应任务：13B-08-04

## 1. 目标

用仓库真实资产关闭批量导入到单一 Package 的功能证据链，并明确 functional PASS 与 production GO
之间仍未关闭的设备输入。

## 2. Fixture 选择

优先从模型资产预检清单中选择无需重建、已获准进入后续开发的模型：

```text
OBJ：至少覆盖单色、OBJ/MTL 纹理和一个真实指甲模型；
3MF：至少覆盖 ColorGroup 或 Texture2D 正向资产；
负向：坏扩展名、坏资源、碰撞、越界、容量超过 22；
规模：1/3/11/12/22 实例。
```
测试必须记录资产路径、hash、Profile、DPI、buildVolume、pipeline mode 和输出 Package 路径。不得用
未获准复杂自相交资产冒充正向结果。

## 3. 验证矩阵

| 维度 | 必测 |
|---|---|
| 导入 | 1、3、部分失败、取消、21+2 容量 |
| 布局 | 11 单行、12 换行、22 上限、越界/碰撞 |
| 输入 | OBJ、OBJ/MTL texture、3MF |
| 模式 | Legacy 正向；Global 按当前 Gate 正向或显式阻断 |
| 输出 | 单 Package、每层一个 RGBWSV TIFF、scene report |
| 预览 | 成功后生产 TIFF 数据源可加载 |
| 协议 | schema、channel order、bitDepth、polarity、RIP strict |
| 构建 | Debug full、Release targeted、Quick CI |

## 4. 阶段声明

设备 buildVolume、原点、轴向或 22 实例预算未关闭时：

```text
允许记录 FUNCTIONAL PASS；
不得记录 PRODUCTION GO；
报告必须列出 INPUT_OPEN 和复测命令。
```

## 5. 自动化与报告

计划新增：

```text
scripts/run_13b_08_scene_workflow.ps1；
必要的 UI smoke case；
REPORT_13B_08_批量导入与当前场景切片当前状态.md；
用户操作说明更新；
Stage 13 总览、矩阵、路线和上下文同步。
```

计划验证：

```powershell
.\scripts\run_13b_08_scene_workflow.ps1 -BuildDir build -Config Debug
.\scripts\run_13b_08_scene_workflow.ps1 -BuildDir build -Config Release
.\scripts\run_ci_quick.ps1
git diff --check
git status --short
```

## 6. 收口 Gate

只有以下全部满足才允许把 13B-08 标记为完成：

```text
01/02/03 原子任务报告和提交存在；
正负向矩阵没有静默跳过；
RIP strict 通过；
Qt 当前场景按钮产生的 Package 与 CLI route 一致；
旧单模型路径和 13C 预览基线不回归；
functional 与 production 声明边界清楚。
```

## 7. 实施结果

2026-07-28 已完成：

```text
Debug/Release 脚本矩阵 PASS；
1/3/11/12/22 和 OBJ/OBJ-MTL/Texture2D 3MF PASS；
所有正向 Package RIP strict PASS；
部分失败、容量、碰撞、越界、stale、cancel 和 Global no-fallback PASS；
Qt 真实三资产通过同一 --scene-config 产品路由写出一个 Package 并回载 TIFF；
设备输入继续标记 INPUT_OPEN，没有宣称 PRODUCTION GO。
```

实际报告：

```text
docs/slice/REPORT/REPORT_13B_08_批量导入与当前场景切片当前状态.md
```
