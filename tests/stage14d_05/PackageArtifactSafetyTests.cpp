#include "slicer_core/api/artifacts/PackageArtifactSafety.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

using slicer_core::api::artifacts::MakePackageArtifactIdentity;
using slicer_core::api::artifacts::PackageArtifactIdentity;
using slicer_core::api::artifacts::RecoverPackageArtifacts;

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path MakeRoot(const std::string& name)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d05_" + name);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    return std::filesystem::absolute(root).lexically_normal();
}

void MakeValidPackage(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path);
    std::ofstream{path / "manifest.json"} << "{}\n";
}

bool IsValidPackage(const std::filesystem::path& path)
{
    return std::filesystem::is_regular_file(path / "manifest.json");
}

bool IdentityIsDeterministicAndRejectsTemporaryTargets()
{
    const std::filesystem::path root = MakeRoot("identity");
    const PackageArtifactIdentity identity = MakePackageArtifactIdentity(
        root / "package",
        "job-01",
        "attempt-01");
    bool rejected{false};
    try
    {
        (void)MakePackageArtifactIdentity(
            root / "package.staging.bad",
            "job-01",
            "attempt-01");
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return Expect(
               identity.staging_directory.filename()
                   == "package.staging.job-01.attempt-01",
               "staging name must bind package, job, and attempt")
        && Expect(
               identity.backup_directory.filename()
                   == "package.backup.job-01.attempt-01",
               "backup name must bind package, job, and attempt")
        && Expect(rejected, "temporary package target must be rejected");
}

bool RecoversUniqueOwnedBackupAndRemovesStaging()
{
    const std::filesystem::path root = MakeRoot("restore");
    const PackageArtifactIdentity identity = MakePackageArtifactIdentity(
        root / "package",
        "job-02",
        "attempt-02");
    MakeValidPackage(identity.backup_directory);
    std::filesystem::create_directories(identity.staging_directory);
    std::filesystem::create_directories(identity.lease_directory);
    const auto recovered = RecoverPackageArtifacts(identity, IsValidPackage);
    const bool pass = Expect(recovered.success, "owned recovery must succeed")
        && Expect(recovered.target_restored, "valid owned backup must be restored")
        && Expect(IsValidPackage(identity.package_directory),
                  "restored package must become the target")
        && Expect(!std::filesystem::exists(identity.staging_directory),
                  "owned staging must be removed")
        && Expect(!std::filesystem::exists(identity.lease_directory),
                  "owned lease must be removed");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return pass;
}

bool PreservesInvalidBackupAndUnownedNeighbor()
{
    const std::filesystem::path root = MakeRoot("preserve");
    const PackageArtifactIdentity identity = MakePackageArtifactIdentity(
        root / "package",
        "job-03",
        "attempt-03");
    std::filesystem::create_directories(identity.backup_directory);
    const std::filesystem::path neighbor =
        root / "package.backup.other-job.other-attempt";
    MakeValidPackage(neighbor);
    const auto recovered = RecoverPackageArtifacts(identity, IsValidPackage);
    const bool pass = Expect(!recovered.success,
                             "invalid owned backup must fail closed")
        && Expect(std::filesystem::exists(identity.backup_directory),
                  "invalid owned backup must be preserved")
        && Expect(std::filesystem::exists(neighbor),
                  "unowned neighboring backup must never be removed");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return pass;
}

}  // namespace

int main()
{
    const bool pass = IdentityIsDeterministicAndRejectsTemporaryTargets()
        && RecoversUniqueOwnedBackupAndRemovesStaging()
        && PreservesInvalidBackupAndUnownedNeighbor();
    if (!pass)
    {
        return 1;
    }
    std::cout << "stage14d05_artifact_safety_tests: PASS\n";
    return 0;
}
