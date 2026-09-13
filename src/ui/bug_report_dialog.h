#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

namespace red::core {
class Settings;
class ThemeService;
} // namespace red::core

namespace red::ui {

/// Report a bug: a title, what happened, and the diagnostics on the clipboard.
/// Opens a pre-filled GitHub issue (or an email to support) for the reporter
/// to paste the diagnostics into; the logs are too long for a URL.
class BugReportDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BugReportDialog)

public:
    BugReportDialog(const core::Settings& settings, const core::ThemeService& theme, QString userAgent,
                    QString engineSummary, QWidget* parent = nullptr);
    ~BugReportDialog() override = default;

private:
    [[nodiscard]] QString diagnostics() const;
    void copyDiagnostics();
    void openIssue();
    void openMail();
    void reportOpened(const QString& url, const QString& where);

    const core::Settings& m_settings;
    QString m_userAgent;
    QString m_engineSummary;
    QLineEdit* m_title = nullptr;
    QPlainTextEdit* m_description = nullptr;
    QCheckBox* m_includeCrash = nullptr;
    QLabel* m_status = nullptr;
};

} // namespace red::ui
