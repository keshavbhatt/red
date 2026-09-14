#include "core/youtube_url.h"

#include <QRegularExpression>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace red::core {

namespace {

bool hostMatches(const QString& host, QLatin1StringView domain)
{
    if (host == domain) {
        return true;
    }
    return host.size() > domain.size() && host.endsWith(domain) &&
           host.at(host.size() - domain.size() - 1) == u'.';
}

bool looksLikeVideoId(const QString& id)
{
    static const QRegularExpression kId(u"^[A-Za-z0-9_-]{11}$"_s);
    return kId.match(id).hasMatch();
}

} // namespace

bool isYouTubeHost(const QString& host)
{
    const QString h = host.toLower();
    return hostMatches(h, QLatin1StringView("youtube.com")) ||
           hostMatches(h, QLatin1StringView("youtu.be")) ||
           hostMatches(h, QLatin1StringView("youtube-nocookie.com")) ||
           hostMatches(h, QLatin1StringView("ytimg.com")) ||
           hostMatches(h, QLatin1StringView("googlevideo.com"));
}

bool isGoogleServiceHost(const QString& host)
{
    const QString h = host.toLower();
    return hostMatches(h, QLatin1StringView("google.com")) ||
           hostMatches(h, QLatin1StringView("gstatic.com")) ||
           hostMatches(h, QLatin1StringView("googleapis.com")) ||
           hostMatches(h, QLatin1StringView("googleusercontent.com")) ||
           hostMatches(h, QLatin1StringView("ggpht.com")) || hostMatches(h, QLatin1StringView("gvt1.com")) ||
           hostMatches(h, QLatin1StringView("recaptcha.net")) ||
           hostMatches(h, QLatin1StringView("youtube.com"));
}

bool isYouTubeMusicHost(const QString& host)
{
    return host.compare(QLatin1StringView("music.youtube.com"), Qt::CaseInsensitive) == 0;
}

QString downloadablePlaylistId(const QString& list)
{
    // YouTube mixes ("RD…" radios, incl. RDMM/RDCLAK) are generated per viewer
    // and "unviewable" as playlists: yt-dlp cannot list them, so a watch link
    // inside one is just a video.
    if (list.startsWith(u"RD"_s)) {
        return {};
    }
    return list;
}

YouTubeUrlInfo classifyYouTubeUrl(const QUrl& url)
{
    YouTubeUrlInfo info;
    const QString host = url.host().toLower();
    const QString path = url.path();
    const QUrlQuery query(url);

    if (hostMatches(host, QLatin1StringView("youtu.be"))) {
        const QString id = path.mid(1).section(u'/', 0, 0);
        if (looksLikeVideoId(id)) {
            info.kind = YouTubeUrlKind::Video;
            info.videoId = id;
            info.playlistId = downloadablePlaylistId(query.queryItemValue(u"list"_s));
        } else {
            info.kind = YouTubeUrlKind::Other;
        }
        return info;
    }
    if (!hostMatches(host, QLatin1StringView("youtube.com")) &&
        !hostMatches(host, QLatin1StringView("youtube-nocookie.com"))) {
        return info; // NotYouTube
    }
    info.kind = YouTubeUrlKind::Other;

    if (path == u"/tv"_s || path.startsWith(u"/tv/"_s)) {
        info.isTv = true;
        const QString id = videoIdFromTvHash(url.fragment());
        if (!id.isEmpty()) {
            info.kind = YouTubeUrlKind::Video;
            info.videoId = id;
        }
        return info;
    }

    const QString list = downloadablePlaylistId(query.queryItemValue(u"list"_s));
    if (path == u"/watch"_s) {
        const QString id = query.queryItemValue(u"v"_s);
        if (looksLikeVideoId(id)) {
            info.kind = YouTubeUrlKind::Video;
            info.videoId = id;
            info.playlistId = list;
        }
        return info;
    }
    if (path == u"/playlist"_s && !list.isEmpty()) {
        info.kind = YouTubeUrlKind::Playlist;
        info.playlistId = list;
        return info;
    }
    static const QRegularExpression kShorts(u"^/shorts/([A-Za-z0-9_-]{11})"_s);
    static const QRegularExpression kLive(u"^/live/([A-Za-z0-9_-]{11})"_s);
    static const QRegularExpression kEmbed(u"^/(?:embed|v)/([A-Za-z0-9_-]{11})"_s);
    if (const auto m = kShorts.match(path); m.hasMatch()) {
        info.kind = YouTubeUrlKind::Video;
        info.videoId = m.captured(1);
        info.isShorts = true;
        return info;
    }
    if (const auto m = kLive.match(path); m.hasMatch()) {
        info.kind = YouTubeUrlKind::Video;
        info.videoId = m.captured(1);
        info.isLive = true;
        return info;
    }
    if (const auto m = kEmbed.match(path); m.hasMatch()) {
        info.kind = YouTubeUrlKind::Video;
        info.videoId = m.captured(1);
        return info;
    }
    static const QRegularExpression kChannel(
        u"^/(?:@[^/]+|channel/[^/]+|c/[^/]+|user/[^/]+)(?:/(?:videos|shorts|streams|playlists|featured))?/?$"_s);
    if (kChannel.match(path).hasMatch()) {
        info.kind = YouTubeUrlKind::Channel;
    }
    return info;
}

std::optional<QUrl> canonicalVideoUrl(const QUrl& url)
{
    const YouTubeUrlInfo info = classifyYouTubeUrl(url);
    if (info.kind != YouTubeUrlKind::Video) {
        return std::nullopt;
    }
    return QUrl(u"https://www.youtube.com/watch?v="_s + info.videoId);
}

QUrl musicVideoUrl(const QString& videoId, const QString& playlistId)
{
    QUrl url(QStringLiteral("https://music.youtube.com/watch"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("v"), videoId);
    if (!playlistId.isEmpty()) {
        query.addQueryItem(QStringLiteral("list"), playlistId);
    }
    url.setQuery(query);
    return url;
}

bool isDownloadable(const QUrl& url)
{
    const YouTubeUrlKind kind = classifyYouTubeUrl(url).kind;
    return kind == YouTubeUrlKind::Video || kind == YouTubeUrlKind::Playlist ||
           kind == YouTubeUrlKind::Channel;
}

QUrl thumbnailUrl(const QString& videoId, const QString& size)
{
    return QUrl(u"https://i.ytimg.com/vi/%1/%2.jpg"_s.arg(videoId, size));
}

QString videoIdFromTvHash(const QString& fragment)
{
    // "#/watch?v=abc&list=..." → the hash without '#'
    const QString route = fragment.startsWith(u'/') ? fragment : u'/' + fragment;
    const QUrl fake(u"https://www.youtube.com"_s + route);
    if (fake.path() != u"/watch"_s) {
        return {};
    }
    const QString id = QUrlQuery(fake).queryItemValue(u"v"_s);
    return looksLikeVideoId(id) ? id : QString();
}

} // namespace red::core
