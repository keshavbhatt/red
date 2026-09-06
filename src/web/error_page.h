#pragma once

#include <QString>

namespace red::web {

/// Self-contained HTML for a Red-branded load-failure page (replaces
/// Chromium's stock error page). Themed to match the app; the "Try again"
/// button reloads YouTube.
[[nodiscard]] QString errorPageHtml(bool dark, const QString& title, const QString& detail);

} // namespace red::web
