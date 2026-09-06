#pragma once

#include <QDialog>

class QPlainTextEdit;

namespace red::core {
class Settings;
class ThemeService;
} // namespace red::core

namespace red::ui {

/// About box: brand hero, links, and a copyable diagnostics panel (FEATURES S7).
class AboutDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AboutDialog)

public:
    AboutDialog(const core::Settings& settings, core::ThemeService& theme, const QString& userAgent,
                const QString& engineSummary, QWidget* parent = nullptr);
    ~AboutDialog() override = default;

private:
    void setupUi();
    void copyDiagnostics();

    const core::Settings& m_settings;
    core::ThemeService& m_theme;
    QString m_userAgent;
    QString m_engineSummary;
    QPlainTextEdit* m_debugText = nullptr;
};

} // namespace red::ui
