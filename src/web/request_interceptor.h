#pragma once

#include "core/blocking/block_list.h"

#include <QWebEngineUrlRequestInterceptor>

#include <atomic>
#include <memory>
#include <mutex>

namespace red::web {

/// Profile-level interceptor (runs on the IO thread). Two jobs:
///  1. Network-layer ad/tracker blocking against an immutable BlockList
///     snapshot (ADR-003) — no settings, no signals, no logging per request.
///  2. TV mode's per-request User-Agent split (ADR-001): www.youtube.com gets
///     the wire Cobalt UA, everything else the generic one.
class RequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RequestInterceptor)

public:
    explicit RequestInterceptor(QObject* parent = nullptr);
    ~RequestInterceptor() override = default;

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

    /// Swaps the block list atomically; the next request sees it.
    void setBlockList(std::shared_ptr<const core::BlockList> list);
    /// Empty strings disable the UA rewrite (desktop mode).
    void setTvUserAgents(const QString& youtubeUa, const QString& genericUa);
    /// User-Agent header for Google's sign-in hosts (desktop mode); empty = none.
    void setSignInUserAgent(const QString& userAgent);

    [[nodiscard]] quint64 blockedCount() const { return m_blocked.load(std::memory_order_relaxed); }
    void resetBlockedCount(quint64 to = 0) { m_blocked.store(to, std::memory_order_relaxed); }

private:
    std::shared_ptr<const core::BlockList> snapshot() const;

    mutable std::mutex m_mutex;
    std::shared_ptr<const core::BlockList> m_list;
    QByteArray m_youtubeUa;
    QByteArray m_genericUa;
    QByteArray m_signInUa;
    std::atomic<quint64> m_blocked{0};
};

} // namespace red::web
