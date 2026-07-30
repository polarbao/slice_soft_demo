# TASKS 13E 甲片自动定向与诊断工作流任务清单

> 文档状态：COMPLETE
> 版本：v1.1
> 日期：2026-07-30
> 顺序：13D COMPLETE -> 13E -> 12E-10A

## 1. 原子任务

### 13E-01 专项准备与证据冻结

状态：`COMPLETE`

```text
记录目标模型与 03/04 的原始/有效 bbox；
冻结浮点平分根因；
生成 DECISION/PRD/DEV/DEMO/TASKS；
声明 13E 优先于 12E-10A。
```

### 13E-02 AutoOrient 确定性修复

状态：`COMPLETE`

```text
新增高度和面积容差；
保持固定候选顺序；
标准长轴甲片等价候选选择 rotate_x_90；
新增生成式 auto_orient_unit_tests。
```

### 13E-03 产品默认高度 9 mm

状态：`COMPLETE`

```text
更新 core/report 默认值；
更新 Qt generated config；
更新普通用户可见生产 Profile；
不机械修改 Golden/负向/OpenVDB fixture。
```

### 13E-04 诊断信息架构调整

状态：`COMPLETE`

```text
右侧“预检”改为“预检与诊断”；
迁入唯一 warningsDiagnosticView；
底部改名“任务详情”；
底部移除“诊断”页，保留详细报告/曲线/耗时/日志。
```

### 13E-05 回归与阶段收口

状态：`COMPLETE`

```text
更新 UI Smoke；
运行 targeted CTest、Qt self-test 和布局 Smoke；
对 03/04/目标模型形成真实方向证据；
更新 REPORT、总览和上下文。
```

### 13E-R1-01 俯视长轴朝向归一化

状态：`COMPLETE`

```text
已平放且 X 占地大于 Y 的甲片自动执行 Z 轴四分之一转；
窄端通过两端 12% 带区横向跨度识别并统一朝 +Y；
已沿 Y 但窄端位于 -Y 的甲片自动执行 Z 轴 180 度旋转；
薄型长轴甲片通过中心/两侧下包络判断 Z 正反面；
AutoOrientReport 输出结构化 rotationDeg；
预检工具消费结构化组合旋转，不再硬编码姿态名称；
Reality 五模型只读检查均为 identity_rotate_z_minus_90；
02/03/04/MF 真实模型批量导入后均满足尖端朝 +Y。
```

## 2. 固定边界

```text
不修改 RGBWSV 协议；
不改变 Legacy/Global 路由；
不启用 OpenVDB 默认路径；
不自动缩放；
不按文件名特判；
不覆盖当前工作树中与 Profile、耗时、构建环境有关的已有修改。
```

## 3. 完成定义

13E-02..05 全部 PASS，且状态报告明确列出实际运行命令后，13E 才可标记 COMPLETE。

## 4. 实际完成证据

```text
auto_orient_unit_tests：PASS；
定向 CTest：7/7 PASS；
Qt self-test：PASS；
workbench-context-inspector：PASS；
workbench-project-diagnostics：PASS；
diagnostics-collapse：PASS；
workspace-layout-sizes：1024x768、1280x720、1440x900 PASS；
Quick CI：PASS；
02.obj、03.obj、04.obj、MF_Mei_gui_wumingzhi_fx04.obj：
批量导入与真实几何 +Y 尖端断言 PASS。
```

状态真源：
`docs/slice/REPORT/REPORT_13E_甲片自动定向与诊断工作流当前状态.md`。
