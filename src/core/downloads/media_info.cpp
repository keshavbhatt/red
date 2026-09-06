#include "core/downloads/media_info.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace red::core {

namespace {

qint64 toInt64(const QJsonValue& value, qint64 def = -1)
{
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble());
    }
    return def;
}

QString codecFamily(const QString& codec)
{
    const QString c = codec.toLower();
    if (c.startsWith(u"avc"_s) || c.startsWith(u"h264"_s)) {
        return u"H.264"_s;
    }
    if (c.startsWith(u"av01"_s)) {
        return u"AV1"_s;
    }
    if (c.startsWith(u"vp09"_s) || c.startsWith(u"vp9"_s)) {
        return u"VP9"_s;
    }
    if (c.startsWith(u"vp8"_s)) {
        return u"VP8"_s;
    }
    if (c.startsWith(u"hev"_s) || c.startsWith(u"hvc"_s) || c.startsWith(u"h265"_s)) {
        return u"HEVC"_s;
    }
    if (c.startsWith(u"mp4a"_s)) {
        return u"AAC"_s;
    }
    if (c.startsWith(u"opus"_s)) {
        return u"Opus"_s;
    }
    if (c.startsWith(u"ac-3"_s) || c.startsWith(u"ac3"_s)) {
        return u"AC3"_s;
    }
    if (c.startsWith(u"ec-3"_s)) {
        return u"EAC3"_s;
    }
    if (c.startsWith(u"vorbis"_s)) {
        return u"Vorbis"_s;
    }
    return codec.section(u'.', 0, 0).toUpper();
}

} // namespace

QString MediaFormat::codecLabel() const
{
    if (hasVideo() && hasAudio()) {
        return codecFamily(vcodec) + u" + "_s + codecFamily(acodec);
    }
    if (hasVideo()) {
        return codecFamily(vcodec);
    }
    if (hasAudio()) {
        return codecFamily(acodec);
    }
    return {};
}

MediaFormat MediaFormat::fromJson(const QJsonObject& o)
{
    MediaFormat f;
    f.id = o.value(u"format_id"_s).toString();
    f.ext = o.value(u"ext"_s).toString();
    f.vcodec = o.value(u"vcodec"_s).toString();
    f.acodec = o.value(u"acodec"_s).toString();
    f.width = o.value(u"width"_s).toInt();
    f.height = o.value(u"height"_s).toInt();
    f.fps = o.value(u"fps"_s).toDouble();
    f.tbr = o.value(u"tbr"_s).toDouble();
    f.abr = o.value(u"abr"_s).toDouble();
    f.filesize = toInt64(o.value(u"filesize"_s));
    if (f.filesize < 0) {
        f.filesize = toInt64(o.value(u"filesize_approx"_s));
    }
    f.note = o.value(u"format_note"_s).toString();
    f.dynamicRange = o.value(u"dynamic_range"_s).toString();
    f.language = o.value(u"language"_s).toString();
    f.protocol = o.value(u"protocol"_s).toString();
    return f;
}

QList<int> MediaInfo::availableHeights() const
{
    QList<int> heights;
    for (const MediaFormat& f : formats) {
        if (f.hasVideo() && f.height > 0 && !heights.contains(f.height)) {
            heights << f.height;
        }
    }
    std::sort(heights.begin(), heights.end(), std::greater<>());
    return heights;
}

QStringList MediaInfo::subtitleLanguages(bool includeAutomatic) const
{
    QStringList langs;
    for (const SubtitleTrack& t : subtitles) {
        if ((!t.automatic || includeAutomatic) && !langs.contains(t.language)) {
            langs << t.language;
        }
    }
    return langs;
}

MediaInfo MediaInfo::fromJson(const QJsonObject& o)
{
    MediaInfo info;
    const QString type = o.value(u"_type"_s).toString(u"video"_s);
    info.type = (type == u"playlist"_s || type == u"multi_video"_s) ? Type::Playlist : Type::Video;
    info.id = o.value(u"id"_s).toString();
    info.url = o.value(u"webpage_url"_s).toString(o.value(u"original_url"_s).toString());
    info.title = o.value(u"title"_s).toString();
    info.uploader = o.value(u"channel"_s).toString(o.value(u"uploader"_s).toString());
    info.channelUrl = o.value(u"channel_url"_s).toString(o.value(u"uploader_url"_s).toString());
    info.description = o.value(u"description"_s).toString();
    info.thumbnail = o.value(u"thumbnail"_s).toString();
    if (info.thumbnail.isEmpty()) {
        const QJsonArray thumbs = o.value(u"thumbnails"_s).toArray();
        if (!thumbs.isEmpty()) {
            info.thumbnail = thumbs.last().toObject().value(u"url"_s).toString();
        }
    }
    info.duration = o.value(u"duration"_s).toDouble();
    info.durationText = o.value(u"duration_string"_s).toString();
    if (info.durationText.isEmpty() && info.duration > 0) {
        info.durationText = formatDuration(info.duration);
    }
    info.viewCount = toInt64(o.value(u"view_count"_s));
    info.uploadDate = o.value(u"upload_date"_s).toString();
    info.isLive = o.value(u"is_live"_s).toBool();
    info.hasChapters = !o.value(u"chapters"_s).toArray().isEmpty();

    const QJsonArray formats = o.value(u"formats"_s).toArray();
    for (const QJsonValue& v : formats) {
        const MediaFormat f = MediaFormat::fromJson(v.toObject());
        if (!f.id.isEmpty() && !f.isStoryboard()) {
            info.formats << f;
        }
    }
    const auto addSubs = [&info](const QJsonObject& tracks, bool automatic) {
        for (auto it = tracks.begin(); it != tracks.end(); ++it) {
            SubtitleTrack t;
            t.language = it.key();
            t.automatic = automatic;
            const QJsonArray variants = it.value().toArray();
            if (!variants.isEmpty()) {
                t.name = variants.first().toObject().value(u"name"_s).toString();
            }
            info.subtitles << t;
        }
    };
    addSubs(o.value(u"subtitles"_s).toObject(), false);
    addSubs(o.value(u"automatic_captions"_s).toObject(), true);

    const QJsonArray entries = o.value(u"entries"_s).toArray();
    for (const QJsonValue& v : entries) {
        const QJsonObject e = v.toObject();
        if (e.isEmpty()) {
            continue;
        }
        MediaEntry entry;
        entry.id = e.value(u"id"_s).toString();
        entry.url = e.value(u"url"_s).toString(e.value(u"webpage_url"_s).toString());
        if (entry.url.isEmpty() && !entry.id.isEmpty()) {
            entry.url = u"https://www.youtube.com/watch?v="_s + entry.id;
        }
        entry.title = e.value(u"title"_s).toString();
        entry.uploader = e.value(u"channel"_s).toString(e.value(u"uploader"_s).toString());
        const QJsonArray thumbs = e.value(u"thumbnails"_s).toArray();
        entry.thumbnail = e.value(u"thumbnail"_s).toString();
        if (entry.thumbnail.isEmpty() && !thumbs.isEmpty()) {
            entry.thumbnail = thumbs.first().toObject().value(u"url"_s).toString();
        }
        entry.duration = e.value(u"duration"_s).toDouble();
        entry.isLive = e.value(u"live_status"_s).toString() == u"is_live"_s;
        entry.viewCount = toInt64(e.value(u"view_count"_s));
        info.entries << entry;
    }
    info.entryCount = o.value(u"playlist_count"_s).toInt(static_cast<int>(info.entries.size()));
    if (info.type == Type::Playlist && info.entryCount == 0) {
        info.entryCount = static_cast<int>(info.entries.size());
    }
    return info;
}

MediaInfo parseMediaInfo(const QByteArray& json, QString* error)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        if (error != nullptr) {
            *error = parseError.error == QJsonParseError::NoError ? u"not a JSON object"_s
                                                                  : parseError.errorString();
        }
        return {};
    }
    return MediaInfo::fromJson(doc.object());
}

QString formatDuration(double seconds)
{
    if (seconds <= 0 || std::isnan(seconds)) {
        return {};
    }
    const auto total = static_cast<qint64>(std::llround(seconds));
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    if (h > 0) {
        return u"%1:%2:%3"_s.arg(h).arg(m, 2, 10, u'0').arg(s, 2, 10, u'0');
    }
    return u"%1:%2"_s.arg(m).arg(s, 2, 10, u'0');
}

QString formatBytes(qint64 bytes)
{
    if (bytes < 0) {
        return {};
    }
    constexpr double kKilo = 1000.0;
    const char* const units[] = {"B", "kB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= kKilo && unit < 4) {
        value /= kKilo;
        ++unit;
    }
    const int precision = unit == 0 ? 0 : (value < 10 ? 2 : (value < 100 ? 1 : 0));
    return QLocale::c().toString(value, 'f', precision) + u' ' + QLatin1StringView(units[unit]);
}

QString formatCount(qint64 count)
{
    if (count < 0) {
        return {};
    }
    if (count < 1000) {
        return QString::number(count);
    }
    if (count < 1'000'000) {
        return QLocale::c().toString(static_cast<double>(count) / 1000.0, 'f', count < 10'000 ? 1 : 0) + u'K';
    }
    if (count < 1'000'000'000) {
        return QLocale::c().toString(static_cast<double>(count) / 1e6, 'f', count < 10'000'000 ? 1 : 0) + u'M';
    }
    return QLocale::c().toString(static_cast<double>(count) / 1e9, 'f', 1) + u'B';
}

} // namespace red::core
