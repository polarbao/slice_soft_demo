#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

/**
 * @brief Read-only context shown beside the editable diagnostic request.
 */
struct DiagnosticSettingsPresentation
{
    QString subjectsummary{QStringLiteral("未选择模型。")};
    std::optional<double> minimumwidthmm;
    std::optional<double> maximumwidthmm;
    std::optional<double> alltexturethresholdmm;
    QString backendavailability{QStringLiteral("尚未检查诊断后端。")};
    QString status{QStringLiteral("等待导入模型。")};
    QStringList blockingreasons;
    bool controlsenabled{false};
    bool analysisrunning{false};
};

/**
 * @brief Edits diagnostic-only texture width and model-fill material in Chinese.
 */
class DiagnosticSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the diagnostic-only settings panel.
     * @param parent QWidget owner.
     */
    explicit DiagnosticSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief Replace the requested values without emitting edit signals.
     * @param widthMm Requested texture-surface width in millimetres.
     * @param modelFillMaterial Stable material id.
     */
    void SetRequestedSettings(
        double widthMm,
        const QString& modelFillMaterial);

    /**
     * @brief Update subject, derived bounds, backend, and blocking state.
     * @param presentation Read-only diagnostic context.
     */
    void SetPresentation(
        const DiagnosticSettingsPresentation& presentation);

    /**
     * @brief Return the requested texture-surface width.
     * @return Width in millimetres.
     */
    double RequestedTextureSurfaceWidthMm() const;

    /**
     * @brief Return the requested model-fill material.
     * @return Stable material id.
     */
    QString RequestedModelFillMaterial() const;

signals:
    /**
     * @brief Emitted after the operator changes diagnostic texture width.
     * @param widthMm Requested width in millimetres.
     */
    void SigTextureSurfaceWidthChanged(double widthMm);

    /**
     * @brief Emitted after the operator changes diagnostic model-fill material.
     * @param material Stable material id.
     */
    void SigModelFillMaterialChanged(const QString& material);

    /**
     * @brief Request starting one immutable background diagnostic run.
     */
    void SigStartAnalysisRequested();

    /**
     * @brief Request logical cancellation of the active diagnostic run.
     */
    void SigCancelAnalysisRequested();

private:
    QString FormatWidth(
        const std::optional<double>& width,
        const QString& missingText) const;

    QDoubleSpinBox* m_widthSpin{nullptr};
    QSlider* m_widthSlider{nullptr};
    QComboBox* m_modelFillMaterial{nullptr};
    QLabel* m_subjectLabel{nullptr};
    QLabel* m_widthBoundsLabel{nullptr};
    QLabel* m_backendLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_blockingReasonsLabel{nullptr};
    QPushButton* m_startButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
};
