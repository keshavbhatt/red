#include "platform/linux/mpris_service_dbus.h"

#include "platform/linux/mpris_adaptors.h"
#include "platform/logging.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace red::platform::linux_ {

namespace {
const QString kObjectPath = u"/org/mpris/MediaPlayer2"_s;
const QString kPlayerInterface = u"org.mpris.MediaPlayer2.Player"_s;
} // namespace

MprisDBusService::MprisDBusService(const QString& identity, const QString& desktopEntry, QObject* parent)
    : MprisService(parent)
    , m_identity(identity)
    , m_desktopEntry(desktopEntry)
    , m_serviceName(u"org.mpris.MediaPlayer2."_s +
                    QCoreApplication::applicationName().toLower().replace(u'-', u'_'))
{
    m_clock.start();
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcPlatform) << "mpris: no session bus";
        return;
    }
    if (!bus.registerObject(kObjectPath, this, QDBusConnection::ExportAdaptors)) {
        qCWarning(lcPlatform) << "mpris: cannot register object:" << bus.lastError().message();
        return;
    }
    if (!bus.registerService(m_serviceName)) {
        // A second profile/instance: keep going with a pid-suffixed name.
        m_serviceName += u".instance%1"_s.arg(QCoreApplication::applicationPid());
        if (!bus.registerService(m_serviceName)) {
            qCWarning(lcPlatform) << "mpris: cannot register service:" << bus.lastError().message();
            return;
        }
    }
    m_registered = true;
    qCInfo(lcPlatform) << "mpris registered as" << m_serviceName;
}

MprisDBusService::~MprisDBusService()
{
    if (m_registered) {
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.unregisterObject(kObjectPath);
        bus.unregisterService(m_serviceName);
    }
}

QString MprisDBusService::playbackStatus() const
{
    switch (m_state.playback) {
    case core::MediaState::Playback::Playing:
        return u"Playing"_s;
    case core::MediaState::Playback::Paused:
        return u"Paused"_s;
    case core::MediaState::Playback::None:
        break;
    }
    return u"Stopped"_s;
}

QString MprisDBusService::trackId() const
{
    if (m_state.videoId.isEmpty()) {
        return u"/org/mpris/MediaPlayer2/TrackList/NoTrack"_s;
    }
    QString id = m_state.videoId;
    id.replace(u'-', u"_2d"_s).replace(u'_', u"_5f"_s);
    return u"/com/ktechpit/red/track/"_s + id;
}

QVariantMap MprisDBusService::metadata() const
{
    QVariantMap m;
    m.insert(u"mpris:trackid"_s, QVariant::fromValue(QDBusObjectPath(trackId())));
    if (!m_state.hasMedia()) {
        return m;
    }
    if (m_state.duration > 0) {
        m.insert(u"mpris:length"_s, static_cast<qlonglong>(m_state.duration * 1e6));
    }
    if (!m_state.artwork.isEmpty()) {
        m.insert(u"mpris:artUrl"_s, m_state.artwork);
    }
    m.insert(u"xesam:title"_s, m_state.title);
    if (!m_state.artist.isEmpty()) {
        m.insert(u"xesam:artist"_s, QStringList{m_state.artist});
    }
    if (!m_state.album.isEmpty()) {
        m.insert(u"xesam:album"_s, m_state.album);
    }
    const QString url = !m_state.url.isEmpty() ? m_state.url
                        : m_state.videoId.isEmpty()
                            ? QString()
                            : QString(u"https://www.youtube.com/watch?v="_s + m_state.videoId);
    if (!url.isEmpty()) {
        m.insert(u"xesam:url"_s, url);
    }
    return m;
}

qlonglong MprisDBusService::positionUs() const
{
    double position = m_state.position;
    if (m_state.isPlaying()) {
        position += static_cast<double>(m_clock.elapsed() - m_state.reportedAtMs) / 1000.0;
    }
    return static_cast<qlonglong>(std::max(0.0, position) * 1e6);
}

void MprisDBusService::setMediaState(const core::MediaState& state)
{
    const bool trackChanged = state.videoId != m_state.videoId || state.title != m_state.title ||
                              state.album != m_state.album || state.artwork != m_state.artwork ||
                              !qFuzzyCompare(state.duration, m_state.duration);
    const bool statusChanged = state.playback != m_state.playback;
    const bool capsChanged =
        state.canGoNext != m_state.canGoNext || state.canGoPrevious != m_state.canGoPrevious;
    const double expected =
        m_state.position +
        (m_state.isPlaying() ? static_cast<double>(state.reportedAtMs - m_state.reportedAtMs) / 1000.0 : 0.0);
    const bool seeked = std::abs(state.position - expected) > 2.0;
    m_state = state;
    if (!m_registered) {
        return;
    }
    QVariantMap changed;
    if (trackChanged) {
        changed.insert(u"Metadata"_s, metadata());
    }
    if (statusChanged) {
        changed.insert(u"PlaybackStatus"_s, playbackStatus());
    }
    if (trackChanged || capsChanged || statusChanged) {
        changed.insert(u"CanGoNext"_s, state.canGoNext);
        changed.insert(u"CanGoPrevious"_s, state.canGoPrevious);
        changed.insert(u"CanPlay"_s, state.hasMedia());
        changed.insert(u"CanPause"_s, state.hasMedia());
        changed.insert(u"CanSeek"_s, state.hasMedia() && !state.live);
    }
    if (!changed.isEmpty()) {
        emitPropertiesChanged(changed);
    }
    if (seeked && !trackChanged) {
        QDBusMessage signal = QDBusMessage::createSignal(kObjectPath, kPlayerInterface, u"Seeked"_s);
        signal << static_cast<qlonglong>(state.position * 1e6);
        QDBusConnection::sessionBus().send(signal);
    }
}

void MprisDBusService::emitPropertiesChanged(const QVariantMap& changed)
{
    QDBusMessage signal =
        QDBusMessage::createSignal(kObjectPath, u"org.freedesktop.DBus.Properties"_s, u"PropertiesChanged"_s);
    signal << kPlayerInterface << changed << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

} // namespace red::platform::linux_

namespace red::platform {

std::unique_ptr<MprisService> MprisService::create(const QString& identity, const QString& desktopEntry,
                                                   QObject* parent)
{
    return std::make_unique<linux_::MprisDBusService>(identity, desktopEntry, parent);
}

} // namespace red::platform
