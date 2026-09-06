#include "platform/crash_handler.h"

namespace red::platform {

void installCrashHandler(const QString& /*crashFilePath*/) {}

QString lastCrashReport()
{
    return {};
}

} // namespace red::platform
