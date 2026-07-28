#include "slicer_core/config.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

std::filesystem::path WriteConfig(
    const std::string& name,
    const std::string& previewObject)
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / "slicesoft_13c_04_preview_output_policy";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / name;
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error(
            "failed to write preview policy fixture: " + path.string());
    }

    output
        << "{\n"
        << "  \"input\": {\n"
        << "    \"modelPath\": \"samples/models/sample.stl\",\n"
        << "    \"format\": \"auto\"\n"
        << "  }";
    if (!previewObject.empty())
    {
        output << ",\n  \"preview\": " << previewObject;
    }
    output << "\n}\n";
    return path;
}

bool ExpectPolicy(
    const std::string& name,
    const std::string& previewObject,
    const std::string& expectedPolicy,
    const bool expectedEnabled)
{
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(WriteConfig(name, previewObject));
    return ExpectTrue(
               config.preview.output_policy == expectedPolicy,
               name + " resolves expected outputPolicy")
        && ExpectTrue(
            config.preview.enabled == expectedEnabled,
            name + " derives compatible enabled");
}

bool TestDefaultsAndLegacyMigration()
{
    return ExpectPolicy(
               "missing_preview.json",
               "",
               "tiff_native",
               false)
        && ExpectPolicy(
               "empty_preview.json",
               "{}",
               "tiff_native",
               false)
        && ExpectPolicy(
               "legacy_enabled.json",
               "{\"enabled\": true}",
               "tiff_native_with_diagnostics",
               true)
        && ExpectPolicy(
               "legacy_disabled.json",
               "{\"enabled\": false}",
               "tiff_native",
               false);
}

bool TestOutputPolicyIsAuthoritative()
{
    return ExpectPolicy(
               "native_overrides_enabled.json",
               "{\"outputPolicy\": \"tiff_native\", \"enabled\": true}",
               "tiff_native",
               false)
        && ExpectPolicy(
               "diagnostics_overrides_disabled.json",
               "{\"outputPolicy\": \"tiff_native_with_diagnostics\", \"enabled\": false}",
               "tiff_native_with_diagnostics",
               true);
}

bool TestInvalidPolicyRejected()
{
    try
    {
        (void)slicer_core::load_slice_config(
            WriteConfig(
                "invalid.json",
                "{\"outputPolicy\": \"duplicate_preview\"}"));
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(
            std::string{error.what()}.find("preview.outputPolicy")
                != std::string::npos,
            "invalid outputPolicy reports the field");
    }
    return ExpectTrue(false, "invalid outputPolicy must fail");
}

}  // namespace

int main()
{
    const bool passed =
        TestDefaultsAndLegacyMigration()
        && TestOutputPolicyIsAuthoritative()
        && TestInvalidPolicyRejected();
    if (!passed)
    {
        return 1;
    }

    std::cout
        << "PASS preview_output_policy_unit_tests "
        << "default=tiff_native migration=enabled_to_policy\n";
    return 0;
}
