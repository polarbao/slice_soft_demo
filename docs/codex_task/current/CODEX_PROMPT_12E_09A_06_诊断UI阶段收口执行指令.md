# CODEX PROMPT 12E-09A-06 诊断 UI 阶段收口执行指令

## 1. 前置

只在 `REPORT_12E_09A_05_同层语义Preview当前状态.md` 明确记录 COMPLETE 后执行。

## 2. 本次只做

```text
运行 09A-01..05 的统一回归和 UI smoke；
验证默认 OpenVDB OFF、生产 TIFF/RIP 和协议不变；
补齐用户说明、阶段报告、索引和上下文；
把 12E-10A 从 WAIT 更新为 READY。
```

## 3. 禁止

```text
不得新增诊断算法；
不得修改生产 package 协议；
不得实施 12E-10A..D；
不得把未运行或失败的验证写成 PASS。
```

## 4. 输出

```text
docs/slice/REPORT/REPORT_12E_09A_诊断UI阶段收口.md
docs/user_guides/SLICE_12E_09A_纹理填充诊断使用说明.md
```

完成后停止，等待用户明确授权 12E-10A。

