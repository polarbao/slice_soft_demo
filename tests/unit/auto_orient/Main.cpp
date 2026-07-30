#include "slicer_core/config.h"
#include "slicer_core/model.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{

constexpr double kTolerance{1.0e-9};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

class TemporaryObjFixture
{
public:
    explicit TemporaryObjFixture(const double zOffsetMm = 0.0)
    {
        const auto suffix =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();
        m_root = std::filesystem::temp_directory_path()
            / ("slicesoft_auto_orient_"
               + std::to_string(suffix));
        std::filesystem::create_directories(m_root);
        m_path = m_root / "nail_orientation_tie.obj";

        std::ofstream output{m_path};
        output << std::fixed << std::setprecision(6);
        output
            << "v -5.973302 -3.000000 " << zOffsetMm << '\n'
            << "v 5.973671 -3.000000 " << zOffsetMm << '\n'
            << "v 5.973671 3.000000 " << zOffsetMm << '\n'
            << "v -5.973302 3.000000 " << zOffsetMm << '\n'
            << "v -5.973302 -3.000000 " << 30.371832 + zOffsetMm << '\n'
            << "v 5.973671 -3.000000 " << 30.371832 + zOffsetMm << '\n'
            << "v 5.973671 3.000000 " << 30.371832 + zOffsetMm << '\n'
            << "v -5.973302 3.000000 " << 30.371832 + zOffsetMm << '\n'
            << "v 5.968403 -3.799789 " << 6.351915 + zOffsetMm << '\n'
            << "v -0.240944 4.183847 " << 8.715206 + zOffsetMm << '\n'
            << "f 1 3 2\n"
            << "f 1 4 3\n"
            << "f 5 6 7\n"
            << "f 5 7 8\n"
            << "f 1 2 6\n"
            << "f 1 6 5\n"
            << "f 2 3 7\n"
            << "f 2 7 6\n"
            << "f 3 4 8\n"
            << "f 3 8 7\n"
            << "f 4 1 5\n"
            << "f 4 5 8\n";
    }

    ~TemporaryObjFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(m_root, error);
    }

    const std::filesystem::path& Path() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_root;
    std::filesystem::path m_path;
};

class TemporaryHorizontalNailFixture
{
public:
    explicit TemporaryHorizontalNailFixture(
        const bool tipAtLowX)
    {
        const auto suffix =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();
        m_root = std::filesystem::temp_directory_path()
            / ("slicesoft_horizontal_nail_"
               + std::to_string(suffix));
        std::filesystem::create_directories(m_root);
        m_path = m_root / "horizontal_nail.obj";

        const double lowHalfWidth = tipAtLowX ? 2.0 : 6.0;
        const double highHalfWidth = tipAtLowX ? 6.0 : 2.0;
        std::ofstream output{m_path};
        output << std::fixed << std::setprecision(6);
        output
            << "v -15.000000 " << -lowHalfWidth << " 0.000000\n"
            << "v -15.000000 " << lowHalfWidth << " 0.000000\n"
            << "v 15.000000 " << -highHalfWidth << " 0.000000\n"
            << "v 15.000000 " << highHalfWidth << " 0.000000\n"
            << "v -15.000000 " << -lowHalfWidth << " 2.000000\n"
            << "v -15.000000 " << lowHalfWidth << " 2.000000\n"
            << "v 15.000000 " << -highHalfWidth << " 2.000000\n"
            << "v 15.000000 " << highHalfWidth << " 2.000000\n"
            << "f 1 3 4\n"
            << "f 1 4 2\n"
            << "f 5 6 8\n"
            << "f 5 8 7\n"
            << "f 1 2 6\n"
            << "f 1 6 5\n"
            << "f 3 7 8\n"
            << "f 3 8 4\n"
            << "f 1 5 7\n"
            << "f 1 7 3\n"
            << "f 2 4 8\n"
            << "f 2 8 6\n";
    }

    ~TemporaryHorizontalNailFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(m_root, error);
    }

    const std::filesystem::path& Path() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_root;
    std::filesystem::path m_path;
};

class TemporaryVerticalNailFixture
{
public:
    explicit TemporaryVerticalNailFixture(
        const bool tipAtLowY)
    {
        const auto suffix =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();
        m_root = std::filesystem::temp_directory_path()
            / ("slicesoft_vertical_nail_"
               + std::to_string(suffix));
        std::filesystem::create_directories(m_root);
        m_path = m_root / "vertical_nail.obj";

        const double lowHalfWidth = tipAtLowY ? 2.0 : 6.0;
        const double highHalfWidth = tipAtLowY ? 6.0 : 2.0;
        std::ofstream output{m_path};
        output << std::fixed << std::setprecision(6);
        output
            << "v " << -lowHalfWidth << " -15.000000 0.000000\n"
            << "v " << lowHalfWidth << " -15.000000 0.000000\n"
            << "v " << -highHalfWidth << " 15.000000 0.000000\n"
            << "v " << highHalfWidth << " 15.000000 0.000000\n"
            << "v " << -lowHalfWidth << " -15.000000 2.000000\n"
            << "v " << lowHalfWidth << " -15.000000 2.000000\n"
            << "v " << -highHalfWidth << " 15.000000 2.000000\n"
            << "v " << highHalfWidth << " 15.000000 2.000000\n"
            << "f 1 2 4\n"
            << "f 1 4 3\n"
            << "f 5 7 8\n"
            << "f 5 8 6\n"
            << "f 1 5 6\n"
            << "f 1 6 2\n"
            << "f 3 4 8\n"
            << "f 3 8 7\n"
            << "f 1 3 7\n"
            << "f 1 7 5\n"
            << "f 2 6 8\n"
            << "f 2 8 4\n";
    }

    ~TemporaryVerticalNailFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(m_root, error);
    }

    const std::filesystem::path& Path() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_root;
    std::filesystem::path m_path;
};

class TemporaryArchedNailFixture
{
public:
    explicit TemporaryArchedNailFixture(const bool faceDown)
    {
        const auto suffix =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();
        m_root = std::filesystem::temp_directory_path()
            / ("slicesoft_arched_nail_"
               + std::to_string(suffix));
        std::filesystem::create_directories(m_root);
        m_path = m_root / "arched_nail.obj";

        const double edgeLowerZ = faceDown ? 2.0 : 0.0;
        const double centerLowerZ = faceDown ? 0.0 : 2.0;
        std::ofstream output{m_path};
        output << std::fixed << std::setprecision(6);
        const auto WriteSection =
            [&](const double x, const double halfWidth)
        {
            output
                << "v " << x << ' ' << -halfWidth << ' '
                << edgeLowerZ << '\n'
                << "v " << x << " 0.000000 "
                << centerLowerZ << '\n'
                << "v " << x << ' ' << halfWidth << ' '
                << edgeLowerZ << '\n'
                << "v " << x << ' ' << -halfWidth << ' '
                << edgeLowerZ + 0.5 << '\n'
                << "v " << x << " 0.000000 "
                << centerLowerZ + 0.5 << '\n'
                << "v " << x << ' ' << halfWidth << ' '
                << edgeLowerZ + 0.5 << '\n';
        };
        WriteSection(-15.0, 2.0);
        WriteSection(15.0, 5.0);
        output
            << "f 1 8 2\n"
            << "f 1 7 8\n"
            << "f 2 9 3\n"
            << "f 2 8 9\n"
            << "f 4 5 11\n"
            << "f 4 11 10\n"
            << "f 5 6 12\n"
            << "f 5 12 11\n"
            << "f 1 4 10\n"
            << "f 1 10 7\n"
            << "f 3 9 12\n"
            << "f 3 12 6\n"
            << "f 1 2 5\n"
            << "f 1 5 4\n"
            << "f 2 3 6\n"
            << "f 2 6 5\n"
            << "f 7 10 11\n"
            << "f 7 11 8\n"
            << "f 8 11 12\n"
            << "f 8 12 9\n";
    }

    ~TemporaryArchedNailFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(m_root, error);
    }

    const std::filesystem::path& Path() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_root;
    std::filesystem::path m_path;
};

class TemporaryTallArchedNailFixture
{
public:
    explicit TemporaryTallArchedNailFixture(const bool faceDown)
    {
        const auto suffix =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();
        m_root = std::filesystem::temp_directory_path()
            / ("slicesoft_tall_arched_nail_"
               + std::to_string(suffix));
        std::filesystem::create_directories(m_root);
        m_path = m_root / "tall_arched_nail.obj";

        const double edgeLowerZ = faceDown ? 2.0 : 0.0;
        const double centerLowerZ = faceDown ? 0.0 : 2.0;
        std::ofstream output{m_path};
        output << std::fixed << std::setprecision(6);
        const auto WriteDesiredPoint =
            [&](const double x,
                const double y,
                const double z)
        {
            output << "v " << x << ' ' << z << ' '
                   << -y << '\n';
        };
        const auto WriteSection =
            [&](const double y, const double halfWidth)
        {
            WriteDesiredPoint(
                -halfWidth,
                y,
                edgeLowerZ);
            WriteDesiredPoint(
                0.0,
                y,
                centerLowerZ);
            WriteDesiredPoint(
                halfWidth,
                y,
                edgeLowerZ);
            WriteDesiredPoint(
                -halfWidth,
                y,
                edgeLowerZ + 0.5);
            WriteDesiredPoint(
                0.0,
                y,
                centerLowerZ + 0.5);
            WriteDesiredPoint(
                halfWidth,
                y,
                edgeLowerZ + 0.5);
        };
        WriteSection(-15.0, 2.0);
        WriteSection(15.0, 5.0);
        output
            << "f 1 8 2\n"
            << "f 1 7 8\n"
            << "f 2 9 3\n"
            << "f 2 8 9\n"
            << "f 4 5 11\n"
            << "f 4 11 10\n"
            << "f 5 6 12\n"
            << "f 5 12 11\n"
            << "f 1 4 10\n"
            << "f 1 10 7\n"
            << "f 3 9 12\n"
            << "f 3 12 6\n"
            << "f 1 2 5\n"
            << "f 1 5 4\n"
            << "f 2 3 6\n"
            << "f 2 6 5\n"
            << "f 7 10 11\n"
            << "f 7 11 8\n"
            << "f 8 11 12\n"
            << "f 8 12 9\n";
    }

    ~TemporaryTallArchedNailFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(m_root, error);
    }

    const std::filesystem::path& Path() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_root;
    std::filesystem::path m_path;
};

slicer_core::ModelReport LoadFixture(
    const std::filesystem::path& path,
    const bool autoOrientEnabled,
    const double maxHeightMm)
{
    slicer_core::SliceConfig config;
    config.input.model_path = path;
    config.input.format = "obj";
    config.auto_orient.enabled = autoOrientEnabled;
    config.auto_orient.max_height_mm = maxHeightMm;
    return slicer_core::load_model_report(
        config,
        path.parent_path());
}

bool DefaultHeightIsNineMillimeters()
{
    const slicer_core::SliceConfig config;
    return ExpectTrue(
        std::abs(config.auto_orient.max_height_mm - 9.0)
            <= kTolerance,
        "product auto-orient height default is 9 mm");
}

bool EquivalentCandidatesPreferFrontUp()
{
    const TemporaryObjFixture fixture;
    const slicer_core::ModelReport first =
        LoadFixture(fixture.Path(), true, 6.0);
    const slicer_core::ModelReport second =
        LoadFixture(fixture.Path(), true, 6.0);
    const slicer_core::ModelReport productDefault =
        LoadFixture(fixture.Path(), true, 9.0);

    return ExpectTrue(
               first.auto_orient.selected_orientation
                   == "rotate_x_90_rotate_z_180",
               "equivalent X candidates prefer front-up with tip at positive Y")
        && ExpectTrue(
            second.auto_orient.selected_orientation
                == first.auto_orient.selected_orientation,
            "orientation is deterministic across repeated loads")
        && ExpectTrue(
            productDefault.auto_orient.selected_orientation
                == "rotate_x_90_rotate_z_180",
            "product default keeps the standard nail front-up")
        && ExpectTrue(
            first.bbox_mm.max.y > 30.0
                && std::abs(first.bbox_mm.min.y)
                    <= kTolerance,
            "front-up candidate points its tip toward positive Y")
        && ExpectTrue(
            std::abs(
                (first.bbox_mm.max.z
                 - first.bbox_mm.min.z)
                - 7.983636)
                <= kTolerance,
            "front-up candidate preserves physical thickness");
}

bool ExplicitDisablePreservesSourcePose()
{
    const TemporaryObjFixture fixture;
    const slicer_core::ModelReport report =
        LoadFixture(fixture.Path(), false, 9.0);

    return ExpectTrue(
               report.auto_orient.selected_orientation
                   == "identity",
               "explicit auto-orient disable preserves identity")
        && ExpectTrue(
            std::abs(
                (report.bbox_mm.max.z
                 - report.bbox_mm.min.z)
                - 30.371832)
                <= kTolerance,
            "disabled auto-orient preserves source height");
}

bool WithinHeightLimitPreservesSourcePose()
{
    const TemporaryObjFixture fixture;
    const slicer_core::ModelReport report =
        LoadFixture(fixture.Path(), true, 31.0);

    return ExpectTrue(
               report.auto_orient.selected_orientation
                   == "identity",
               "source pose remains when it already fits")
        && ExpectTrue(
            std::abs(
                (report.bbox_mm.max.z
                 - report.bbox_mm.min.z)
                - 30.371832)
                <= kTolerance,
            "fitting source height remains unchanged");
}

bool WithinHeightLimitGroundsEnabledSource()
{
    constexpr double kSourceOffsetMm{303.731};
    const TemporaryObjFixture fixture(kSourceOffsetMm);
    const slicer_core::ModelReport report =
        LoadFixture(fixture.Path(), true, 31.0);

    return ExpectTrue(
               report.auto_orient.selected_orientation
                   == "identity",
               "fitting offset source keeps identity orientation")
        && ExpectTrue(
            std::abs(
                report.auto_orient.original_bbox_mm.min.z
                - kSourceOffsetMm)
                <= kTolerance,
            "inspection preserves original source Z offset")
        && ExpectTrue(
            std::abs(report.bbox_mm.min.z)
                <= kTolerance,
            "enabled auto-orient grounds fitting source to build plate")
        && ExpectTrue(
            std::abs(
                (report.bbox_mm.max.z
                 - report.bbox_mm.min.z)
                - 30.371832)
                <= kTolerance,
            "grounding preserves physical source height");
}

bool HorizontalNailsUseAConsistentPositiveYHeading()
{
    const TemporaryHorizontalNailFixture lowTipFixture(true);
    const TemporaryHorizontalNailFixture highTipFixture(false);
    const slicer_core::ModelReport lowTip =
        LoadFixture(lowTipFixture.Path(), true, 9.0);
    const slicer_core::ModelReport highTip =
        LoadFixture(highTipFixture.Path(), true, 9.0);
    const slicer_core::ModelReport disabled =
        LoadFixture(lowTipFixture.Path(), false, 9.0);

    const double lowTipWidth =
        lowTip.bbox_mm.max.x - lowTip.bbox_mm.min.x;
    const double lowTipDepth =
        lowTip.bbox_mm.max.y - lowTip.bbox_mm.min.y;
    return ExpectTrue(
               lowTip.auto_orient.selected_orientation
                   == "identity_rotate_z_minus_90",
               "low-X nail tip rotates toward positive scene Y")
        && ExpectTrue(
            highTip.auto_orient.selected_orientation
                == "identity_rotate_z_90",
            "high-X nail tip rotates toward positive scene Y")
        && ExpectTrue(
            lowTipDepth > lowTipWidth,
            "horizontal nail long axis is aligned with scene Y")
        && ExpectTrue(
            disabled.auto_orient.selected_orientation
                == "identity",
            "explicit disable preserves horizontal source heading");
}

bool VerticalNailsUseAConsistentPositiveYHeading()
{
    const TemporaryVerticalNailFixture lowTipFixture(true);
    const TemporaryVerticalNailFixture highTipFixture(false);
    const slicer_core::ModelReport lowTip =
        LoadFixture(lowTipFixture.Path(), true, 9.0);
    const slicer_core::ModelReport highTip =
        LoadFixture(highTipFixture.Path(), true, 9.0);

    return ExpectTrue(
               lowTip.auto_orient.selected_orientation
                   == "identity_rotate_z_180",
               "low-Y nail tip rotates toward positive scene Y")
        && ExpectTrue(
            highTip.auto_orient.selected_orientation
                == "identity",
            "high-Y nail tip keeps its positive scene Y heading");
}

bool ArchedNailsKeepTheirOuterSurfaceFacingPositiveZ()
{
    const TemporaryArchedNailFixture faceUpFixture(false);
    const TemporaryArchedNailFixture faceDownFixture(true);
    const slicer_core::ModelReport faceUp =
        LoadFixture(faceUpFixture.Path(), true, 9.0);
    const slicer_core::ModelReport faceDown =
        LoadFixture(faceDownFixture.Path(), true, 9.0);
    const slicer_core::ModelReport disabled =
        LoadFixture(faceDownFixture.Path(), false, 9.0);

    return ExpectTrue(
               faceUp.auto_orient.selected_orientation
                   == "identity_rotate_z_minus_90",
               "front-up arched nail keeps its Z-facing side")
        && ExpectTrue(
            faceDown.auto_orient.selected_orientation
                == "identity_rotate_x_180_rotate_z_minus_90",
            "front-down arched nail flips around its long axis")
        && ExpectTrue(
            std::abs(faceDown.bbox_mm.min.z) <= kTolerance,
            "front-up correction grounds the flipped nail")
        && ExpectTrue(
            disabled.auto_orient.selected_orientation
                == "identity",
            "explicit disable preserves front-down source pose");
}

bool TallArchedNailsResolveFrontAndHeadingAfterLayingFlat()
{
    const TemporaryTallArchedNailFixture faceUpFixture(false);
    const TemporaryTallArchedNailFixture faceDownFixture(true);
    const slicer_core::ModelReport faceUp =
        LoadFixture(faceUpFixture.Path(), true, 9.0);
    const slicer_core::ModelReport faceDown =
        LoadFixture(faceDownFixture.Path(), true, 9.0);

    return ExpectTrue(
               faceUp.auto_orient.selected_orientation
                   == "rotate_x_90_rotate_z_180",
               "tall front-up nail is laid flat with tip at positive Y")
        && ExpectTrue(
            faceDown.auto_orient.selected_orientation
                == "rotate_x_90_rotate_y_180_rotate_z_180",
            "tall front-down nail is corrected after laying flat")
        && ExpectTrue(
            std::abs(faceDown.bbox_mm.min.z) <= kTolerance,
            "corrected tall nail remains grounded");
}

}  // namespace

int main()
{
    bool passed{true};
    passed = DefaultHeightIsNineMillimeters() && passed;
    passed = EquivalentCandidatesPreferFrontUp() && passed;
    passed = ExplicitDisablePreservesSourcePose() && passed;
    passed = WithinHeightLimitPreservesSourcePose() && passed;
    passed = WithinHeightLimitGroundsEnabledSource() && passed;
    passed = HorizontalNailsUseAConsistentPositiveYHeading()
        && passed;
    passed = VerticalNailsUseAConsistentPositiveYHeading()
        && passed;
    passed = ArchedNailsKeepTheirOuterSurfaceFacingPositiveZ()
        && passed;
    passed = TallArchedNailsResolveFrontAndHeadingAfterLayingFlat()
        && passed;
    if (!passed)
    {
        return 1;
    }
    std::cout << "auto_orient_unit_tests: PASS\n";
    return 0;
}
