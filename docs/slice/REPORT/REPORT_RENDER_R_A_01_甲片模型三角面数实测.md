# R-A-01 甲片模型三角面数实测

> 状态：**COMPLETE / P1 CONFIRMED** ｜ ⚠️ **2026-08-09 口径更正：实际触发面比本报告更大**

> 🔴 **口径更正 —— 读结论前必看**
>
> 本报告用「约 13,800 triangles/instance 的预算阈值」判定 17/36 超限。
> 但 `apps/slicer_ui_host_sim/render/SceneRenderPolicy.cpp:46` **把 `lod` 硬编码为 `"lod2"`**，
> 而 `SceneViewMeshBuilder.cpp:93` 中 `Lod2 = 10000U` —— **真实抽稀阈值是 10,000，
> 与 128 MiB 预算无关**。
>
> 按 10k 重算：**35 / 36 超限**，仅 `caihong/5mm.obj`（308 面）幸免。
> 表中判为「否」的 11,680 / 12,736 / 13,386 一批模型**实际同样在被抽稀**。
>
> 同时，`SceneViewCandidateBuilder.cpp:194` 逐实例重建 mesh（外观有缓存、网格没有），
> 使 22 实例排版下预算再膨胀一个数量级。
>
> **因此本报告的数据有效，但「需引入 meshoptimizer」的推论不成立。**
> 完整分析见 `DOC_ANALYSIS_RENDER_RD_B_前置复核_预算膨胀三处根因.md`；
> RD-B 已推迟，须由 **R-A-02**（修完根因后重测）给出裁决依据。
> 日期：2026-08-09
> 统计范围：`model/obj/**/*.obj`，共 36 个文件
> 口径：OBJ 面按 importer 的扇形三角化规则计数，即一个 n 边面计 `n - 2` 个三角面。

## 1. 结论

- 36 个模型中有 **17 个**超过当前约 **13,800 triangles/instance** 的 ViewData 预算阈值；
- 占比为 **47.2%**，最小值 308，最大值 299,980；
- 爱神、玫瑰、奶油、梯田等主要真实纹理资产普遍会进入现有 LOD 跳采样路径；
- 因此 `SceneViewMeshBuilder` 的破洞风险不是理论问题，优先级确认为 **P1**；
- H-D-01 的俯视图使用 `surfacePreview`，不依赖被抽稀的 3D mesh，可按计划完成；
- 本条为历史结论：2026-08-09 已先以 RB-P1 将宿主固定 `lod2` 改为 `auto`，H-D-02
  完成且真实资产矩阵没有显示跳采样破碎网格；R-B-01/02 不再是 H-D-02 硬前置。

## 2. 实测清单

| # | 模型 | 三角面 | 超过 13.8k |
|---:|---|---:|:---:|
| 1 | `aishen_fudiao/ai_shen_wumingzhi_L_tx03.obj` | 79,904 | 是 |
| 2 | `aishen_fudiao/MF_ai_shen_shizhi_L_tx03.obj` | 299,980 | 是 |
| 3 | `aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj` | 84,534 | 是 |
| 4 | `aishen_fudiao/MF_aishen_xiaozhi_L.obj` | 109,538 | 是 |
| 5 | `aishen_fudiao/MF_aishen_zhongzhi_L_tx03.obj` | 131,584 | 是 |
| 6 | `caihong/5mm.obj` | 308 | 否 |
| 7 | `meigui_fudiao/02.obj` | 70,262 | 是 |
| 8 | `meigui_fudiao/03.obj` | 75,596 | 是 |
| 9 | `meigui_fudiao/04.obj` | 76,926 | 是 |
| 10 | `meigui_fudiao/MF_Mei_gui_wumingzhi_fx04.obj` | 50,568 | 是 |
| 11 | `nai_you_new/MF_nai_you.obj` | 117,706 | 是 |
| 12 | `reality/260729-16-39-21-792-segment_101.txt.obj` | 14,774 | 是 |
| 13 | `reality/260729-16-39-48-086-segment_102.txt.obj` | 13,386 | 否 |
| 14 | `reality/260729-16-39-55-435-segment_103.txt.obj` | 13,386 | 否 |
| 15 | `reality/260729-16-40-09-567-segment_104.txt.obj` | 13,386 | 否 |
| 16 | `reality/260729-16-40-21-739-segment_105.txt.obj` | 13,386 | 否 |
| 17 | `reality/260730-13-35-10-849-segment_101.txt.obj` | 14,774 | 是 |
| 18 | `reality/260805-11-50-11-034-segment_101.txt.obj` | 14,774 | 是 |
| 19 | `reality/260805-11-50-54-330-segment_102.txt.obj` | 13,386 | 否 |
| 20 | `reality/260805-11-51-02-243-segment_103.txt.obj` | 13,386 | 否 |
| 21 | `reality/260805-11-51-08-746-segment_104.txt.obj` | 13,386 | 否 |
| 22 | `reality/260805-11-51-15-122-segment_105.txt.obj` | 13,386 | 否 |
| 23 | `shengdanjie_fudiao/star/MF_shengdanjie_zhongzhi_R_fy02.obj` | 23,454 | 是 |
| 24 | `titian_fudiao/dmz.obj` | 95,590 | 是 |
| 25 | `xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | 12,736 | 否 |
| 26 | `xiao_ma_wu_yu_new/MF_Xiao_ma_Shizhi_ty03.obj` | 11,680 | 否 |
| 27 | `xiao_ma_wu_yu_new/MF_Xiao_ma_Wumingzhi_ty03.obj` | 11,680 | 否 |
| 28 | `xiao_ma_wu_yu_new/MF_Xiao_ma_Xiaozhi_ty03.obj` | 11,680 | 否 |
| 29 | `xiao_ma_wu_yu_new/MF_Xiao_ma_Zhongzhi_ty03.obj` | 11,680 | 否 |
| 30 | `yecan/3.obj` | 14,606 | 是 |
| 31 | `yecan/4.obj` | 14,606 | 是 |
| 32 | `小马物语/小马物语大拇指/MF_Xiao_ma_Damuzhi_ty02.obj` | 12,736 | 否 |
| 33 | `小马物语/小马物语食指/MF_Xiao_ma_Shizhi_ty03.obj` | 11,680 | 否 |
| 34 | `小马物语/小马物语无名指/MF_Xiao_ma_Wumingzhi_ty03.obj` | 11,680 | 否 |
| 35 | `小马物语/小马物语小指/MF_Xiao_ma_Xiaozhi_ty03.obj` | 11,680 | 否 |
| 36 | `小马物语/小马物语中指/MF_Xiao_ma_Zhongzhi_ty03.obj` | 11,680 | 否 |

## 3. 后续约束

R-A-01 只形成历史事实证据，不自行授权第三方依赖。H-D-02 已通过 RB-P1 避免无条件
进入跳采样路径；RB-P2（按模型去重）、RB-P3（UV 缝安全顶点共享）和 R-A-02 重测完成后，
再判断是否需要恢复 `meshoptimizer` 与自研保守简化之间的正式选型。
