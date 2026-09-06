#include "web/request_interceptor.h"

#include "web/logging.h"

#include <QWebEngineUrlRequestInfo>

using namespace Qt::StringLiterals;

namespace red::web {

RequestInterceptor::RequestInterceptor(QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_list(std::make_shared<const core::BlockList>(core::BlockList::none()))
{}

void RequestInterceptor::setBlockList(std::shared_ptr<const core::BlockList> list)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_list = std::move(list);
}

void RequestInterceptor::setTvUserAgents(const QString& youtubeUa, const QString& genericUa)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_youtubeUa = youtubeUa.toUtf8();
    m_genericUa = genericUa.toUtf8();
}

std::shared_ptr<const core::BlockList> RequestInterceptor::snapshot() const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_list;
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    const QUrl url = info.requestUrl();
    const QString host = url.host();

    // The UA split needs the same lock as the list; take both once.
    QByteArray youtubeUa;
    QByteArray genericUa;
    std::shared_ptr<const core::BlockList> list;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        list = m_list;
        youtubeUa = m_youtubeUa;
        genericUa = m_genericUa;
    }
    if (!youtubeUa.isEmpty()) {
        info.setHttpHeader("User-Agent",
                           host == QLatin1StringView("www.youtube.com") ? youtubeUa : genericUa);
    }
    if (list->isEmpty() || url.scheme() == u"data"_s || url.scheme() == u"blob"_s) {
        return;
    }
    if (list->matches(host.toLower(), url.path())) {
        info.block(true);
        const quint64 n = m_blocked.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n % 100 == 0) {
            qCDebug(lcWeb) << "blocked requests so far:" << n;
        }
    }
}

} // namespace red::web
