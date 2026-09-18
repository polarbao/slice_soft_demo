// F-09 单测缺口的第七批：四个「取值/判定」入口。
//
// 这四个都是同一种东西——**在多个来源之间定优先级**。它们出错的方式也相同：
// 不崩、不报错，只是选了另一个来源，而产物看上去仍然完全合理。
// 所以断言的重点全在【优先级本身】与【条件的与/或结构】，不在返回值长什么样。

#include "Cases.h"

#include "slicer_core/materials/SliceMaterialTexture.h"
#include "slicer_core/output/preview/LayerPreviewWriter.h"
#include "slicer_core/pipeline/SliceProgressNotifier.h"

#include "tests/support/Expect.h"

#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace {

using slicer_core::ModelFillMaterial;
using slicer_core::PreviewConfig;
using slicer_core::SliceConfig;

/// 打开 process_profile 的白墨通道。
SliceConfig ProfileWhite()
{
    SliceConfig config;
    config.material_process_profile.enabled = true;
    config.material_process_profile.white.enabled = true;
    config.material_process_profile.white.mode = "surface";
    return config;
}

PreviewConfig EnabledPreview(const int interval)
{
    PreviewConfig preview;
    preview.enabled = true;
    preview.interval = interval;
    return preview;
}

}  // namespace

int RunPolicyResolverCases()
{
    using namespace slicer_core;
    return slicesoft_test::RunCases(
        "policy resolvers (F-09 batch 7)",
        {
            // ---- materials/SliceMaterialTexture ----
            {"fill material follows a four level precedence",
             [] {
                 // 优先级是 process_profile > material_policy > material_channel > Rgb 兜底。
                 // 调换任意两级都不会崩，只会让默认填充材料换一种——
                 // 产物照出、报告照写，只是整批模型填错了料。
                 SliceConfig both = ProfileWhite();
                 both.material_policy.enabled = true;
                 both.material_policy.varnish.enabled = true;
                 both.material_policy.varnish.mode = "surface";
                 both.material.material_channel = "V";
                 SLICESOFT_EXPECT_TRUE(
                     materials::ResolveProfileDefaultModelFillMaterial(both)
                         == ModelFillMaterial::White,
                     "process_profile 应压过 material_policy 与 material_channel");

                 SliceConfig policyOnly;
                 policyOnly.material_policy.enabled = true;
                 policyOnly.material_policy.varnish.enabled = true;
                 policyOnly.material_policy.varnish.mode = "surface";
                 policyOnly.material.material_channel = "W";
                 SLICESOFT_EXPECT_TRUE(
                     materials::ResolveProfileDefaultModelFillMaterial(policyOnly)
                         == ModelFillMaterial::Varnish,
                     "material_policy 应压过 material_channel");

                 SliceConfig channelOnly;
                 channelOnly.material.material_channel = "W";
                 SLICESOFT_EXPECT_TRUE(
                     materials::ResolveProfileDefaultModelFillMaterial(channelOnly)
                         == ModelFillMaterial::White,
                     "无 profile 与 policy 时看 material_channel");

                 SliceConfig bare;
                 SLICESOFT_EXPECT_TRUE(
                     materials::ResolveProfileDefaultModelFillMaterial(bare)
                         == ModelFillMaterial::Rgb,
                     "都没有时兜底为 Rgb");
             }},
            {"a disabled mode does not count as enabled",
             [] {
                 // 【两个条件缺一不可】通道的 enabled 为真、但 mode 是 disabled 时不算数。
                 // 只看 enabled 会让「开了开关又显式关掉模式」的配置被当成生效。
                 SliceConfig config = ProfileWhite();
                 config.material_process_profile.white.mode = "disabled";
                 SLICESOFT_EXPECT_TRUE(
                     materials::ResolveProfileDefaultModelFillMaterial(config)
                         == ModelFillMaterial::Rgb,
                     "mode 为 disabled 时该通道不应生效");
             }},
            {"surface varnish value needs all three conditions",
             [] {
                 // 三条件与：来源指向 material_policy、policy 开着、policy 的光油通道开着。
                 // 缺任一条都回落到 surface_varnish 自己的取值。
                 SliceConfig config;
                 config.surface_varnish.value = 11U;
                 config.surface_varnish.source = "material_policy";
                 config.material_policy.enabled = true;
                 config.material_policy.varnish.enabled = true;
                 config.material_policy.varnish.value = 77U;
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(materials::ResolveSurfaceVarnishValue(config)), 77,
                     "三条件齐备时取 policy 的值");

                 SliceConfig otherSource = config;
                 otherSource.surface_varnish.source = "profile";
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(materials::ResolveSurfaceVarnishValue(otherSource)), 11,
                     "来源不是 material_policy 时回落");

                 SliceConfig policyOff = config;
                 policyOff.material_policy.enabled = false;
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(materials::ResolveSurfaceVarnishValue(policyOff)), 11,
                     "policy 关闭时回落");

                 SliceConfig varnishOff = config;
                 varnishOff.material_policy.varnish.enabled = false;
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(materials::ResolveSurfaceVarnishValue(varnishOff)), 11,
                     "policy 的光油通道关闭时回落");
             }},

            // ---- output/preview/LayerPreviewWriter ----
            {"preview always covers the first and last layer",
             [] {
                 // 首末层无条件写（只要开着且在范围内）。少了这条，
                 // 间隔不整除时末层预览就没了，而那恰恰是最常被人工核对的一层。
                 const auto preview = EnabledPreview(7);
                 SLICESOFT_EXPECT_TRUE(
                     preview::should_write_preview(preview, 0, 100), "首层必写");
                 SLICESOFT_EXPECT_TRUE(
                     preview::should_write_preview(preview, 99, 100), "末层必写");
                 SLICESOFT_EXPECT_TRUE(
                     preview::should_write_preview(preview, 7, 100), "间隔点应写");
                 SLICESOFT_EXPECT_FALSE(
                     preview::should_write_preview(preview, 8, 100), "非间隔点不写");
             }},
            {"a zero interval does not divide by zero",
             [] {
                 // 间隔取 max(1, interval)，所以配成 0 时是「每层都写」而不是崩。
                 const auto preview = EnabledPreview(0);
                 SLICESOFT_EXPECT_TRUE(
                     preview::should_write_preview(preview, 5, 100), "间隔为零时每层都写");
             }},
            {"the layer range gate runs before the interval",
             [] {
                 // 范围外一律不写，哪怕它是首层或间隔点——两条判据的先后在这里可见。
                 auto preview = EnabledPreview(1);
                 preview.has_layer_range = true;
                 preview.layer_range = {10, 20};
                 SLICESOFT_EXPECT_FALSE(
                     preview::should_write_preview(preview, 0, 100), "范围外的首层也不写");
                 SLICESOFT_EXPECT_TRUE(
                     preview::should_write_preview(preview, 15, 100), "范围内按间隔写");
                 SLICESOFT_EXPECT_FALSE(
                     preview::should_write_preview(preview, 21, 100), "范围外不写");
             }},
            {"a disabled preview writes nothing",
             [] {
                 PreviewConfig off;
                 off.enabled = false;
                 SLICESOFT_EXPECT_FALSE(
                     preview::should_write_preview(off, 0, 100), "关闭时连首层也不写");
             }},

            // ---- pipeline/SliceProgressNotifier ----
            {"cancellation is checked before the callback",
             [] {
                 // 【这条钉的是顺序，不是功能】取消判定在「有没有进度回调」之前。
                 // 调换之后，没设进度回调的调用方就再也取消不掉了——
                 // 而那种调用方恰恰是批处理场景，最需要能中途停下。
                 SliceRunOptions options;
                 options.cancellation_requested = [] { return true; };
                 // 刻意【不】设 progress_callback。
                 std::string thrown;
                 try
                 {
                     progress::NotifyProgress(
                         options, SlicerClock::now(), "slicing", 1, 10, 10);
                 }
                 catch (const std::exception& error)
                 {
                     thrown = error.what();
                 }
                 SLICESOFT_EXPECT_FALSE(
                     thrown.empty(), "无进度回调时取消仍须生效");
             }},
            {"progress percent is clamped to both ends",
             [] {
                 // 百分比越界会让宿主进度条跳出轨道或倒退。两端都钉。
                 std::vector<int> seen;
                 SliceRunOptions options;
                 options.progress_callback =
                     [&seen](const SliceRunProgress& p) { seen.push_back(p.percent); };
                 const auto start = SlicerClock::now();
                 progress::NotifyProgress(options, start, "a", 1, 10, -5);
                 progress::NotifyProgress(options, start, "b", 1, 10, 250);
                 progress::NotifyProgress(options, start, "c", 1, 10, 42);
                 SLICESOFT_EXPECT_EQ(seen.size(), std::size_t{3}, "三次都应回调");
                 SLICESOFT_EXPECT_EQ(seen.at(0), 0, "负值须夹到 0");
                 SLICESOFT_EXPECT_EQ(seen.at(1), 100, "超百须夹到 100");
                 SLICESOFT_EXPECT_EQ(seen.at(2), 42, "范围内原样透传");
             }},
            {"no callback and no cancellation is a silent no-op",
             [] {
                 // 两者都没有时既不抛也不做事——这是最常见的调用形态，不该有副作用。
                 const SliceRunOptions options;
                 std::string thrown;
                 try
                 {
                     progress::NotifyProgress(
                         options, SlicerClock::now(), "slicing", 1, 10, 10);
                 }
                 catch (const std::exception& error)
                 {
                     thrown = error.what();
                 }
                 SLICESOFT_EXPECT_TRUE(thrown.empty(), "空选项不应抛出");
             }},
        });
}
