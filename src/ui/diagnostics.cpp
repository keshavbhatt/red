#include "ui/diagnostics.h"

#include "core/log_sink.h"
#include "core/settings/settings.h"
#include "platform/crash_handler.h"
#include "platform/platform_info.h"
#include "ui/links.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {

QString environmentBlock(const QString& userAgent)
{
    Q_UNUSED(userAgent)
    return u"- Red: %1 (%2)\n- OS: %3\n- Chromium: %4\n"_s.arg(
        QCoreApplication::applicationVersion(), platform::packageType(), platform::describeHost(),
        qWebEngineChromiumVersion());
}

QString reportBody(const QString& userAgent, const QString& description, bool markdown)
{
    const QString described = description.trimmed().isEmpty()
                                  ? (markdown ? u"<!-- describe the problem here -->"_s : QString())
                                  : description.trimmed().left(1500);
    QString body;
    body += (markdown ? u"**What happened?**\n\n"_s : u"What happened?\n\n"_s) + described + u"\n\n"_s;
    body += (markdown ? u"**Steps to reproduce**\n1. \n2. \n3. \n\n"_s
                      : u"Steps to reproduce\n1. \n2. \n3. \n\n"_s);
    body += (markdown ? u"**Environment**\n"_s : u"Environment\n"_s) + environmentBlock(userAgent) + u'\n';
    body += markdown ? u"<!-- Paste the diagnostics from your clipboard below. -->"_s
                     : u"(Paste the diagnostics from your clipboard below.)"_s;
    return body;
}

QString reportTitle(const QString& title)
{
    return u"[Bug] "_s + title.trimmed().section(u'\n', 0, 0).left(80);
}

} // namespace

QString bugReportUrl(const QString& userAgent, const QString& title, const QString& description)
{
    QUrl url(links::kIssues + u"/new"_s);
    QUrlQuery query;
    query.addQueryItem(u"labels"_s, u"bug"_s);
    query.addQueryItem(u"title"_s, reportTitle(title));
    query.addQueryItem(u"body"_s, reportBody(userAgent, description, true));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QString bugReportMailUrl(const QString& userAgent, const QString& title, const QString& description)
{
    QUrl url(links::kContact);
    QUrlQuery query;
    query.addQueryItem(u"subject"_s, reportTitle(title));
    query.addQueryItem(u"body"_s, reportBody(userAgent, description, false));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QString buildDiagnostics(const core::Settings& settings, const QString& userAgent,
                         const QString& engineSummary, int logLines, bool includeCrash)
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
    if (includeCrash) {
        if (const QString crash = platform::lastCrashReport(); !crash.isEmpty()) {
            out << u"\n<details><summary>Last crash</summary>\n\n```\n"_s << crash
                << u"\n```\n</details>\n"_s;
        }
    }
    return text;
}

} // namespace red::ui
