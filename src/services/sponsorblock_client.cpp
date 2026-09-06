#include "services/sponsorblock_client.h"

#include "core/settings/settings.h"
#include "services/logging.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace red::services {

namespace {
constexpr int kCacheLimit = 200;
} // namespace

SponsorBlockClient::SponsorBlockClient(core::Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setTransferTimeout(15000);
    // Category changes affect which segments we ask for: drop the cache.
    connect(&m_settings, &core::Settings::sponsorBlockChanged, this, &SponsorBlockClient::clearCache);
}

void SponsorBlockClient::clearCache()
{
    m_cache.clear();
}

void SponsorBlockClient::request(const QString& videoId)
{
    if (videoId.isEmpty() || !m_settings.sponsorBlockEnabled()) {
        return;
    }
    const QHash<QString, core::SponsorAction> actions = m_settings.sponsorActions();
    if (const auto it = m_cache.constFind(videoId); it != m_cache.constEnd()) {
        const QJsonArray segments = core::sponsorSegmentsForPage(it.value(), actions);
        QTimer::singleShot(0, this, [this, videoId, segments] { Q_EMIT segmentsReady(videoId, segments); });
        return;
    }
    if (m_inFlight.contains(videoId)) {
        return;
    }
    const QStringList categories = core::enabledSponsorCategories(actions);
    if (categories.isEmpty()) {
        return;
    }
    m_inFlight.insert(videoId);
    QNetworkRequest request(core::sponsorSegmentsUrl(videoId, categories));
    request.setHeader(QNetworkRequest::UserAgentHeader, u"Red/10 (+https://github.com/keshavbhatt/red)"_s);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, videoId, actions] {
        reply->deleteLater();
        m_inFlight.remove(videoId);
        QList<core::SponsorSegment> segments;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            segments = core::parseSponsorSegments(reply->readAll(), videoId);
        } else if (status != 404) {
            qCInfo(lcServices) << "sponsorblock" << videoId << "failed:" << reply->errorString();
            return; // transient: do not cache, do not report
        }
        if (m_cache.size() >= kCacheLimit) {
            m_cache.clear();
        }
        m_cache.insert(videoId, segments);
        qCDebug(lcServices) << "sponsorblock" << videoId << segments.size() << "segments";
        Q_EMIT segmentsReady(videoId, core::sponsorSegmentsForPage(segments, actions));
    });
}

} // namespace red::services
