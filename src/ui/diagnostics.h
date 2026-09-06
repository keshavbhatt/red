#pragma once

#include <QString>

namespace red::core {
class Settings;
}

namespace red::ui {

/// Markdown block for bug reports (FEATURES S7): versions, host, paths,
/// engine, recent log lines. Never includes cookies or the session.
[[nodiscard]] QString buildDiagnostics(const core::Settings& settings, const QString& userAgent,
                                       const QString& engineSummary, int logLines = 60);

} // namespace red::ui
