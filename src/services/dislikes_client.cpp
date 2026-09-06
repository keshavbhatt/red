#include "services/dislikes_client.h"

#include "core/settings/settings.h"
#include "services/logging.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace red::services {

namespace {
constexpr int kCacheLimit = 300;
}

DislikesClient::DislikesClient(core::Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setTransferTimeout(15000);
}

double DislikesClient::parseDislikes(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        return -1;
    }
    const QJsonValue v = doc.object().value(u"dislikes"_s);
    return v.isDouble() ? v.toDouble() : -1;
}

void DislikesClient::request(const QString& videoId)
{
    if (videoId.isEmpty() || !m_settings.showDislikes()) {
        return;
    }
    if (const auto it = m_cache.constFind(videoId); it != m_cache.constEnd()) {
        const double count = it.value();
        QTimer::singleShot(0, this, [this, videoId, count] { Q_EMIT dislikesReady(videoId, count); });
        return;
    }
    if (m_inFlight.contains(videoId)) {
        return;
    }
    m_inFlight.insert(videoId);
    QUrl url(u"https://returnyoutubedislikeapi.com/votes"_s);
    QUrlQuery query;
    query.addQueryItem(u"videoId"_s, videoId);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"Red/10 (+https://github.com/keshavbhatt/red)"_s);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, videoId] {
        reply->deleteLater();
        m_inFlight.remove(videoId);
        if (reply->error() != QNetworkReply::NoError) {
            qCInfo(lcServices) << "dislikes" << videoId << "failed:" << reply->errorString();
            return;
        }
        const double count = parseDislikes(reply->readAll());
        if (m_cache.size() >= kCacheLimit) {
            m_cache.clear();
        }
        m_cache.insert(videoId, count);
        Q_EMIT dislikesReady(videoId, count);
    });
}

} // namespace red::services
