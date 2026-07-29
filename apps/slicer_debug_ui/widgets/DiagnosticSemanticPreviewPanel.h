#pragma once

#include "../services/TiffLayerLoadWorker.h"
#include "../workers/DiagnosticAnalysisWorker.h"

#include "slicer_core/preview/TextureFillPartitionSemanticPreview.h"

#include <QImage>
#include <QWidget>

#include <optional>

class QComboBox;
class QLabel;
class QResizeEvent;
class QScrollArea;

/**
 * @brief Display modes for the same-layer Texture/Fill semantic diagnostic.
 */
enum class DiagnosticSemanticDisplayMode
{
    PartitionSupportVarnish = 0,
    TextureSurface = 1,
    ModelFill = 2,
};

/**
 * @brief TIFF-native same-layer diagnostic view for Texture Surface and Model Fill.
 */
class DiagnosticSemanticPreviewPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the semantic diagnostic view.
     * @param parent Qt parent widget.
     */
    explicit DiagnosticSemanticPreviewPanel(
        QWidget* parent = nullptr);

    /**
     * @brief Accept the newest immutable diagnostic analysis result.
     * @param result Scene-aware result and in-memory partition evidence.
     */
    void SetDiagnosticAnalysis(
        const DiagnosticAnalysisResult& result);

    /**
     * @brief Clear stale or cancelled diagnostic evidence.
     * @param reason User-visible reason for the unavailable state.
     */
    void ClearDiagnosticAnalysis(
        const QString& reason = QString());

    /**
     * @brief Accept the exact currently selected production TIFF layer.
     * @param buffer Shared immutable RGBWSV layer.
     */
    void SetProductionLayer(
        TiffLayerBufferPtr buffer);

    /**
     * @brief Select a diagnostic display mode for tests.
     * @param mode Texture, fill, or partition with production S/V.
     * @return True when the mode exists.
     */
    bool SetDisplayModeForTest(
        DiagnosticSemanticDisplayMode mode);

    /**
     * @brief Return the current same-layer semantic image.
     * @return Diagnostic image in production TIFF coordinates.
     */
    QImage CurrentImageForTest() const;

    /**
     * @brief Return the current user-visible semantic status.
     * @return Status including layer, z, identity, and coverage.
     */
    QString StatusForTest() const;

    /**
     * @brief Return the source production layer index.
     * @return Real manifest layer index, or -1.
     */
    int LayerIndexForTest() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void OnDisplayModeChanged(int index);

private:
    void Compose();
    void ApplyImage();
    void SetUnavailable(const QString& status);
    DiagnosticSemanticDisplayMode CurrentDisplayMode() const;

    QComboBox* m_displayModeSelector{nullptr};
    QLabel* m_status{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    QLabel* m_imageLabel{nullptr};
    TiffLayerBufferPtr m_productionLayer;
    std::optional<DiagnosticAnalysisResult> m_analysis;
    slicer_core::TextureFillPartitionSemanticPreviewResult
        m_semantics;
    QImage m_image;
};
