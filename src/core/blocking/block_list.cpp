#include "core/blocking/block_list.h"

#include "core/youtube_url.h"

using namespace Qt::StringLiterals;

namespace red::core {

// Curated, small, and deliberately conservative: third-party ad and analytics
// networks YouTube pages talk to. Nothing on googlevideo.com / ytimg.com /
// gstatic.com is ever listed (LESSONS R2) — playback and thumbnails need them.
QStringList BlockList::adHosts()
{
    return {
        u"doubleclick.net"_s,
        u"googlesyndication.com"_s,
        u"googleadservices.com"_s,
        u"adservice.google.com"_s,
        u"2mdn.net"_s,
        u"innovid.com"_s,
        u"moatads.com"_s,
        u"fwmrm.net"_s,
        u"adsafeprotected.com"_s,
        u"doubleverify.com"_s,
        u"serving-sys.com"_s,
        u"adnxs.com"_s,
        u"advertising.com"_s,
        u"taboola.com"_s,
        u"outbrain.com"_s,
        u"criteo.com"_s,
        u"casalemedia.com"_s,
        u"pubmatic.com"_s,
        u"rubiconproject.com"_s,
        u"openx.net"_s,
        u"scorecardresearch.com"_s,
        u"imrworldwide.com"_s,
        u"quantserve.com"_s,
    };
}

QStringList BlockList::trackerHosts()
{
    return {
        u"google-analytics.com"_s, u"googletagmanager.com"_s, u"googletagservices.com"_s,
        u"analytics.google.com"_s, u"crashlytics.com"_s,
        u"csp.withgoogle.com"_s, // CSP violation reports
    };
}

QStringList BlockList::youtubeAdPaths()
{
    return {
        u"/pagead/"_s,        u"/api/stats/ads"_s, u"/get_midroll_info"_s, u"/youtubei/v1/player/ad_break"_s,
        u"/pcs/activeview"_s, u"/ptracking"_s,
    };
}

QStringList BlockList::youtubeTrackerPaths()
{
    return {
        u"/youtubei/v1/log_event"_s, u"/api/stats/atr"_s, u"/api/stats/delayplay"_s, u"/csi_204"_s,
        u"/generate_204"_s,
    };
}

BlockList BlockList::builtin(const Options& options)
{
    BlockList list;
    if (options.ads) {
        for (const QString& host : adHosts()) {
            list.m_hosts.insert(host);
        }
        list.m_youtubePathPrefixes += youtubeAdPaths();
    }
    if (options.trackers) {
        for (const QString& host : trackerHosts()) {
            list.m_hosts.insert(host);
        }
        list.m_youtubePathPrefixes += youtubeTrackerPaths();
    }
    return list;
}

BlockList BlockList::none()
{
    return {};
}

bool BlockList::matches(const QUrl& url) const
{
    return matches(url.host().toLower(), url.path());
}

bool BlockList::matches(const QString& host, const QString& path) const
{
    if (host.isEmpty()) {
        return false;
    }
    // Walk the label boundaries: "s0.2mdn.net" → "s0.2mdn.net", "2mdn.net", "net".
    qsizetype from = 0;
    int steps = 0;
    while (from >= 0 && from < host.size() && steps < 6) {
        if (m_hosts.contains(host.mid(from))) {
            return true;
        }
        const qsizetype dot = host.indexOf(u'.', from);
        if (dot < 0) {
            break;
        }
        from = dot + 1;
        ++steps;
    }
    if (m_youtubePathPrefixes.isEmpty()) {
        return false;
    }
    const bool youtube = host.endsWith(QLatin1StringView("youtube.com")) ||
                         host.endsWith(QLatin1StringView("youtube-nocookie.com"));
    if (!youtube) {
        return false;
    }
    for (const QString& prefix : m_youtubePathPrefixes) {
        if (path.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

} // namespace red::core
