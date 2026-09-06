#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>

// Backend-agnostic notification model and the interface every backend
// implements (FEATURES S6). Ids are assigned by NotificationService and must
// be tracked by backends so that a click on an *unrelated* notification never
// reaches us (whatsie W#5, 42, 271, 278).
namespace red::core {

struct NotificationAction
{
    QString key;   ///< returned in actionInvoked
    QString label; ///< shown on the button
};

struct Notification
{
    QString title;
    QString body;
    QImage image;       ///< thumbnail, may be null
    bool sound = false; ///< ask the desktop to play its message sound
    int timeoutMs = -1; ///< -1: server default; 0: never expire
    QString desktopEntry;
    QString category = QStringLiteral("transfer.complete");
    QList<NotificationAction> actions; ///< besides the default click
};

class INotifier : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(INotifier)

public:
    using QObject::QObject;
    ~INotifier() override = default;

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual bool isAvailable() const = 0;
    virtual void show(quint64 id, const Notification& notification) = 0;
    virtual void close(quint64 id) = 0;

Q_SIGNALS:
    /// The user clicked / activated this notification (the default action).
    void activated(quint64 id);
    /// The user pressed one of the extra action buttons.
    void actionInvoked(quint64 id, const QString& key);
    /// The notification is gone (expired, dismissed, or closed by us).
    void closed(quint64 id);
    /// The backend could not deliver it; the service may try a fallback.
    void failed(quint64 id, const QString& reason);
};

} // namespace red::core
