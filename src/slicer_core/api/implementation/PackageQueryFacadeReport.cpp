#include "slicer_core/api/implementation/PackageQueryFacadeInternal.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

// 文件职责：按 manifest 声明读取具名结构化报告；
// 边界：报告路径必须落在已验证生产包内，未知名称和越界路径均失败即拒绝。
namespace slicer_core::api::implementation::detail
{

ApiResult<PackageReport> PackageQueryFacadeService::ReadReport(
    const std::filesystem::path& packageDir,
    const std::string_view name) const noexcept
{
    try
    {
        if (name.empty())
        {
            return ApiResult<PackageReport>::Failure(MakeError(
                kInputError,
                "report name is empty"));
        }
        const std::filesystem::path absolutePackage =
            RequirePackageDirectory(packageDir);
        {
            std::scoped_lock lock{m_layerMutex};
            (void)EnsureVerifiedPackageLocked(absolutePackage, false);
        }
        const Json manifest = ParseObjectFile(
            absolutePackage / "manifest.json");
        if (!manifest.contains("reports")
            || !manifest.at("reports").is_object())
        {
            return ApiResult<PackageReport>::Failure(MakeError(
                kContractError,
                "manifest reports map is missing"));
        }
        const std::string reportName{name};
        const Json& reports = manifest.at("reports");
        if (!reports.contains(reportName)
            || !reports.at(reportName).is_string())
        {
            return ApiResult<PackageReport>::Failure(MakeError(
                kInputError,
                "report is not listed by the package",
                reportName));
        }

        const std::filesystem::path relativePath{
            reports.at(reportName).as_string()};
        if (relativePath.empty() || relativePath.is_absolute())
        {
            return ApiResult<PackageReport>::Failure(MakeError(
                kContractError,
                "manifest report path must be package-relative",
                relativePath.generic_string()));
        }
        const std::filesystem::path reportPath =
            std::filesystem::weakly_canonical(
                absolutePackage / relativePath);
        if (!IsPathWithin(absolutePackage, reportPath)
            || !std::filesystem::is_regular_file(reportPath))
        {
            return ApiResult<PackageReport>::Failure(MakeError(
                kContractError,
                "manifest report path is missing or escapes the package",
                reportPath.generic_string()));
        }

        const Json report = ParseObjectFile(reportPath);
        if (!report.contains("schema")
            || !report.at("schema").is_string()
            || report.at("schema").as_string().empty())
        {
            return ApiResult<PackageReport>::Failure(MakeError(
                kContractError,
                "package report schema is missing",
                reportPath.generic_string()));
        }

        PackageReport result;
        result.report_name = reportName;
        result.report_schema = report.at("schema").as_string();
        result.data = StructuredJsonObject{report.dump(0)};
        result.source_path = reportPath;
        return ApiResult<PackageReport>::Success(std::move(result));
    }
    catch (const ValidationError& error)
    {
        return ApiResult<PackageReport>::Failure(
            MapValidationError(error));
    }
    catch (const TiffLayerError& error)
    {
        return ApiResult<PackageReport>::Failure(
            MapTiffLayerError(error));
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        return ApiResult<PackageReport>::Failure(MakeError(
            kInputError,
            "package report path could not be accessed",
            error.what()));
    }
    catch (const std::runtime_error& error)
    {
        return ApiResult<PackageReport>::Failure(MakeError(
            kContractError,
            "package report is invalid",
            error.what()));
    }
    catch (const std::exception& error)
    {
        return UnexpectedFailure<PackageReport>(
            "package report query",
            error);
    }
    catch (...)
    {
        return ApiResult<PackageReport>::Failure(MakeError(
            kInternalError,
            "unknown package report failure"));
    }
}

}  // namespace slicer_core::api::implementation::detail
