# DEMO_10_切片输出契约与纹理保真验证方案

> 文档版本：v0.1
> 文档状态：Formal DEMO / Stage 10
> 生成日期：2026-07-01

---

## 1. 验证目标

验证切片输出是否足够稳定、可解释、可交付给下游 RIP 工程团队。

不验证 RIP 半色调、设备通信、喷头 bitstream 或真实打印效果。

---

## 2. 验证对象

```text
package manifest；
RGBWSV channel metadata；
layer summary；
texture fidelity summary；
material process summary；
diagnostic issue codes；
production admission summary；
downstream handoff checklist。
```

---

## 3. 验收模型集合

建议最小集合：

```text
OBJ + MTL + PNG texture；
3MF BaseMaterial；
3MF ColorGroup；
3MF Texture2DGroup；
white / varnish / support mixed model；
texture fallback model；
surface-shell experimental candidate；
large real-world model for memory stats。
```

具体模型、配置、期望摘要和不可 production-safe 原因见：

```text
docs/slice/DEMO/DEMO_10_RealModelAcceptanceSet.md
```

---

## 4. 验证命令

基础验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
```

Golden / schema：

```powershell
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
```

10 阶段建议新增：

```powershell
.\scripts\run_10_output_contract_tests.ps1
```

---

## 5. 完成判定

REPORT_10 必须说明：

```text
输出契约字段是否稳定；
哪些字段允许下游依赖；
纹理保真指标是否达标；
哪些模型不能进入 release candidate；
哪些信息仍需要下游 RIP 团队反馈；
是否可以进入 11 阶段 UI layer preview。
```
