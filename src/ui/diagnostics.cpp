#include "ui/diagnostics.h"

#include "core/log_sink.h"
#include "core/settings/settings.h"
#include "platform/platform_info.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

using namespace Qt::StringLiterals;

namespace red::ui {

QString buildDiagnostics(const core::Settings& settings, const QString& userAgent,
                         const QString& engineSummary, int logLines)
{
    QString text;
    QTextStream out(&text);
    out << u"### Red diagnostics\n\n"_s;
    out << u"- Red: "_s << QCoreApplication::applicationVersion() << u'\n';
    out << u"- Qt: "_s << QString::fromLatin1(qVersion()) << u" (built against "_s
        << QString::fromLatin1(QT_VERSION_STR) << u")\n"_s;
    out << u"- Chromium: "_s << qWebEngineChromiumVersion() << u" (security patches "_s
        << qWebEngineChromiumSecurityPatchVersion() << u")\n"_s;
    out << u"- Host: "_s << platform::describeHost() << u'\n';
    out << u"- CPU: "_s << QSysInfo::currentCpuArchitecture() << u'\n';
    out << u"- User agent: "_s << userAgent << u'\n';
    out << u"- Engine: "_s << engineSummary << u'\n';
    out << u"- Settings: "_s << settings.fileName() << u'\n';
    out << u"- Data: "_s << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) << u'\n';
    out << u"- Log: "_s << core::LogSink::logFilePath() << u'\n';
    out << u"- Hardware acceleration: "_s << static_cast<int>(settings.hardwareAcceleration())
        << (settings.gpuAutoDisabled() ? u" (auto-disabled)"_s : QString()) << u'\n';
    const QStringList lines = core::LogSink::recentLines();
    const qsizetype from = std::max<qsizetype>(0, lines.size() - logLines);
    out << u"\n<details><summary>Recent log</summary>\n\n```\n"_s;
    for (qsizetype i = from; i < lines.size(); ++i) {
        out << lines.at(i) << u'\n';
    }
    out << u"```\n</details>\n"_s;
    return text;
}

} // namespace red::ui
