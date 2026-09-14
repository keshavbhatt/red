#include "app/cli_options.h"

#include <QCommandLineParser>

using namespace Qt::StringLiterals;

namespace red::app {

bool CliOptions::hasCommands() const
{
    return download.has_value() || showSettings || quit || tvMode || musicMode || !urls.isEmpty();
}

CliParseResult parseCliOptions(const QStringList& arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        u"YouTube desktop client with TV and Music modes and a download manager"_s);
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption version = parser.addVersionOption();
    const QCommandLineOption profile({u"p"_s, u"profile"_s},
                                     u"Use a separate profile (own session and settings)."_s, u"name"_s);
    const QCommandLineOption tv({u"t"_s, u"tv"_s}, u"Start in TV mode (YouTube's living-room interface)."_s);
    const QCommandLineOption music({u"m"_s, u"music"_s}, u"Start in Music mode (YouTube Music)."_s);
    const QCommandLineOption logFile(
        u"log-file"_s, u"Write the log to <path> instead of the default location."_s, u"path"_s);
    const QCommandLineOption noLogFile(u"no-log-file"_s, u"Do not write a log file."_s);
    const QCommandLineOption download({u"d"_s, u"download"_s},
                                      u"Open the download dialog for a YouTube link."_s, u"url"_s);
    const QCommandLineOption settings({u"s"_s, u"settings"_s}, u"Open the settings dialog."_s);
    const QCommandLineOption quit({u"q"_s, u"quit"_s}, u"Quit the running instance."_s);
    parser.addOptions({profile, tv, music, logFile, noLogFile, download, settings, quit});
    parser.addPositionalArgument(u"url"_s, u"YouTube video, playlist or channel link to open."_s, u"[url]"_s);

    CliParseResult result;
    if (!parser.parse(arguments)) {
        result.errorText = parser.errorText();
        return result;
    }
    if (parser.isSet(help)) {
        result.helpRequested = true;
        result.helpText = parser.helpText();
        return result;
    }
    if (parser.isSet(version)) {
        result.versionRequested = true;
        return result;
    }

    CliOptions& o = result.options;
    o.profile = parser.value(profile).trimmed();
    o.tvMode = parser.isSet(tv);
    o.musicMode = parser.isSet(music);
    if (o.tvMode && o.musicMode) {
        result.errorText = u"--tv and --music cannot be combined."_s;
        return result;
    }
    if (parser.isSet(logFile)) {
        o.logFile = parser.value(logFile);
    }
    o.noLogFile = parser.isSet(noLogFile);
    if (parser.isSet(download)) {
        o.download = parser.value(download);
    }
    o.showSettings = parser.isSet(settings);
    o.quit = parser.isSet(quit);
    o.urls = parser.positionalArguments();
    return result;
}

QList<QJsonObject> commandsFor(const CliOptions& options)
{
    QList<QJsonObject> commands;
    const auto key = QString::fromLatin1(cmd::kKey);
    const auto urlKey = QString::fromLatin1(cmd::kUrlKey);
    if (options.quit) {
        commands.append({{key, QString::fromLatin1(cmd::kQuit)}});
        return commands;
    }
    if (options.tvMode) {
        commands.append({{key, QString::fromLatin1(cmd::kTv)}});
    }
    if (options.musicMode) {
        commands.append({{key, QString::fromLatin1(cmd::kMusic)}});
    }
    for (const QString& url : options.urls) {
        commands.append({{key, QString::fromLatin1(cmd::kOpen)}, {urlKey, url}});
    }
    if (options.download) {
        commands.append({{key, QString::fromLatin1(cmd::kDownload)}, {urlKey, *options.download}});
    }
    if (options.showSettings) {
        commands.append({{key, QString::fromLatin1(cmd::kSettings)}});
    }
    commands.append({{key, QString::fromLatin1(cmd::kRaise)}});
    return commands;
}

} // namespace red::app
