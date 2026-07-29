# CODEX PROMPT 12E-09A-05 同层语义 Preview 执行指令

## 1. 必读

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/PRD/PRD_12E_09A_SceneAware诊断UI.md
docs/slice/DEV/DEV_12E_09A_SceneAware诊断UI设计.md
docs/slice/DEMO/DEMO_12E_09A_SceneAware诊断UI验证方案.md
docs/slice/DOC/DOC_PREP_12E_09A_05_同层语义Preview准备.md
docs/codex_task/current/TASKS_12E_09A_诊断UI任务清单.md
```

## 2. 本次只做

```text
扩展 TIFF-native layer metadata，保留 origin/pixel pitch/layer thickness；
按生产像素中心世界坐标把 Texture Surface / Model Fill 映射到当前 TIFF layer；
复用当前层 TIFF buffer 读取同层 S/V；
新增诊断语义预览入口和中文状态；
绑定诊断 identity、生产 scene identity 与真实 layerIndex/zMm；
覆盖空层、缺证据、身份不匹配和非方形 DPI；
更新 09A-05 状态报告、任务清单和总览。
```

## 3. 禁止

```text
不得从 preview PNG 或文件名序号构造同层关系；
不得寻找相邻层或最近层兜底；
不得从 TIFF 反推 Texture Surface / Model Fill；
不得写新的生产 TIFF、manifest、report 或 package；
不得改变 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
不得把 diagnostic 显示为 production PASS；
不得启用或默认切换 OpenVDB。
```

## 4. 实施顺序

```text
1. 先增加物理坐标 mapper 单测；
2. 扩展 ProductionLayerRef/TiffLayerSource 元数据；
3. 实现无 Qt 语义 layer mapper；
4. 复用 LayerPreviewPanel 当前 TIFF buffer；
5. 接入 PreviewWorkspace 与 MainWindow 诊断结果；
6. 增加 UI smoke；
7. 运行定向验证；
8. 更新 REPORT 和状态索引；
9. git diff --check；
10. 按项目提交格式提交。
```

## 5. 通过条件

只有 `DOC_PREP_12E_09A_05_同层语义Preview准备.md` 的 09A-P01..P10 有实际证据，
才可将 09A-05 标记为 COMPLETE，并解锁 09A-06。
