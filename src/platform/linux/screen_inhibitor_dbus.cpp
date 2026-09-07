#include "platform/logging.h"
#include "platform/screen_inhibitor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;

namespace red::platform {

namespace {

/// org.freedesktop.ScreenSaver Inhibit/UnInhibit (KDE, GNOME via its
/// compatibility service, xfce…). Chromium holds its own wake lock for
/// playing video, so this is the belt to that suspender.
class DBusInhibitor : public ScreenInhibitor
{
public:
    explicit DBusInhibitor(QObject* parent)
        : ScreenInhibitor(parent)
        , m_iface(u"org.freedesktop.ScreenSaver"_s, u"/org/freedesktop/ScreenSaver"_s,
                  u"org.freedesktop.ScreenSaver"_s, QDBusConnection::sessionBus())
    {}

    ~DBusInhibitor() override { release(); }

    void setInhibited(bool inhibited, const QString& reason) override
    {
        if (inhibited == (m_cookie != 0)) {
            return;
        }
        if (!inhibited) {
            release();
            return;
        }
        if (!m_iface.isValid()) {
            return;
        }
        const QDBusReply<uint> reply =
            m_iface.call(u"Inhibit"_s, QCoreApplication::applicationName(), reason);
        if (reply.isValid()) {
            m_cookie = reply.value();
            qCDebug(lcPlatform) << "screen inhibit acquired" << m_cookie;
        } else {
            qCDebug(lcPlatform) << "screen inhibit failed:" << reply.error().message();
        }
    }

    [[nodiscard]] bool isInhibited() const override { return m_cookie != 0; }

private:
    void release()
    {
        if (m_cookie == 0) {
            return;
        }
        m_iface.call(u"UnInhibit"_s, m_cookie);
        qCDebug(lcPlatform) << "screen inhibit released" << m_cookie;
        m_cookie = 0;
    }

    QDBusInterface m_iface;
    uint m_cookie = 0;
};

/// org.freedesktop.portal.Inhibit — the sandbox-safe route (Flatpak/snap):
/// Inhibit(idle) returns a request object; closing it lifts the inhibit.
class PortalInhibitor : public ScreenInhibitor
{
public:
    explicit PortalInhibitor(QObject* parent)
        : ScreenInhibitor(parent)
    {}
    ~PortalInhibitor() override { release(); }

    void setInhibited(bool inhibited, const QString& reason) override
    {
        if (inhibited == isInhibited()) {
            return;
        }
        if (!inhibited) {
            release();
            return;
        }
        constexpr uint kIdle = 8;
        QDBusMessage call = QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s,
                                                           u"/org/freedesktop/portal/desktop"_s,
                                                           u"org.freedesktop.portal.Inhibit"_s, u"Inhibit"_s);
        call << QString() << kIdle << QVariantMap{{u"reason"_s, reason}};
        const QDBusReply<QDBusObjectPath> reply = QDBusConnection::sessionBus().call(call);
        if (!reply.isValid()) {
            qCWarning(lcPlatform) << "portal inhibit failed:" << reply.error().message();
            return;
        }
        m_request = reply.value().path();
        qCDebug(lcPlatform) << "screen inhibit acquired (portal)" << m_request;
    }

    [[nodiscard]] bool isInhibited() const override { return !m_request.isEmpty(); }

private:
    void release()
    {
        if (m_request.isEmpty()) {
            return;
        }
        QDBusMessage close = QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s, m_request,
                                                            u"org.freedesktop.portal.Request"_s, u"Close"_s);
        QDBusConnection::sessionBus().call(close);
        qCDebug(lcPlatform) << "screen inhibit released (portal)" << m_request;
        m_request.clear();
    }

    QString m_request;
};

} // namespace

std::unique_ptr<ScreenInhibitor> ScreenInhibitor::create(QObject* parent)
{
    // Sandboxed: the portal is what the sandbox lets through. Elsewhere the
    // screen saver interface answers on every desktop that has one.
    if (qEnvironmentVariableIsSet("FLATPAK_ID") || qEnvironmentVariableIsSet("SNAP")) {
        return std::make_unique<PortalInhibitor>(parent);
    }
    return std::make_unique<DBusInhibitor>(parent);
}

} // namespace red::platform
