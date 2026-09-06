#pragma once

#include "core/settings/settings.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

// Everything a download job needs, and the pure mapping to yt-dlp arguments
// (ADR-005). Built by the download dialog from the settings defaults, stored
// with the job so a retry reproduces the exact command.
namespace red::core {

struct DownloadOptions
{
    enum class Kind
    {
        Video,  ///< preset: quality ceiling + container
        Audio,  ///< audio-only, optionally converted
        Custom, ///< explicit format ids from the Advanced table
    };

    Kind kind = Kind::Video;
    VideoQuality quality = VideoQuality::Best;
    Container container = Container::Mp4;
    AudioFormat audioFormat = AudioFormat::Best;
    int audioBitrateKbps = 0;  ///< 0 = best
    QString customVideoFormat; ///< Custom: video (or progressive) format id
    QString customAudioFormat; ///< Custom: audio format id (may be empty)

    QStringList subtitleLanguages; ///< empty = none
    bool includeAutoSubtitles = false;
    bool embedSubtitles = true;
    bool embedThumbnail = true;
    bool embedMetadata = true;
    bool removeSponsors = false;
    QStringList sponsorCategories; ///< for --sponsorblock-remove

    QString outputDirectory;
    FilenamePattern filenamePattern = FilenamePattern::Title;
    QString playlistItems; ///< yt-dlp item spec ("1,3-5"); empty = all
    bool isPlaylist = false;
    int speedLimitKbps = 0;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static DownloadOptions fromJson(const QJsonObject& object);
};

/// Paths the runner injects (engine-provided, never persisted with the job).
struct EnginePaths
{
    QString ytdlp;
    QString ffmpeg;      ///< binary or directory; empty = let yt-dlp search PATH
    QString jsRuntime;   ///< "quickjs:/path/qjs", "deno", "node:/path/node"…; empty = default
    QString cookiesFile; ///< Netscape file; empty = none
};

/// yt-dlp's -f selector for the options (pure, tested).
[[nodiscard]] QString formatSelector(const DownloadOptions& options);
/// yt-dlp's -S sort spec for the options ("res:1080,ext:mp4:m4a" …), may be empty.
[[nodiscard]] QString formatSort(const DownloadOptions& options);
/// The -o template (relative to outputDirectory), e.g. "%(title)s [%(id)s].%(ext)s".
[[nodiscard]] QString outputTemplate(FilenamePattern pattern, bool playlist);
/// The container extension the options will produce ("mp4", "mp3", …), for the preview.
[[nodiscard]] QString expectedExtension(const DownloadOptions& options);
/// A filename preview for the dialog ("Title [id].mp4").
[[nodiscard]] QString previewFileName(const DownloadOptions& options, const QString& title, const QString& id,
                                      const QString& uploader);

/// The full argv (without the program) for one download of `url`.
[[nodiscard]] QStringList downloadArguments(const DownloadOptions& options, const EnginePaths& paths,
                                            const QString& url);
/// The argv for probing `url` with -J (flat for playlists/channels).
[[nodiscard]] QStringList probeArguments(const EnginePaths& paths, const QString& url, bool flatPlaylist);

/// Arguments shared by every invocation (--no-update, --ignore-config, …).
[[nodiscard]] QStringList baseArguments(const EnginePaths& paths);

/// The line prefixes the runner parses (see ytdlp_output.h).
inline constexpr QLatin1StringView kProgressPrefix{"RED:"};
inline constexpr QLatin1StringView kPostprocessPrefix{"REDPP:"};
inline constexpr QLatin1StringView kItemPrefix{"REDITEM:"};
inline constexpr QLatin1StringView kFilePrefix{"REDFILE:"};

[[nodiscard]] QString qualityLabel(VideoQuality quality);
[[nodiscard]] int qualityHeight(VideoQuality quality); ///< 0 for Best
[[nodiscard]] QString containerExtension(Container container);
[[nodiscard]] QString audioFormatExtension(AudioFormat format); ///< "" for Best

} // namespace red::core
