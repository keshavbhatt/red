#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

namespace red::ui {

/// Fetches and caches thumbnails for download cards and the download dialog
/// (in memory + the disk HTTP cache). One instance per window.
class ThumbnailCache : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ThumbnailCache)

public:
    explicit ThumbnailCache(QObject* parent = nullptr);
    ~ThumbnailCache() override = default;

    /// Returns the cached pixmap or a null one (and starts a fetch; `ready` follows).
    [[nodiscard]] QPixmap get(const QString& url);

Q_SIGNALS:
    void ready(const QString& url);

private:
    QNetworkAccessManager* m_network;
    QHash<QString, QPixmap> m_cache;
    QSet<QString> m_inFlight;
};

} // namespace red::ui
