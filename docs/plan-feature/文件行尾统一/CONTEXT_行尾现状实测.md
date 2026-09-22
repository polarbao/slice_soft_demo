# CONTEXT：行尾现状实测

> 采集日期：2026-09-22 ｜ 口径：`git ls-files` 全量，按原始字节统计，非 git 视图

## 1. 总体分布

| 状态 | 文件数 |
| --- | --- |
| 纯 LF | 1561 |
| 纯 CRLF | 1295 |
| **单文件内混合** | **176** |
| 二进制（含 NUL） | 190 |
| 单行无换行 | 1 |

LF 已是多数，但优势不大（1561 : 1295）；真正的问题是那 176 个混合文件。

## 2. 按后缀

| 后缀 | CRLF | LF | 混合 |
| --- | --- | --- | --- |
| `.md` | 514 | 721 | 63 |
| `.cpp` | 248 | 296 | 63 |
| `.h` | 165 | 227 | 23 |
| `.json` | 64 | 144 | 18 |
| `.obj` | 136 | 32 | 0 |
| `.mtl` | 80 | 26 | 0 |
| `.ps1` | 32 | 59 | 5 |
| `.py` | 14 | 30 | 2 |
| `.toml` | 13 | 0 | 1 |

值得注意：`.obj` / `.mtl` 是**唯二以 CRLF 为主**的文本类，且**零混合**——它们是模型资产，
多半由建模软件按 Windows 习惯导出，从未被人手工编辑过。这两类要单独裁决，见 §4。

## 3. 混合最严重的文件（前 10）

| 文件 | CRLF | 裸 LF |
| --- | --- | --- |
| `docs/archive/.../chat_exports/...` | 5787 | 21127 |
| `src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.cpp` | 1771 | 14 |
| `src/slicer_core/pipeline/MultiModelProductionService.cpp` | 1637 | 16 |
| `src/slicer_core/pipeline/SceneLayerComposer.cpp` | 1506 | 7 |
| `apps/slicer_ui_host_sim/HostRipJobController.cpp` | 1344 | 40 |
| `apps/multi_model_scene_matrix/Main.cpp` | 1202 | 158 |
| `src/slicer_core/scene/MultiModelScene.cpp` | 1187 | 8 |
| `docs/slice/DOC/DOC_PREP_12E_...md` | 888 | 114 |
| `src/slicer_core/layout/SceneCollisionService.cpp` | 798 | 181 |
| `tests/rip_integration/RipIntegrationTests.cpp` | 934 | 40 |

形态分两类：

- **少量裸 LF 混进 CRLF 正文**（如 1771 : 14）——典型是脚本化补丁按 `\n` 插入所致，
  每一处都是一次未被发现的注入。
- **大比例混合**（如 1202 : 158、798 : 181）——多半是多人多工具长期编辑叠加。

## 4. 字节敏感面：不能一把梭的原因

仓里**已经存在**按精确字节校验的文件。现有 `.gitattributes` 只有三行，其中一行就是：

```
# This frozen fixture is verified by its exact SHA-256 bytes.
samples/configs/3mf/three_mf_texture2d_checker.json text eol=lf
```

也就是说「某些文件的字节不能动」这件事**早有先例且已有处理范式**——单独钉住。

另外实测：

- 多个契约校验脚本与单测直接比对文件字节或其哈希
  （`tests/contracts/Validate*.py`、`tests/matvol/MatvolFactsTests.cpp`、
  `tests/unit/production_effective_config/Main.cpp` 等）。
- 模型字节会进 `sourceHash`（`ModelFacadeImplementation.cpp:203`），
  但该哈希在运行时重算、**不签入仓库**；已核查 `tests/golden/expected/` 与
  `samples/configs/scene/` 下出现的 `sourceHash` / `resourceHash` 均为占位串
  （`resource-hash`、`fixture_source_a`），不是真哈希。
- 字节级基线的 11 例清单里**不含任何 64 位十六进制串**，即产出清单不嵌模型哈希；
  其中 `cms_rgbwsvt` 一例的输入确为 `.obj`。

> 因此「改 `.obj` 行尾会打破基线」这个担心**目前看不成立**，但那是推理不是实测：
> OBJ 解析器对行尾是否完全免疫，必须靠改完跑一次字节级基线来证。方案据此安排。

## 5. 根因实证：`core.autocrlf = true`

本机实测：

```
core.autocrlf    = true      （系统级，Git for Windows 安装器默认）
core.eol         = (未设置)
```

`autocrlf=true` 的语义是「提交时 CRLF→LF，检出时 LF→CRLF」。由此可推出仓库现状的成因：

- 若所有人都是 `autocrlf=true`，仓内**本应**统一为 LF。现实是仓内有 1295 个 CRLF 文件，
  说明**存在 `autocrlf=false` 的环境往里提交过**——两种配置并存，仓内就出现两种纯态。
- 混合态则来自脚本化编辑：按 `\n` 插入的内容进了 CRLF 正文，而 `autocrlf` 的转换
  并不会把已在仓内的内容重新理顺。

**一条直接的证据**：本专项这四份文档是按 LF 写盘的，`git add` 时 git 当场警告

```
warning: in the working copy of '...', LF will be replaced by CRLF the next time Git touches it
```

也就是说，**「以后新文件都写 LF」这条规矩，在没有 `.gitattributes` 的情况下根本立不住**
——下次检出就被 `autocrlf` 改回去了。这把 `.gitattributes` 从「建议」变成「必需」。

## 6. 为什么现有 `.gitattributes` 拦不住

它只有三行：`*.pdf binary`、一行注释、一条单文件 `eol=lf`。**没有 `text=auto`**，
于是绝大多数文本文件的行尾完全取决于各人的 `core.autocrlf` 与编辑器设置。

这解释了为什么仓里会同时存在两种纯态和一大批混合态：不是谁做错了一次，
是**根本没有约束**。只做一次性清理而不补这条，过一段时间会原样长回来。
