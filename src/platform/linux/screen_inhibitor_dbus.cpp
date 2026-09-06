#include "platform/logging.h"
#include "platform/screen_inhibitor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

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
        const QDBusReply<uint> reply = m_iface.call(u"Inhibit"_s, QCoreApplication::applicationName(), reason);
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

} // namespace

std::unique_ptr<ScreenInhibitor> ScreenInhibitor::create(QObject* parent)
{
    return std::make_unique<DBusInhibitor>(parent);
}

} // namespace red::platform
