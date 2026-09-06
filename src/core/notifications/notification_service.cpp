#include "core/notifications/notification_service.h"

#include "core/logging.h"
#include "core/settings/settings.h"

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace red::core {

NotificationService::NotificationService(Settings& settings, INotifier* primary, INotifier* fallback,
                                         QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_primary(primary)
    , m_fallback(fallback)
{
    attach(m_primary);
    attach(m_fallback);
    qCInfo(lcCore) << "notification backend:" << backendName();
}

void NotificationService::attach(INotifier* notifier)
{
    if (notifier == nullptr) {
        return;
    }
    connect(notifier, &INotifier::activated, this, [this](quint64 id) {
        if (m_active.contains(id)) {
            Q_EMIT activated(id);
        }
    });
    connect(notifier, &INotifier::actionInvoked, this, [this](quint64 id, const QString& key) {
        if (m_active.contains(id)) {
            Q_EMIT actionInvoked(id, key);
        }
    });
    connect(notifier, &INotifier::closed, this, &NotificationService::handleClosed);
    connect(notifier, &INotifier::failed, this, &NotificationService::handleFailed);
}

INotifier* NotificationService::pick() const
{
    if (m_primary != nullptr && m_primary->isAvailable()) {
        return m_primary;
    }
    if (m_fallback != nullptr && m_fallback->isAvailable()) {
        return m_fallback;
    }
    return nullptr;
}

QString NotificationService::backendName() const
{
    INotifier* n = pick();
    return n != nullptr ? n->name() : u"none"_s;
}

quint64 NotificationService::notify(Notification notification)
{
    INotifier* backend = pick();
    if (backend == nullptr) {
        qCWarning(lcCore) << "no notification backend available";
        return 0;
    }
    if (notification.desktopEntry.isEmpty() && QCoreApplication::instance() != nullptr) {
        notification.desktopEntry = QCoreApplication::instance()->property("desktopFileName").toString();
    }
    const quint64 id = m_nextId++;
    m_active.insert(id, notification);
    m_owner.insert(id, backend);
    backend->show(id, notification);
    return id;
}

void NotificationService::close(quint64 id)
{
    if (INotifier* owner = m_owner.value(id, nullptr); owner != nullptr) {
        owner->close(id);
    }
}

void NotificationService::handleFailed(quint64 id, const QString& reason)
{
    const auto it = m_active.constFind(id);
    if (it == m_active.constEnd()) {
        return;
    }
    INotifier* failedBackend = m_owner.value(id, nullptr);
    INotifier* other = (failedBackend == m_primary) ? m_fallback : nullptr;
    qCWarning(lcCore) << "notification" << id << "failed on" << (failedBackend ? failedBackend->name() : u"?"_s)
                      << ":" << reason;
    if (other != nullptr && other->isAvailable()) {
        m_owner.insert(id, other);
        other->show(id, it.value());
        return;
    }
    m_active.remove(id);
    m_owner.remove(id);
    Q_EMIT closed(id);
}

void NotificationService::handleClosed(quint64 id)
{
    if (m_active.remove(id) > 0) {
        m_owner.remove(id);
        Q_EMIT closed(id);
    }
}

} // namespace red::core
