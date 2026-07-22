#pragma once

#include "slicer_core/preflight/ModelPreflightService.h"

#include <QList>
#include <QString>

struct ModelPreflightIssuePresentation
{
    QString severity;
    QString summary;
    QString count;
    QString recommendation;
    QString code;
};

struct ModelPreflightPresentation
{
    QString state;
    QString mode;
    QString admission;
    QString detail;
    QList<ModelPreflightIssuePresentation> issues;
    bool running{false};
    bool canrecheck{true};
    bool cancancel{false};
};

class ModelPreflightPresenter final
{
public:
    /**
     * @brief Build a Chinese, read-only presentation from one preflight result.
     * @param execution Fresh UI-thread execution snapshot.
     * @param mode Explicit slice mode being presented.
     * @return Localized state, admission and issue rows.
     */
    static ModelPreflightPresentation Present(
        const slicer_core::ModelPreflightExecutionResult& execution,
        slicer_core::ModelPreflightPipelineMode mode);

    /**
     * @brief Translate a stable issue code without altering its severity.
     * @param code Stable model preflight or mesh diagnostic code.
     * @return Chinese issue summary; unknown codes fail closed visibly.
     */
    static QString IssueSummary(const std::string& code);

    /**
     * @brief Translate a stable issue code into an operator recommendation.
     * @param code Stable model preflight or mesh diagnostic code.
     * @return Chinese recommendation preserving the original code in the UI.
     */
    static QString IssueRecommendation(const std::string& code);
};
