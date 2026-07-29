#include "slicer_core/config.h"
#include "slicer_core/model.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
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
    TemporaryObjFixture()
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
        output
            << "v -5.973302 -3.000000 0\n"
            << "v 5.973671 -3.000000 0\n"
            << "v 5.973671 3.000000 0\n"
            << "v -5.973302 3.000000 0\n"
            << "v -5.973302 -3.000000 30.371832\n"
            << "v 5.973671 -3.000000 30.371832\n"
            << "v 5.973671 3.000000 30.371832\n"
            << "v -5.973302 3.000000 30.371832\n"
            << "v 5.968403 -3.799789 6.351915\n"
            << "v -0.240944 4.183847 8.715206\n"
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
                   == "rotate_x_90",
               "equivalent X candidates prefer rotate_x_90")
        && ExpectTrue(
            second.auto_orient.selected_orientation
                == first.auto_orient.selected_orientation,
            "orientation is deterministic across repeated loads")
        && ExpectTrue(
            productDefault.auto_orient.selected_orientation
                == "rotate_x_90",
            "product default keeps the standard nail front-up")
        && ExpectTrue(
            first.bbox_mm.min.y < -30.0
                && std::abs(first.bbox_mm.max.y)
                    <= kTolerance,
            "front-up candidate keeps standard nail heading")
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

}  // namespace

int main()
{
    bool passed{true};
    passed = DefaultHeightIsNineMillimeters() && passed;
    passed = EquivalentCandidatesPreferFrontUp() && passed;
    passed = ExplicitDisablePreservesSourcePose() && passed;
    passed = WithinHeightLimitPreservesSourcePose() && passed;
    if (!passed)
    {
        return 1;
    }
    std::cout << "auto_orient_unit_tests: PASS\n";
    return 0;
}
