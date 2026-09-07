#include "ui/thumbnail_cache.h"

#include "core/youtube_url.h"
#include "ui/logging.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kMaxEntries = 400;
constexpr int kKeepsakeWidth = 480; // enough for cards and the dialog hero at 2x
} // namespace

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

QString ThumbnailCache::keepsakeDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/thumbnails"_s;
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
    if (url.startsWith(u'/')) {
        QPixmap pm(url);
        if (m_cache.size() >= kMaxEntries) {
            m_cache.clear();
        }
        m_cache.insert(url, pm);
        return pm;
    }
    if (!m_inFlight.contains(url)) {
        fetch(url, url, true);
    }
    return {};
}

QString ThumbnailCache::fallbackFor(const QString& url)
{
    // https://i9.ytimg.com/vi/<id>/maxresdefault.jpg → the frame every video has.
    static const QRegularExpression kFrame(u"^https?://[^/]*ytimg\\.com/vi(?:_webp)?/([A-Za-z0-9_-]{11})/"_s);
    const QRegularExpressionMatch m = kFrame.match(url);
    if (!m.hasMatch()) {
        return {};
    }
    const QString standard = core::thumbnailUrl(m.captured(1)).toString();
    return standard == url ? QString() : standard;
}

void ThumbnailCache::fetch(const QString& key, const QString& url, bool allowFallback)
{
    m_inFlight.insert(key);
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, url, allowFallback] {
        reply->deleteLater();
        QPixmap pm;
        if (reply->error() == QNetworkReply::NoError) {
            pm.loadFromData(reply->readAll());
        }
        if (pm.isNull() && allowFallback) {
            if (const QString fallback = fallbackFor(url); !fallback.isEmpty()) {
                qCDebug(lcUi) << "thumbnail" << url << "unavailable, trying" << fallback;
                fetch(key, fallback, false);
                return;
            }
        }
        m_inFlight.remove(key);
        if (m_cache.size() >= kMaxEntries) {
            m_cache.clear();
        }
        m_cache.insert(key, pm); // a null pixmap caches the failure, too
        if (!pm.isNull()) {
            Q_EMIT ready(key);
        }
    });
}

QString ThumbnailCache::keep(const QString& url, quint64 jobId)
{
    const QPixmap pm = get(url);
    if (pm.isNull()) {
        return {};
    }
    QDir().mkpath(keepsakeDirectory());
    const QString path = keepsakeDirectory() + u"/%1.jpg"_s.arg(jobId);
    const QPixmap scaled =
        pm.width() > kKeepsakeWidth ? pm.scaledToWidth(kKeepsakeWidth, Qt::SmoothTransformation) : pm;
    if (!scaled.save(path, "JPEG", 85)) {
        qCWarning(lcUi) << "could not keep thumbnail" << path;
        return {};
    }
    m_cache.insert(path, scaled);
    return path;
}

QString ThumbnailCache::compact(const QString& path)
{
    if (!path.startsWith(keepsakeDirectory()) || !path.endsWith(u"-full.jpg"_s)) {
        return path;
    }
    QPixmap pm(path);
    if (pm.isNull()) {
        return path;
    }
    const QPixmap scaled =
        pm.width() > kKeepsakeWidth ? pm.scaledToWidth(kKeepsakeWidth, Qt::SmoothTransformation) : pm;
    const QString card = path.chopped(9) + u".jpg"_s; // "-full.jpg" → ".jpg"
    if (!scaled.save(card, "JPEG", 85)) {
        return path;
    }
    QFile::remove(path);
    m_cache.insert(card, scaled);
    return card;
}

void ThumbnailCache::forget(quint64 jobId)
{
    const QStringList paths{QString(keepsakeDirectory() + u"/%1.jpg"_s.arg(jobId)),
                            QString(keepsakeDirectory() + u"/%1-full.jpg"_s.arg(jobId))};
    for (const QString& path : paths) {
        m_cache.remove(path);
        QFile::remove(path);
    }
}

} // namespace red::ui
