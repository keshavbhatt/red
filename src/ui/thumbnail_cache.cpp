#include "ui/thumbnail_cache.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kMaxEntries = 400;
}

ThumbnailCache::ThumbnailCache(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    auto* disk = new QNetworkDiskCache(this);
    disk->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                            u"/thumbnails"_s);
    disk->setMaximumCacheSize(64 * 1024 * 1024);
    m_network->setCache(disk);
    m_network->setTransferTimeout(20000);
}

QPixmap ThumbnailCache::get(const QString& url)
{
    if (url.isEmpty()) {
        return {};
    }
    const auto it = m_cache.constFind(url);
    if (it != m_cache.constEnd()) {
        return it.value();
    }
    if (m_inFlight.contains(url)) {
        return {};
    }
    m_inFlight.insert(url);
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        m_inFlight.remove(url);
        QPixmap pm;
        if (reply->error() == QNetworkReply::NoError) {
            pm.loadFromData(reply->readAll());
        }
        if (m_cache.size() >= kMaxEntries) {
            m_cache.clear();
        }
        m_cache.insert(url, pm); // a null pixmap caches the failure, too
        if (!pm.isNull()) {
            Q_EMIT ready(url);
        }
    });
    return {};
}

} // namespace red::ui
