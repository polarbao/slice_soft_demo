#include "slicer_core/api/implementation/PackageQueryFacadeInternal.h"

#include "slicer_core/api/artifacts/PackageArtifactSafety.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace slicer_core::api::implementation::detail
{

ApiError MakeError(
    const char* code,
    std::string message,
    std::string detail)
{
    return ApiError{code, std::move(message), std::move(detail)};
}

Json ParseObjectFile(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error(
            "failed to open JSON file: " + path.generic_string());
    }
    Json document = Json::parse(input);
    if (!document.is_object())
    {
        throw std::runtime_error(
            "JSON root must be an object: " + path.generic_string());
    }
    return document;
}

std::filesystem::path RequirePackageDirectory(
    const std::filesystem::path& packageDir)
{
    if (packageDir.empty())
    {
        throw ValidationError(
            ValidationErrorCode::PackageNotFound,
            "package directory is empty");
    }
    const std::filesystem::path absolute =
        std::filesystem::absolute(packageDir).lexically_normal();
    if (artifacts::IsTemporaryPackagePath(absolute))
    {
        throw ValidationError(
            ValidationErrorCode::PackageNotFound,
            "temporary package artifacts cannot be queried: "
                + absolute.generic_string());
    }
    if (!std::filesystem::is_directory(absolute))
    {
        throw ValidationError(
            ValidationErrorCode::PackageNotFound,
            "package directory not found: " + absolute.generic_string());
    }
    return std::filesystem::weakly_canonical(absolute);
}

bool IsPathWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    const std::filesystem::path relative =
        candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute())
    {
        return false;
    }
    const auto first = relative.begin();
    return first == relative.end() || *first != "..";
}

ApiError MapValidationError(const ValidationError& error)
{
    const bool inputFailure =
        error.code() == ValidationErrorCode::PackageNotFound
        || error.code() == ValidationErrorCode::ManifestMissing;
    return MakeError(
        inputFailure ? kInputError : kContractError,
        inputFailure ? "package input is unavailable"
                     : "package contract validation failed",
        error.what());
}

ApiError MapTiffLayerError(const TiffLayerError& error)
{
    if (error.Code() == TiffLayerErrorCode::Cancelled)
    {
        return MakeError(
            kCancelledError,
            "package query was cancelled",
            error.what());
    }
    const bool inputFailure =
        error.Code() == TiffLayerErrorCode::PackageNotFound
        || error.Code() == TiffLayerErrorCode::LayerNotListed;
    return MakeError(
        inputFailure ? kInputError : kContractError,
        inputFailure ? "package or layer input is unavailable"
                     : "production TIFF contract validation failed",
        error.what());
}

}  // namespace slicer_core::api::implementation::detail
