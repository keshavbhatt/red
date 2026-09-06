#pragma once

#include <QLoggingCategory>

namespace red::web {

Q_DECLARE_LOGGING_CATEGORY(lcWeb)
/// Console output of the page itself (console.log/warn/error from YouTube
/// and from our injected scripts).
Q_DECLARE_LOGGING_CATEGORY(lcWebJs)

} // namespace red::web
