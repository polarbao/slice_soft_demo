#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::string> IssueCodes(
    const std::vector<slicer_core::ValidationIssue>& issues)
{
    std::vector<std::string> codes;
    codes.reserve(issues.size());
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        codes.push_back(issue.code);
    }
    return codes;
}

slicer_core::TextureFillPartitionGridSpec MakeGrid(
    const int width,
    const int height = 1,
    const int depth = 1)
{
    slicer_core::TextureFillPartitionGridSpec grid;
    grid.width = width;
    grid.height = height;
    grid.depth = depth;
    grid.spacingXMm = 0.05;
    grid.spacingYMm = 0.05;
    grid.spacingZMm = 0.01;
    return grid;
}

slicer_core::TextureFillPartitionMask3D MakeMask(
    const slicer_core::TextureFillPartitionGridSpec& grid,
    std::vector<std::uint8_t> values)
{
    slicer_core::TextureFillPartitionMask3D mask;
    mask.grid = grid;
    mask.values = std::move(values);
    return mask;
}

slicer_core::GlobalTextureFillPartitionCandidate MakeCandidate(
    const std::vector<std::uint8_t>& model,
    const std::vector<std::uint8_t>& texture,
    const std::vector<std::uint8_t>& fill)
{
    const slicer_core::TextureFillPartitionGridSpec grid =
        MakeGrid(static_cast<int>(model.size()));
    slicer_core::GlobalTextureFillPartitionCandidate candidate;
    candidate.available = true;
    candidate.backend = "generated_fixture";
    candidate.backendRole = "production_candidate";
    candidate.modelMask = MakeMask(grid, model);
    candidate.textureSurfaceMask = MakeMask(grid, texture);
    candidate.modelFillMask = MakeMask(grid, fill);
    return candidate;
}

class StaticPartitionBackend final
    : public slicer_core::IGlobalTextureFillPartitionBackend
{
public:
    explicit StaticPartitionBackend(
        slicer_core::GlobalTextureFillPartitionCandidate candidate)
        : m_candidate(std::move(candidate))
    {
    }

    slicer_core::GlobalTextureFillPartitionCandidate Evaluate(
        const slicer_core::GlobalTextureFillPartitionRequest&) const override
    {
        return m_candidate;
    }

private:
    slicer_core::GlobalTextureFillPartitionCandidate m_candidate;
};

class ThrowingPartitionBackend final
    : public slicer_core::IGlobalTextureFillPartitionBackend
{
public:
    slicer_core::GlobalTextureFillPartitionCandidate Evaluate(
        const slicer_core::GlobalTextureFillPartitionRequest&) const override
    {
        throw std::runtime_error("fixture backend failure");
    }
};

bool NullBackendIsUnavailable()
{
    slicer_core::GlobalTextureFillPartitionService service;
    slicer_core::GlobalTextureFillPartitionRequest request;
    request.options.requestedWidthMm = 0.20;
    const slicer_core::GlobalTextureFillPartitionResult result =
        service.Evaluate(request);

    return ExpectTrue(!result.available, "null backend is unavailable")
        && ExpectTrue(!result.partitionPass, "null backend cannot pass partition")
        && ExpectTrue(result.status == "blocked", "null backend status is blocked")
        && ExpectTrue(
               result.productionAcceptance == "not_evaluated",
               "null backend is never production accepted")
        && ExpectTrue(
               result.options.requestedWidthMm > 0.199
                   && result.options.requestedWidthMm < 0.201,
               "request options are preserved")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_PARTITION_BACKEND_UNAVAILABLE"),
               "null backend reports stable unavailable issue");
}

bool UnavailableBackendIsBlocked()
{
    slicer_core::GlobalTextureFillPartitionCandidate candidate;
    candidate.backend = "unavailable_fixture";
    candidate.backendRole = "unavailable";
    StaticPartitionBackend backend(std::move(candidate));
    slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::GlobalTextureFillPartitionResult result =
        service.Evaluate(slicer_core::GlobalTextureFillPartitionRequest{});

    return ExpectTrue(!result.available, "unavailable candidate remains unavailable")
        && ExpectTrue(result.status == "blocked", "unavailable candidate remains blocked")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_PARTITION_BACKEND_UNAVAILABLE"),
               "unavailable candidate receives stable issue");
}

bool BackendExceptionIsBlocked()
{
    ThrowingPartitionBackend backend;
    slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::GlobalTextureFillPartitionResult result =
        service.Evaluate(slicer_core::GlobalTextureFillPartitionRequest{});

    return ExpectTrue(!result.available, "throwing backend is unavailable")
        && ExpectTrue(!result.partitionPass, "throwing backend cannot pass")
        && ExpectTrue(result.status == "blocked", "throwing backend remains blocked")
        && ExpectTrue(
               result.productionAcceptance == "not_evaluated",
               "throwing backend is never production accepted")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_PARTITION_BACKEND_FAILED"),
               "throwing backend reports stable failure issue");
}

bool ExactPartitionsPassDiagnosticInvariants()
{
    const std::vector<slicer_core::GlobalTextureFillPartitionCandidate> candidates{
        MakeCandidate({1U}, {1U}, {0U}),
        MakeCandidate({1U}, {0U}, {1U}),
        MakeCandidate({1U, 1U, 1U, 0U}, {1U, 0U, 1U, 0U}, {0U, 1U, 0U, 0U}),
    };

    for (const slicer_core::GlobalTextureFillPartitionCandidate& candidate : candidates)
    {
        StaticPartitionBackend backend(candidate);
        slicer_core::GlobalTextureFillPartitionService service(&backend);
        const slicer_core::GlobalTextureFillPartitionResult result =
            service.Evaluate(slicer_core::GlobalTextureFillPartitionRequest{});
        if (!ExpectTrue(result.available, "exact candidate is available")
            || !ExpectTrue(result.partitionPass, "exact candidate passes invariants")
            || !ExpectTrue(result.status == "diagnostic", "exact candidate remains diagnostic")
            || !ExpectTrue(
                result.productionAcceptance == "not_evaluated",
                "diagnostic pass is not production acceptance")
            || !ExpectTrue(
                result.stats.textureSurfaceVoxels + result.stats.modelFillVoxels
                    == result.stats.modelVoxels,
                "texture plus fill equals model")
            || !ExpectTrue(
                result.stats.overlapTextureFillVoxels == 0U,
                "exact candidate has no overlap")
            || !ExpectTrue(
                result.stats.unassignedModelVoxels == 0U,
                "exact candidate has no unassigned model voxels"))
        {
            return false;
        }
    }
    return true;
}

bool InvalidGridAndMaskShapeFail()
{
    slicer_core::GlobalTextureFillPartitionCandidate invalidGrid =
        MakeCandidate({1U}, {1U}, {0U});
    invalidGrid.modelMask.grid.spacingXMm = 0.0;
    StaticPartitionBackend invalidGridBackend(std::move(invalidGrid));
    slicer_core::GlobalTextureFillPartitionService invalidGridService(
        &invalidGridBackend);
    const slicer_core::GlobalTextureFillPartitionResult invalidGridResult =
        invalidGridService.Evaluate(
            slicer_core::GlobalTextureFillPartitionRequest{});

    slicer_core::GlobalTextureFillPartitionCandidate sizeMismatch =
        MakeCandidate({1U, 1U}, {1U}, {0U, 1U});
    StaticPartitionBackend sizeMismatchBackend(std::move(sizeMismatch));
    slicer_core::GlobalTextureFillPartitionService sizeMismatchService(
        &sizeMismatchBackend);
    const slicer_core::GlobalTextureFillPartitionResult sizeMismatchResult =
        sizeMismatchService.Evaluate(
            slicer_core::GlobalTextureFillPartitionRequest{});

    return ExpectTrue(!invalidGridResult.partitionPass, "invalid grid does not pass")
        && ExpectTrue(invalidGridResult.status == "fail", "invalid grid status is fail")
        && ExpectTrue(
               HasIssueCode(invalidGridResult.issues, "E_12E_PARTITION_GRID_INVALID"),
               "invalid grid reports stable issue")
        && ExpectTrue(!sizeMismatchResult.partitionPass, "mask mismatch does not pass")
        && ExpectTrue(
               HasIssueCode(
                   sizeMismatchResult.issues,
                   "E_12E_PARTITION_MASK_SIZE_MISMATCH"),
               "mask mismatch reports stable issue");
}

bool RequestedGridContractIsEnforced()
{
    StaticPartitionBackend mismatchBackend(MakeCandidate({1U}, {1U}, {0U}));
    slicer_core::GlobalTextureFillPartitionService mismatchService(
        &mismatchBackend);
    slicer_core::GlobalTextureFillPartitionRequest mismatchRequest;
    mismatchRequest.grid = MakeGrid(2);
    const slicer_core::GlobalTextureFillPartitionResult mismatchResult =
        mismatchService.Evaluate(mismatchRequest);

    StaticPartitionBackend invalidBackend(MakeCandidate({1U}, {1U}, {0U}));
    slicer_core::GlobalTextureFillPartitionService invalidService(&invalidBackend);
    slicer_core::GlobalTextureFillPartitionRequest invalidRequest;
    invalidRequest.grid = MakeGrid(1);
    invalidRequest.grid.spacingXMm = 0.0;
    const slicer_core::GlobalTextureFillPartitionResult invalidResult =
        invalidService.Evaluate(invalidRequest);

    return ExpectTrue(
               !mismatchResult.partitionPass,
               "backend grid mismatch cannot pass")
        && ExpectTrue(
               mismatchResult.status == "fail",
               "backend grid mismatch status is fail")
        && ExpectTrue(
               HasIssueCode(
                   mismatchResult.issues,
                   "E_12E_PARTITION_MASK_SIZE_MISMATCH"),
               "backend grid mismatch reports stable issue")
        && ExpectTrue(
               !invalidResult.partitionPass,
               "invalid requested grid cannot pass")
        && ExpectTrue(
               invalidResult.status == "fail",
               "invalid requested grid status is fail")
        && ExpectTrue(
               HasIssueCode(
                   invalidResult.issues,
                   "E_12E_PARTITION_GRID_INVALID"),
               "invalid requested grid reports stable issue");
}

bool NonBinaryMaskFails()
{
    slicer_core::GlobalTextureFillPartitionCandidate candidate =
        MakeCandidate({1U, 1U}, {2U, 0U}, {0U, 1U});
    StaticPartitionBackend backend(std::move(candidate));
    slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::GlobalTextureFillPartitionResult result =
        service.Evaluate(slicer_core::GlobalTextureFillPartitionRequest{});

    return ExpectTrue(!result.partitionPass, "non-binary mask does not pass")
        && ExpectTrue(result.status == "fail", "non-binary mask status is fail")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_PARTITION_MASK_NON_BINARY"),
               "non-binary mask reports stable issue");
}

bool InvariantViolationsAreCounted()
{
    slicer_core::GlobalTextureFillPartitionCandidate candidate =
        MakeCandidate(
            {1U, 1U, 1U, 0U, 0U},
            {1U, 1U, 0U, 1U, 0U},
            {1U, 0U, 0U, 0U, 1U});
    StaticPartitionBackend backend(std::move(candidate));
    slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::GlobalTextureFillPartitionResult result =
        service.Evaluate(slicer_core::GlobalTextureFillPartitionRequest{});

    return ExpectTrue(!result.partitionPass, "invalid partition does not pass")
        && ExpectTrue(result.stats.overlapTextureFillVoxels == 1U, "overlap count")
        && ExpectTrue(result.stats.unassignedModelVoxels == 1U, "unassigned count")
        && ExpectTrue(result.stats.textureOutsideModelVoxels == 1U, "texture outside count")
        && ExpectTrue(result.stats.modelFillOutsideModelVoxels == 1U, "fill outside count")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_TEXTURE_FILL_OVERLAP"),
               "overlap issue is stable")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_MODEL_VOXEL_UNASSIGNED"),
               "unassigned issue is stable")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_TEXTURE_OUTSIDE_MODEL"),
               "texture outside issue is stable")
        && ExpectTrue(
               HasIssueCode(result.issues, "E_12E_MODEL_FILL_OUTSIDE_MODEL"),
               "fill outside issue is stable");
}

bool RepeatedEvaluationIsDeterministic()
{
    StaticPartitionBackend backend(MakeCandidate(
        {1U, 1U, 1U, 0U, 0U},
        {1U, 1U, 0U, 1U, 0U},
        {1U, 0U, 0U, 0U, 1U}));
    slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::GlobalTextureFillPartitionRequest request;
    const slicer_core::GlobalTextureFillPartitionResult first =
        service.Evaluate(request);
    const slicer_core::GlobalTextureFillPartitionResult second =
        service.Evaluate(request);

    return ExpectTrue(first.status == second.status, "status is deterministic")
        && ExpectTrue(
               first.partitionPass == second.partitionPass,
               "partition pass is deterministic")
        && ExpectTrue(
               first.stats.overlapTextureFillVoxels
                   == second.stats.overlapTextureFillVoxels,
               "overlap count is deterministic")
        && ExpectTrue(
               first.stats.unassignedModelVoxels
                   == second.stats.unassignedModelVoxels,
               "unassigned count is deterministic")
        && ExpectTrue(
               IssueCodes(first.issues) == IssueCodes(second.issues),
               "issue order is deterministic");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"null_backend_is_unavailable", NullBackendIsUnavailable},
        {"unavailable_backend_is_blocked", UnavailableBackendIsBlocked},
        {"backend_exception_is_blocked", BackendExceptionIsBlocked},
        {"exact_partitions_pass_diagnostic_invariants", ExactPartitionsPassDiagnosticInvariants},
        {"invalid_grid_and_mask_shape_fail", InvalidGridAndMaskShapeFail},
        {"requested_grid_contract_is_enforced", RequestedGridContractIsEnforced},
        {"non_binary_mask_fails", NonBinaryMaskFails},
        {"invariant_violations_are_counted", InvariantViolationsAreCounted},
        {"repeated_evaluation_is_deterministic", RepeatedEvaluationIsDeterministic},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        bool passed{false};
        try
        {
            passed = test.second();
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first
                      << " exception=" << error.what() << '\n';
            return 1;
        }
        if (!passed)
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Texture/fill partition service unit tests complete.\n";
    return 0;
}
