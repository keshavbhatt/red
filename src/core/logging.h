#pragma once

#include <QLoggingCategory>

// Logging categories for the core layer. Every layer declares its own in its
// `logging.h`; category names are dotted under "red." so they can be enabled
// with QT_LOGGING_RULES="red.*.debug=true".
namespace red::core {

Q_DECLARE_LOGGING_CATEGORY(lcCore)
Q_DECLARE_LOGGING_CATEGORY(lcSettings)
Q_DECLARE_LOGGING_CATEGORY(lcDownloads)
Q_DECLARE_LOGGING_CATEGORY(lcBlocking)

} // namespace red::core
