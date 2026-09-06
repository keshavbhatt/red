#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

// The network layer of ad blocking (ADR-003). A BlockList is an immutable
// snapshot: the interceptor holds a shared_ptr to it and swaps the pointer on
// settings change, so the IO thread never locks or reads settings.
namespace red::core {

class BlockList
{
public:
    struct Options
    {
        bool ads = true;
        bool trackers = true;
    };

    /// The curated built-in lists, filtered by the options.
    [[nodiscard]] static BlockList builtin(const Options& options);
    /// An empty list (everything allowed).
    [[nodiscard]] static BlockList none();

    /// True when the request should be blocked. Cost: ≤ 6 hash lookups on the
    /// host labels plus a handful of prefix compares for YouTube hosts.
    [[nodiscard]] bool matches(const QUrl& url) const;
    [[nodiscard]] bool matches(const QString& host, const QString& path) const;

    [[nodiscard]] bool isEmpty() const { return m_hosts.isEmpty() && m_youtubePathPrefixes.isEmpty(); }
    [[nodiscard]] int hostCount() const { return static_cast<int>(m_hosts.size()); }

    /// The curated data, exposed for tests and the settings UI.
    [[nodiscard]] static QStringList adHosts();
    [[nodiscard]] static QStringList trackerHosts();
    [[nodiscard]] static QStringList youtubeAdPaths();
    [[nodiscard]] static QStringList youtubeTrackerPaths();

private:
    QSet<QString> m_hosts;              ///< blocked host suffixes (registrable + subdomains)
    QStringList m_youtubePathPrefixes;  ///< blocked path prefixes on youtube.com hosts
};

} // namespace red::core
