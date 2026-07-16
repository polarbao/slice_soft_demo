#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief One worst-layer item extracted from a material-closure report.
 */
struct MaterialClosureWorstLayerUi
{
    int layerindex{-1};
    double zmm{0.0};
    int gappixels{0};
    QStringList types;
    QString gappreviewpath;
};

/**
 * @brief Read-only UI projection of p0.material_closure.1.
 */
struct MaterialClosureDiagnosticsSummary
{
    bool reportavailable{false};
    bool schemavalid{false};
    bool candidateonly{false};
    QString reportpath;
    QString error;
    QString closurestatus;
    QString confidence;
    QString productionacceptance;
    bool repairenabled{false};
    bool repairattempted{false};
    int repairedpixels{0};
    int remaininggappixels{0};
    int colorfillgappixels{0};
    int modelsupportgappixels{0};
    int colorsupportgappixels{0};
    int internalvoidgappixels{0};
    int varnishsupportgappixels{0};
    int externalbackgroundprotectedpixels{0};
    QStringList diagnosticcodes;
    QVector<MaterialClosureWorstLayerUi> worstlayers;
};

/**
 * @brief Converts a material-closure JSON report into stable Chinese UI diagnostics.
 */
class MaterialClosureReportInterpreter final
{
public:
    /**
     * @brief Read and validate one material-closure report.
     * @param reportPath JSON report path.
     * @param packageDir Package root used to resolve relative gap preview paths.
     * @return Read-only UI summary. Missing and invalid reports remain explicit.
     */
    static MaterialClosureDiagnosticsSummary Interpret(
        const QString& reportPath,
        const QString& packageDir);

    /**
     * @brief Build the Chinese summary displayed in the diagnostics dock.
     * @param summary Parsed report summary.
     * @return Human-readable diagnostic text.
     */
    static QString BuildSummaryText(const MaterialClosureDiagnosticsSummary& summary);

    /**
     * @brief Map a stable gap type to a Chinese display label.
     * @param type Stable report type or diagnostic code.
     * @return Chinese label while retaining the source enum in parentheses.
     */
    static QString DisplayGapType(const QString& type);
};
