#pragma once

#include "../services/ConfigDocument.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QWidget>

struct ProductionProfileCapability;

class QuickConfigPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the quick configuration panel.
     * @param document Shared configuration document edited by the UI.
     * @param parent Qt parent widget.
     */
    explicit QuickConfigPanel(ConfigDocument* document, QWidget* parent = nullptr);

    /**
     * @brief Refresh all controls from the current document.
     */
    void LoadFromDocument();

    /**
     * @brief Apply product Profile capability locks to ordinary configuration controls.
     * @param profile Null for Legacy pass-through; admitted Global Profile otherwise.
     */
    void ApplyProductionCapability(const ProductionProfileCapability* profile);

private slots:
    void OnBrowseModel();
    void OnBrowseOutput();
    void OnModelPathEdited();
    void OnOutputDirEdited();
    void OnModelScaleChanged(double value);
    void OnResetModelScale();
    void OnLayerHeightChanged(double value);
    void OnTexturePolicyChanged(int index);
    void OnNonSurfaceRgbPolicyChanged(int index);
    void OnModelFillMaterialChanged(int index);
    void OnSupportEnabledChanged(bool checked);
    void OnSupportPlacementChanged(int index);
    void OnInternalVoidEnabledChanged(bool checked);
    void OnInternalVoidMinAreaChanged(int value);
    void OnWhiteEnabledChanged(bool checked);
    void OnVarnishEnabledChanged(bool checked);
    void OnVarnishTopLayersChanged(int value);
    void OnSurfaceVarnishEnabledChanged(bool checked);
    void OnSurfaceVarnishThicknessChanged(int value);
    void OnOuterVarnishEnabledChanged(bool checked);
    void OnOuterVarnishThicknessChanged(double value);
    void OnPreviewEnabledChanged(bool checked);
    void OnPreviewIntervalChanged(int value);
    void OnOpenVdbEnabledChanged(bool checked);

private:
    void SetValueIfChanged(const QStringList& path, const QJsonValue& value);
    QString StringValue(const QStringList& path, const QString& fallback = QString()) const;
    bool BoolValue(const QStringList& path, bool fallback = false) const;
    int IntValue(const QStringList& path, int fallback = 0) const;
    double DoubleValue(const QStringList& path, double fallback = 0.0) const;
    void UpdateNormalizedView();

    ConfigDocument* m_document{nullptr};
    bool m_loading{false};

    QLineEdit* m_modelPathEdit{nullptr};
    QLineEdit* m_outputDirEdit{nullptr};
    QDoubleSpinBox* m_modelScaleXSpin{nullptr};
    QDoubleSpinBox* m_modelScaleYSpin{nullptr};
    QDoubleSpinBox* m_modelScaleZSpin{nullptr};
    QDoubleSpinBox* m_layerHeightSpin{nullptr};
    QComboBox* m_texturePolicyCombo{nullptr};
    QComboBox* m_nonSurfaceRgbPolicyCombo{nullptr};
    QComboBox* m_modelFillMaterialCombo{nullptr};
    QCheckBox* m_supportEnabledCheck{nullptr};
    QComboBox* m_supportPlacementCombo{nullptr};
    QCheckBox* m_internalVoidEnabledCheck{nullptr};
    QSpinBox* m_internalVoidMinAreaSpin{nullptr};
    QCheckBox* m_whiteEnabledCheck{nullptr};
    QCheckBox* m_varnishEnabledCheck{nullptr};
    QSpinBox* m_varnishTopLayersSpin{nullptr};
    QCheckBox* m_surfaceVarnishEnabledCheck{nullptr};
    QSpinBox* m_surfaceVarnishThicknessSpin{nullptr};
    QCheckBox* m_outerVarnishEnabledCheck{nullptr};
    QDoubleSpinBox* m_outerVarnishThicknessSpin{nullptr};
    QCheckBox* m_previewEnabledCheck{nullptr};
    QSpinBox* m_previewIntervalSpin{nullptr};
    QCheckBox* m_openVdbEnabledCheck{nullptr};
    QPlainTextEdit* m_normalizedView{nullptr};
};
