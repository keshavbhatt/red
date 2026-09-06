#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// Typed view of yt-dlp's -J output (ADR-005). Only the fields Red shows or
// decides on; everything else stays in the JSON.
namespace red::core {

struct MediaFormat
{
    QString id;     ///< yt-dlp format_id ("137", "251", "18", …)
    QString ext;    ///< container of this stream
    QString vcodec; ///< "none" for audio-only
    QString acodec; ///< "none" for video-only
    int width = 0;
    int height = 0;
    double fps = 0;
    double tbr = 0;       ///< total bitrate kbps
    double abr = 0;       ///< audio bitrate kbps
    qint64 filesize = -1; ///< exact or approximate bytes, -1 unknown
    QString note;         ///< "1080p60", "medium", …
    QString dynamicRange; ///< "SDR", "HDR10", …
    QString language;
    QString protocol; ///< https, m3u8_native, …

    [[nodiscard]] bool hasVideo() const { return !vcodec.isEmpty() && vcodec != QLatin1StringView("none"); }
    [[nodiscard]] bool hasAudio() const { return !acodec.isEmpty() && acodec != QLatin1StringView("none"); }
    [[nodiscard]] bool isVideoOnly() const { return hasVideo() && !hasAudio(); }
    [[nodiscard]] bool isAudioOnly() const { return hasAudio() && !hasVideo(); }
    [[nodiscard]] bool isStoryboard() const { return ext == QLatin1StringView("mhtml"); }
    /// "AVC", "VP9", "AV1", "Opus", "AAC" … for the UI.
    [[nodiscard]] QString codecLabel() const;
    [[nodiscard]] static MediaFormat fromJson(const QJsonObject& object);
};

struct SubtitleTrack
{
    QString language; ///< "en", "de", "en-US"
    QString name;     ///< human name from yt-dlp
    bool automatic = false;
};

struct MediaEntry
{
    QString id;
    QString url; ///< webpage url (or the "url" of a flat entry)
    QString title;
    QString uploader;
    QString thumbnail;
    double duration = 0;
    bool isLive = false;
    qint64 viewCount = -1;
};

struct MediaInfo
{
    enum class Type
    {
        Video,
        Playlist, ///< playlist, channel tab, multi-video page
    };

    Type type = Type::Video;
    QString id;
    QString url;
    QString title;
    QString uploader;
    QString channelUrl;
    QString description;
    QString thumbnail;
    double duration = 0;
    QString durationText;
    qint64 viewCount = -1;
    QString uploadDate; ///< yyyyMMdd
    bool isLive = false;
    bool hasChapters = false;
    QList<MediaFormat> formats;
    QList<SubtitleTrack> subtitles; ///< manual + automatic, manual first
    QList<MediaEntry> entries;      ///< when type == Playlist
    int entryCount = 0;

    /// Video heights available (video streams only), descending, unique.
    [[nodiscard]] QList<int> availableHeights() const;
    /// Manual subtitle languages, then automatic ones.
    [[nodiscard]] QStringList subtitleLanguages(bool includeAutomatic) const;
    [[nodiscard]] bool isPlaylist() const { return type == Type::Playlist; }

    [[nodiscard]] static MediaInfo fromJson(const QJsonObject& object);
};

/// Parses yt-dlp -J output. Returns nullopt-like empty info (id empty) on failure.
[[nodiscard]] MediaInfo parseMediaInfo(const QByteArray& json, QString* error = nullptr);

/// "1:02:03" / "12:34" / "0:42".
[[nodiscard]] QString formatDuration(double seconds);
/// "118 MB", "3.2 GB", "" when < 0.
[[nodiscard]] QString formatBytes(qint64 bytes);
/// "1.2M views" style compact count.
[[nodiscard]] QString formatCount(qint64 count);

} // namespace red::core
