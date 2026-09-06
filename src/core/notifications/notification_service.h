#pragma once

#include "core/notifications/notification.h"

#include <QHash>
#include <QObject>

namespace red::core {

class Settings;

/// Assigns ids, fills defaults (desktop entry) and routes a notification to
/// the primary backend with an optional fallback (FEATURES S6).
class NotificationService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(NotificationService)

public:
    /// `primary` may be null (then `fallback` is used directly); `fallback` may be null.
    NotificationService(Settings& settings, INotifier* primary, INotifier* fallback,
                        QObject* parent = nullptr);
    ~NotificationService() override = default;

    /// Returns the id, or 0 when no backend is available.
    quint64 notify(Notification notification);
    void close(quint64 id);

    [[nodiscard]] int activeCount() const { return static_cast<int>(m_active.size()); }
    [[nodiscard]] QString backendName() const;
    [[nodiscard]] bool isAvailable() const { return pick() != nullptr; }

Q_SIGNALS:
    void activated(quint64 id);
    void actionInvoked(quint64 id, const QString& key);
    void closed(quint64 id);

private:
    void attach(INotifier* notifier);
    [[nodiscard]] INotifier* pick() const;
    void handleFailed(quint64 id, const QString& reason);
    void handleClosed(quint64 id);

    Settings& m_settings;
    INotifier* m_primary;
    INotifier* m_fallback;
    QHash<quint64, Notification> m_active;
    QHash<quint64, INotifier*> m_owner;
    quint64 m_nextId = 1;
};

} // namespace red::core
