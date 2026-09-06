#include "core/downloads/download_options.h"

#include <QDir>
#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace red::core {

namespace {

QJsonArray toArray(const QStringList& list)
{
    QJsonArray array;
    for (const QString& s : list) {
        array.append(s);
    }
    return array;
}

QStringList fromArray(const QJsonArray& array)
{
    QStringList list;
    for (const QJsonValue& v : array) {
        list << v.toString();
    }
    return list;
}

QString sanitizeForTemplate(QString text)
{
    // yt-dlp sanitizes filenames itself; the preview only needs the same rough
    // treatment of the characters that never survive.
    static const QString kForbidden = u"/\\:*?\"<>|"_s;
    for (const QChar c : kForbidden) {
        text.replace(c, u'_');
    }
    return text.simplified();
}

} // namespace

QString qualityLabel(VideoQuality quality)
{
    switch (quality) {
    case VideoQuality::Best:
        return u"Best available"_s;
    case VideoQuality::Q2160:
        return u"2160p (4K)"_s;
    case VideoQuality::Q1440:
        return u"1440p"_s;
    case VideoQuality::Q1080:
        return u"1080p"_s;
    case VideoQuality::Q720:
        return u"720p"_s;
    case VideoQuality::Q480:
        return u"480p"_s;
    case VideoQuality::Q360:
        return u"360p"_s;
    }
    return {};
}

int qualityHeight(VideoQuality quality)
{
    switch (quality) {
    case VideoQuality::Best:
        return 0;
    case VideoQuality::Q2160:
        return 2160;
    case VideoQuality::Q1440:
        return 1440;
    case VideoQuality::Q1080:
        return 1080;
    case VideoQuality::Q720:
        return 720;
    case VideoQuality::Q480:
        return 480;
    case VideoQuality::Q360:
        return 360;
    }
    return 0;
}

QString containerExtension(Container container)
{
    switch (container) {
    case Container::Mp4:
        return u"mp4"_s;
    case Container::Mkv:
        return u"mkv"_s;
    case Container::Webm:
        return u"webm"_s;
    }
    return u"mp4"_s;
}

QString audioFormatExtension(AudioFormat format)
{
    switch (format) {
    case AudioFormat::Best:
        return {};
    case AudioFormat::Mp3:
        return u"mp3"_s;
    case AudioFormat::M4a:
        return u"m4a"_s;
    case AudioFormat::Opus:
        return u"opus"_s;
    case AudioFormat::Flac:
        return u"flac"_s;
    case AudioFormat::Wav:
        return u"wav"_s;
    }
    return {};
}

QJsonObject DownloadOptions::toJson() const
{
    return {
        {u"kind"_s, static_cast<int>(kind)},
        {u"quality"_s, static_cast<int>(quality)},
        {u"container"_s, static_cast<int>(container)},
        {u"audioFormat"_s, static_cast<int>(audioFormat)},
        {u"audioBitrateKbps"_s, audioBitrateKbps},
        {u"customVideoFormat"_s, customVideoFormat},
        {u"customAudioFormat"_s, customAudioFormat},
        {u"subtitleLanguages"_s, toArray(subtitleLanguages)},
        {u"includeAutoSubtitles"_s, includeAutoSubtitles},
        {u"embedSubtitles"_s, embedSubtitles},
        {u"embedThumbnail"_s, embedThumbnail},
        {u"embedMetadata"_s, embedMetadata},
        {u"removeSponsors"_s, removeSponsors},
        {u"sponsorCategories"_s, toArray(sponsorCategories)},
        {u"outputDirectory"_s, outputDirectory},
        {u"filenamePattern"_s, static_cast<int>(filenamePattern)},
        {u"playlistItems"_s, playlistItems},
        {u"isPlaylist"_s, isPlaylist},
        {u"speedLimitKbps"_s, speedLimitKbps},
    };
}

DownloadOptions DownloadOptions::fromJson(const QJsonObject& o)
{
    DownloadOptions d;
    d.kind = static_cast<Kind>(std::clamp(o.value(u"kind"_s).toInt(), 0, 2));
    d.quality = static_cast<VideoQuality>(std::clamp(o.value(u"quality"_s).toInt(), 0, 6));
    d.container = static_cast<Container>(std::clamp(o.value(u"container"_s).toInt(), 0, 2));
    d.audioFormat = static_cast<AudioFormat>(std::clamp(o.value(u"audioFormat"_s).toInt(), 0, 5));
    d.audioBitrateKbps = o.value(u"audioBitrateKbps"_s).toInt();
    d.customVideoFormat = o.value(u"customVideoFormat"_s).toString();
    d.customAudioFormat = o.value(u"customAudioFormat"_s).toString();
    d.subtitleLanguages = fromArray(o.value(u"subtitleLanguages"_s).toArray());
    d.includeAutoSubtitles = o.value(u"includeAutoSubtitles"_s).toBool();
    d.embedSubtitles = o.value(u"embedSubtitles"_s).toBool(true);
    d.embedThumbnail = o.value(u"embedThumbnail"_s).toBool(true);
    d.embedMetadata = o.value(u"embedMetadata"_s).toBool(true);
    d.removeSponsors = o.value(u"removeSponsors"_s).toBool();
    d.sponsorCategories = fromArray(o.value(u"sponsorCategories"_s).toArray());
    d.outputDirectory = o.value(u"outputDirectory"_s).toString();
    d.filenamePattern = static_cast<FilenamePattern>(std::clamp(o.value(u"filenamePattern"_s).toInt(), 0, 2));
    d.playlistItems = o.value(u"playlistItems"_s).toString();
    d.isPlaylist = o.value(u"isPlaylist"_s).toBool();
    d.speedLimitKbps = o.value(u"speedLimitKbps"_s).toInt();
    return d;
}

QString formatSelector(const DownloadOptions& options)
{
    switch (options.kind) {
    case DownloadOptions::Kind::Audio:
        return u"ba/b"_s;
    case DownloadOptions::Kind::Custom:
        if (options.customVideoFormat.isEmpty()) {
            return options.customAudioFormat.isEmpty() ? u"b"_s : options.customAudioFormat;
        }
        if (options.customAudioFormat.isEmpty()) {
            return options.customVideoFormat;
        }
        return options.customVideoFormat + u'+' + options.customAudioFormat;
    case DownloadOptions::Kind::Video:
        break;
    }
    // Best video + best audio, falling back to the best progressive stream; the
    // quality ceiling and container preference are expressed through -S so the
    // fallback chain stays short and yt-dlp does the ranking.
    return u"bv*+ba/b"_s;
}

QString formatSort(const DownloadOptions& options)
{
    if (options.kind != DownloadOptions::Kind::Video) {
        return {};
    }
    QStringList parts;
    if (const int height = qualityHeight(options.quality); height > 0) {
        parts << u"res:%1"_s.arg(height);
    }
    switch (options.container) {
    case Container::Mp4:
        parts << u"ext:mp4:m4a"_s;
        break;
    case Container::Webm:
        parts << u"ext:webm:webm"_s;
        break;
    case Container::Mkv:
        break; // anything goes into Matroska
    }
    return parts.join(u',');
}

QString outputTemplate(FilenamePattern pattern, bool playlist)
{
    QString name;
    switch (pattern) {
    case FilenamePattern::Title:
        name = u"%(title)s.%(ext)s"_s;
        break;
    case FilenamePattern::TitleId:
        name = u"%(title)s [%(id)s].%(ext)s"_s;
        break;
    case FilenamePattern::ChannelTitle:
        name = u"%(uploader,channel|Unknown)s - %(title)s.%(ext)s"_s;
        break;
    }
    if (playlist) {
        return u"%(playlist_title,playlist_id|Playlist)s/%(playlist_index|0)03d - "_s + name;
    }
    return name;
}

QString expectedExtension(const DownloadOptions& options)
{
    switch (options.kind) {
    case DownloadOptions::Kind::Audio: {
        const QString ext = audioFormatExtension(options.audioFormat);
        return ext.isEmpty() ? u"m4a"_s : ext;
    }
    case DownloadOptions::Kind::Custom:
        return options.customVideoFormat.isEmpty() ? u"m4a"_s : containerExtension(options.container);
    case DownloadOptions::Kind::Video:
        return containerExtension(options.container);
    }
    return {};
}

QString previewFileName(const DownloadOptions& options, const QString& title, const QString& id,
                        const QString& uploader)
{
    const QString ext = expectedExtension(options);
    const QString safeTitle = sanitizeForTemplate(title.isEmpty() ? u"Video"_s : title);
    switch (options.filenamePattern) {
    case FilenamePattern::Title:
        return safeTitle + u'.' + ext;
    case FilenamePattern::TitleId:
        return safeTitle + u" ["_s + id + u"]."_s + ext;
    case FilenamePattern::ChannelTitle:
        return sanitizeForTemplate(uploader.isEmpty() ? u"Unknown"_s : uploader) + u" - "_s + safeTitle +
               u'.' + ext;
    }
    return safeTitle + u'.' + ext;
}

QStringList baseArguments(const EnginePaths& paths)
{
    QStringList args{
        u"--no-update"_s, u"--no-colors"_s, u"--ignore-config"_s, u"--no-cache-dir"_s,
        u"--no-mtime"_s,  u"--encoding"_s,  u"utf-8"_s,
    };
    if (!paths.ffmpeg.isEmpty()) {
        args << u"--ffmpeg-location"_s << paths.ffmpeg;
    }
    if (!paths.jsRuntime.isEmpty()) {
        args << u"--js-runtimes"_s << paths.jsRuntime;
    }
    if (!paths.cookiesFile.isEmpty()) {
        args << u"--cookies"_s << paths.cookiesFile;
    }
    return args;
}

QStringList probeArguments(const EnginePaths& paths, const QString& url, bool flatPlaylist)
{
    QStringList args = baseArguments(paths);
    args << u"--dump-single-json"_s << u"--no-warnings"_s;
    if (flatPlaylist) {
        args << u"--flat-playlist"_s;
    } else {
        args << u"--no-playlist"_s;
    }
    args << u"--"_s << url;
    return args;
}

QStringList downloadArguments(const DownloadOptions& options, const EnginePaths& paths, const QString& url)
{
    QStringList args = baseArguments(paths);
    args << u"--newline"_s << u"--no-quiet"_s << u"--progress"_s << u"--continue"_s << u"--no-overwrites"_s;
    // Machine-readable progress: one JSON object per line behind a fixed prefix
    // (ADR-005). `|` gives a default for empty fields so the JSON stays valid.
    args
        << u"--progress-template"_s
        << u"download:"_s + kProgressPrefix +
               u"{\"status\":%(progress.status|)j,\"downloaded\":%(progress.downloaded_bytes|0)j,"
               u"\"total\":%(progress.total_bytes|0)j,\"estimate\":%(progress.total_bytes_estimate|0)j,"
               u"\"speed\":%(progress.speed|0)j,\"eta\":%(progress.eta|0)j,\"filename\":%(progress.filename|)"
               u"j,"
               u"\"index\":%(info.playlist_index|0)j,\"id\":%(info.id|)j,\"count\":%(info.playlist_count|0)j}"_s;
    args << u"--progress-template"_s
         << u"postprocess:"_s + kPostprocessPrefix +
                u"{\"status\":%(progress.status|)j,\"postprocessor\":%(progress.postprocessor|)j,"
                u"\"id\":%(info.id|)j}"_s;
    args << u"--print"_s
         << u"before_dl:"_s + kItemPrefix +
                u"{\"id\":%(id)j,\"title\":%(title)j,\"index\":%(playlist_index|0)j,"
                u"\"count\":%(playlist_count|0)j,\"thumbnail\":%(thumbnail|)j,\"duration\":%(duration|0)j,"
                u"\"uploader\":%(uploader|)j,\"url\":%(webpage_url|)j}"_s;
    args << u"--print"_s << u"after_move:"_s + kFilePrefix + u"%(filepath)s"_s;

    args << u"-f"_s << formatSelector(options);
    if (const QString sort = formatSort(options); !sort.isEmpty()) {
        args << u"-S"_s << sort;
    }
    switch (options.kind) {
    case DownloadOptions::Kind::Video:
    case DownloadOptions::Kind::Custom:
        if (options.kind == DownloadOptions::Kind::Video || !options.customVideoFormat.isEmpty()) {
            const QString ext = containerExtension(options.container);
            args << u"--merge-output-format"_s << ext << u"--remux-video"_s << ext;
        }
        break;
    case DownloadOptions::Kind::Audio: {
        args << u"--extract-audio"_s;
        const QString ext = audioFormatExtension(options.audioFormat);
        args << u"--audio-format"_s << (ext.isEmpty() ? u"best"_s : ext);
        args << u"--audio-quality"_s
             << (options.audioBitrateKbps > 0 ? u"%1K"_s.arg(options.audioBitrateKbps) : u"0"_s);
        break;
    }
    }
    if (!options.subtitleLanguages.isEmpty()) {
        args << u"--sub-langs"_s << options.subtitleLanguages.join(u',') << u"--write-subs"_s;
        if (options.includeAutoSubtitles) {
            args << u"--write-auto-subs"_s;
        }
        if (options.embedSubtitles && options.kind != DownloadOptions::Kind::Audio) {
            args << u"--embed-subs"_s;
        } else {
            args << u"--convert-subs"_s << u"srt"_s;
        }
    }
    if (options.embedThumbnail) {
        args << u"--embed-thumbnail"_s;
    }
    if (options.embedMetadata) {
        args << u"--embed-metadata"_s << u"--embed-chapters"_s;
    }
    if (options.removeSponsors && !options.sponsorCategories.isEmpty()) {
        args << u"--sponsorblock-remove"_s << options.sponsorCategories.join(u',');
    }
    if (options.isPlaylist) {
        args << u"--yes-playlist"_s;
        if (!options.playlistItems.isEmpty()) {
            args << u"--playlist-items"_s << options.playlistItems;
        }
    } else {
        args << u"--no-playlist"_s;
    }
    if (options.speedLimitKbps > 0) {
        args << u"--limit-rate"_s << u"%1K"_s.arg(options.speedLimitKbps);
    }
    const QString dir = options.outputDirectory.isEmpty() ? QDir::homePath() : options.outputDirectory;
    args << u"-o"_s << QDir(dir).filePath(outputTemplate(options.filenamePattern, options.isPlaylist));
    args << u"--"_s << url;
    return args;
}

} // namespace red::core
