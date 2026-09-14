#pragma once

#include <QString>
#include <QUrl>

#include <optional>

// Pure classification of YouTube URL shapes (LESSONS R13). Everything the
// download flow, the rail's Download button and the navigation policy need to
// know about a URL is decided here, with tests.
namespace red::core {

enum class YouTubeUrlKind
{
    NotYouTube,
    Other,    ///< youtube.com but nothing downloadable (home, settings…)
    Video,    ///< watch?v=, youtu.be/, /shorts/, /live/, /embed/
    Playlist, ///< playlist?list=
    Channel,  ///< /@handle, /channel/, /c/, /user/ (+ /videos, /shorts, /streams)
};

struct YouTubeUrlInfo
{
    YouTubeUrlKind kind = YouTubeUrlKind::NotYouTube;
    QString videoId;    ///< 11-char id when kind == Video
    QString playlistId; ///< when present and downloadable (mixes are dropped)
    bool isShorts = false;
    bool isLive = false;
    bool isTv = false; ///< youtube.com/tv (Leanback)
};

/// Hosts whose navigation stays inside the app (YouTube itself plus the Google
/// sign-in / consent / CDN hosts it needs).
[[nodiscard]] bool isYouTubeHost(const QString& host);
[[nodiscard]] bool isGoogleServiceHost(const QString& host);
/// Exactly music.youtube.com (YouTube Music).
[[nodiscard]] bool isYouTubeMusicHost(const QString& host);

/// The `list=` value if yt-dlp can read it as a playlist, else empty.
[[nodiscard]] QString downloadablePlaylistId(const QString& list);
[[nodiscard]] YouTubeUrlInfo classifyYouTubeUrl(const QUrl& url);

/// A canonical https://www.youtube.com/watch?v=<id> URL, or nullopt.
[[nodiscard]] std::optional<QUrl> canonicalVideoUrl(const QUrl& url);
/// https://music.youtube.com/watch?v=<id>, with the playlist when given.
[[nodiscard]] QUrl musicVideoUrl(const QString& videoId, const QString& playlistId = {});
[[nodiscard]] bool isDownloadable(const QUrl& url);
[[nodiscard]] QUrl thumbnailUrl(const QString& videoId, const QString& size = QStringLiteral("mqdefault"));

/// Leanback keeps its route in the hash: https://www.youtube.com/tv#/watch?v=…
[[nodiscard]] QString videoIdFromTvHash(const QString& fragment);

} // namespace red::core
