#pragma once

#include <QWidget>

class QComboBox;
class QPlainTextEdit;

/**
 * @brief Displays the centralized Chinese help metadata for slicing settings.
 */
class SettingHelpPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the setting help panel.
     * @param parent Qt parent widget.
     */
    explicit SettingHelpPanel(QWidget* parent = nullptr);

    /**
     * @brief Select a setting by its stable help key.
     * @param key Setting key registered by HelpTextProvider.
     * @return true when the key exists and was selected.
     */
    bool SelectKey(const QString& key);

    /**
     * @brief Return the full text currently displayed by the panel.
     * @return Current Chinese setting description.
     */
    QString CurrentText() const;

private slots:
    void OnSettingChanged(int index);

private:
    QComboBox* m_settingSelector{nullptr};
    QPlainTextEdit* m_detailView{nullptr};
};
