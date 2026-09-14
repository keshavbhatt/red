#include "platform/logging.h"
#include "platform/taskbar_progress.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace red::platform {

namespace {

/// com.canonical.Unity.LauncherEntry "Update": the signal Plasma's task manager
/// (and Unity-style docks) read for the progress bar behind a launcher entry.
/// No service to register; the app URI names the desktop file.
class LauncherEntryProgress : public TaskbarProgress
{
public:
    LauncherEntryProgress(const QString& desktopEntry, QObject* parent)
        : TaskbarProgress(parent)
        , m_appUri(u"application://"_s + desktopEntry +
                   (desktopEntry.endsWith(u".desktop"_s) ? QString() : u".desktop"_s))
    {}

    ~LauncherEntryProgress() override { clear(); }

    void setProgress(double fraction) override
    {
        const double clamped = std::clamp(fraction, 0.0, 1.0);
        // The bar is a few hundred pixels wide at most: 1/500 steps are plenty.
        if (m_visible && std::abs(clamped - m_progress) < 0.002) {
            return;
        }
        m_progress = clamped;
        m_visible = true;
        send();
    }

    void clear() override
    {
        if (!m_visible) {
            return;
        }
        m_visible = false;
        m_progress = 0;
        send();
    }

private:
    void send()
    {
        QDBusMessage signal = QDBusMessage::createSignal(u"/com/canonical/unity/launcherentry/red"_s,
                                                         u"com.canonical.Unity.LauncherEntry"_s, u"Update"_s);
        QVariantMap properties{{u"progress"_s, m_progress}, {u"progress-visible"_s, m_visible}};
        signal << m_appUri << properties;
        if (!QDBusConnection::sessionBus().send(signal)) {
            qCDebug(lcPlatform) << "launcher entry update not sent";
        }
    }

    QString m_appUri;
    double m_progress = 0;
    bool m_visible = false;
};

} // namespace

std::unique_ptr<TaskbarProgress> TaskbarProgress::create(const QString& desktopEntry, QObject* parent)
{
    return std::make_unique<LauncherEntryProgress>(desktopEntry, parent);
}

} // namespace red::platform
