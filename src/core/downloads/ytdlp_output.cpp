#include "core/downloads/ytdlp_output.h"

#include "core/downloads/download_options.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace red::core {

namespace {

std::optional<QJsonObject> jsonAfter(const QString& line, QLatin1StringView prefix)
{
    const QJsonDocument doc = QJsonDocument::fromJson(line.mid(prefix.size()).toUtf8());
    if (!doc.isObject()) {
        return std::nullopt;
    }
    return doc.object();
}

qint64 int64Of(const QJsonObject& o, QLatin1StringView key)
{
    return static_cast<qint64>(o.value(key).toDouble());
}

} // namespace

QString PostprocessEvent::label() const
{
    if (postprocessor == u"Merger"_s) {
        return u"Merging streams"_s;
    }
    if (postprocessor == u"EmbedThumbnail"_s) {
        return u"Embedding thumbnail"_s;
    }
    if (postprocessor == u"Metadata"_s) {
        return u"Writing metadata"_s;
    }
    if (postprocessor == u"ExtractAudio"_s) {
        return u"Converting audio"_s;
    }
    if (postprocessor == u"VideoRemuxer"_s || postprocessor == u"VideoConvertor"_s) {
        return u"Remuxing"_s;
    }
    if (postprocessor == u"EmbedSubtitle"_s) {
        return u"Embedding subtitles"_s;
    }
    if (postprocessor == u"SponsorBlock"_s || postprocessor == u"ModifyChapters"_s) {
        return u"Removing sponsor segments"_s;
    }
    if (postprocessor == u"MoveFiles"_s) {
        return u"Finishing"_s;
    }
    if (postprocessor.startsWith(u"Fixup"_s)) {
        return u"Fixing container"_s;
    }
    return u"Processing"_s;
}

std::optional<OutputEvent> parseOutputLine(const QString& rawLine)
{
    const QString line = rawLine.trimmed();
    if (line.isEmpty()) {
        return std::nullopt;
    }
    if (line.startsWith(kProgressPrefix)) {
        const auto o = jsonAfter(line, kProgressPrefix);
        if (!o) {
            return std::nullopt;
        }
        ProgressEvent e;
        e.status = o->value(u"status"_s).toString();
        e.downloaded = int64Of(*o, QLatin1StringView("downloaded"));
        e.total = int64Of(*o, QLatin1StringView("total"));
        e.estimate = int64Of(*o, QLatin1StringView("estimate"));
        e.speed = o->value(u"speed"_s).toDouble();
        e.eta = o->value(u"eta"_s).toInt();
        e.filename = o->value(u"filename"_s).toString();
        e.index = o->value(u"index"_s).toInt();
        e.count = o->value(u"count"_s).toInt();
        e.id = o->value(u"id"_s).toString();
        return e;
    }
    if (line.startsWith(kPostprocessPrefix)) {
        const auto o = jsonAfter(line, kPostprocessPrefix);
        if (!o) {
            return std::nullopt;
        }
        PostprocessEvent e;
        e.status = o->value(u"status"_s).toString();
        e.postprocessor = o->value(u"postprocessor"_s).toString();
        e.id = o->value(u"id"_s).toString();
        return e;
    }
    if (line.startsWith(kItemPrefix)) {
        const auto o = jsonAfter(line, kItemPrefix);
        if (!o) {
            return std::nullopt;
        }
        ItemEvent e;
        e.id = o->value(u"id"_s).toString();
        e.title = o->value(u"title"_s).toString();
        e.index = o->value(u"index"_s).toInt();
        e.count = o->value(u"count"_s).toInt();
        e.thumbnail = o->value(u"thumbnail"_s).toString();
        e.duration = o->value(u"duration"_s).toDouble();
        e.uploader = o->value(u"uploader"_s).toString();
        e.url = o->value(u"url"_s).toString();
        return e;
    }
    if (line.startsWith(kFilePrefix)) {
        return FileEvent{line.mid(kFilePrefix.size())};
    }
    MessageEvent m;
    if (line.startsWith(u"ERROR:"_s)) {
        m.level = MessageEvent::Level::Error;
        m.text = line.mid(6).trimmed();
    } else if (line.startsWith(u"WARNING:"_s)) {
        m.level = MessageEvent::Level::Warning;
        m.text = line.mid(8).trimmed();
    } else {
        m.text = line;
    }
    return m;
}

QString friendlyError(const QString& stderrTail, int exitCode)
{
    const QString text = stderrTail.trimmed();
    QString lastError;
    const QStringList lines = text.split(u'\n', Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (it->startsWith(u"ERROR:"_s)) {
            lastError = it->mid(6).trimmed();
            break;
        }
    }
    const QString probe = lastError.isEmpty() ? text : lastError;
    if (probe.contains(u"Sign in to confirm"_s, Qt::CaseInsensitive) ||
        probe.contains(u"not a bot"_s, Qt::CaseInsensitive)) {
        return u"YouTube asked for a sign-in. Sign in to YouTube in Red and try again."_s;
    }
    if (probe.contains(u"Private video"_s, Qt::CaseInsensitive)) {
        return u"This video is private."_s;
    }
    if (probe.contains(u"age"_s, Qt::CaseInsensitive) && probe.contains(u"confirm"_s, Qt::CaseInsensitive)) {
        return u"Age-restricted video: sign in to YouTube in Red and try again."_s;
    }
    if (probe.contains(u"members-only"_s, Qt::CaseInsensitive) || probe.contains(u"Join this channel"_s)) {
        return u"Members-only content: sign in with a membership and try again."_s;
    }
    if (probe.contains(u"Video unavailable"_s, Qt::CaseInsensitive)) {
        return u"This video is unavailable."_s;
    }
    if (probe.contains(u"playlist type is unviewable"_s, Qt::CaseInsensitive)) {
        return u"This is a YouTube mix, generated per viewer; only its videos can be downloaded one by one."_s;
    }
    if (probe.contains(u"playlist does not exist"_s, Qt::CaseInsensitive) ||
        (probe.contains(u"[youtube:tab]"_s) && probe.contains(u"HTTP Error 400"_s))) {
        return u"This playlist does not exist or is private."_s;
    }
    if (probe.contains(u"HTTP Error 403"_s) || probe.contains(u"HTTP Error 429"_s)) {
        return u"YouTube refused the request (rate limited). Try again in a few minutes."_s;
    }
    if (probe.contains(u"ffmpeg"_s, Qt::CaseInsensitive) &&
        probe.contains(u"not found"_s, Qt::CaseInsensitive)) {
        return u"ffmpeg is missing. Reinstall the download engine in Settings → Downloads."_s;
    }
    if (probe.contains(u"Requested format is not available"_s)) {
        return u"The chosen quality is not available for this video."_s;
    }
    if (probe.contains(u"No space left"_s)) {
        return u"No space left on the download drive."_s;
    }
    if (probe.contains(u"Unable to download webpage"_s) || probe.contains(u"Network is unreachable"_s) ||
        probe.contains(u"Temporary failure in name resolution"_s)) {
        return u"Network error. Check your connection and try again."_s;
    }
    if (!lastError.isEmpty()) {
        // Drop yt-dlp's "[extractor] id: " prefix and the "(caused by …)" tail.
        static const QRegularExpression kPrefix(u"^\\[[^\\]]+\\]\\s*(?:[A-Za-z0-9_-]+:\\s*)?"_s);
        QString cleaned = lastError;
        cleaned.remove(kPrefix);
        if (const qsizetype cause = cleaned.indexOf(u" (caused by"_s); cause > 0) {
            cleaned.truncate(cause);
        }
        return cleaned.left(200);
    }
    if (exitCode != 0) {
        return u"The download engine exited with code %1."_s.arg(exitCode);
    }
    return u"Download failed."_s;
}

} // namespace red::core
