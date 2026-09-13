#pragma once

#include <QString>

namespace red::core {
class Settings;
}

namespace red::ui {

/// Markdown block for bug reports (FEATURES S7): versions, host, paths,
/// engine, recent log lines. Never includes cookies or the session.
[[nodiscard]] QString buildDiagnostics(const core::Settings& settings, const QString& userAgent,
                                       const QString& engineSummary, int logLines = 60,
                                       bool includeCrash = false);

/// A GitHub "new issue" URL for the public repo, pre-filled with the user's
/// title, description and a short environment block. Logs never go in the URL
/// (GitHub rejects long ones); the caller copies the diagnostics to the
/// clipboard for the reporter to paste, so the body must not repeat them.
[[nodiscard]] QString bugReportUrl(const QString& userAgent, const QString& title,
                                   const QString& description);

/// The same report as a mailto: link to the support address, for people
/// without a GitHub account.
[[nodiscard]] QString bugReportMailUrl(const QString& userAgent, const QString& title,
                                       const QString& description);

} // namespace red::ui
