#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

namespace red::ui {

/// Fetches and caches thumbnails for download cards and the download dialog.
/// Three layers: memory, the HTTP disk cache, and a keepsake per download
/// (`keep()`): a small JPEG under the app data folder that outlives YouTube's
/// URL — a private or removed video still has its picture on the card. A
/// YouTube frame that is not served (i9.ytimg.com maxres) falls back to the
/// standard i.ytimg.com one. One instance per window.
class ThumbnailCache : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ThumbnailCache)

public:
    explicit ThumbnailCache(QObject* parent = nullptr);
    ~ThumbnailCache() override = default;

    /// Returns the cached pixmap or a null one (and starts a fetch; `ready`
    /// follows). Local paths ("/…") are read from disk.
    [[nodiscard]] QPixmap get(const QString& url);

    /// Stores the picture for `url` as the keepsake of download `jobId` and
    /// returns its local path, or empty when nothing is available yet (then
    /// `ready(url)` will fire once it is).
    [[nodiscard]] QString keep(const QString& url, quint64 jobId);
    /// Deletes the keepsake of a download.
    void forget(quint64 jobId);
    /// Turns the full-size "<id>-full.jpg" yt-dlp wrote into the card-size
    /// "<id>.jpg" keepsake and returns that path (or `path` unchanged).
    [[nodiscard]] QString compact(const QString& path);
    [[nodiscard]] static QString keepsakeDirectory();

Q_SIGNALS:
    void ready(const QString& url);

private:
    void fetch(const QString& key, const QString& url, bool allowFallback);
    [[nodiscard]] static QString fallbackFor(const QString& url);

    QNetworkAccessManager* m_network;
    QHash<QString, QPixmap> m_cache;
    QSet<QString> m_inFlight;
};

} // namespace red::ui
