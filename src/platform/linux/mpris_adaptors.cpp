#include "platform/linux/mpris_adaptors.h"

#include "platform/linux/mpris_service_dbus.h"

using namespace Qt::StringLiterals;

namespace red::platform::linux_ {

MprisRootAdaptor::MprisRootAdaptor(MprisDBusService* service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{}

QString MprisRootAdaptor::identity() const
{
    return m_service->identity();
}

QString MprisRootAdaptor::desktopEntry() const
{
    return m_service->desktopEntry();
}

void MprisRootAdaptor::Raise()
{
    Q_EMIT m_service->raiseRequested();
}

void MprisRootAdaptor::Quit()
{
    Q_EMIT m_service->quitRequested();
}

MprisPlayerAdaptor::MprisPlayerAdaptor(MprisDBusService* service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{}

QString MprisPlayerAdaptor::playbackStatus() const
{
    return m_service->playbackStatus();
}

QVariantMap MprisPlayerAdaptor::metadata() const
{
    return m_service->metadata();
}

double MprisPlayerAdaptor::volume() const
{
    return m_service->volume();
}

void MprisPlayerAdaptor::setVolume(double volume)
{
    Q_EMIT m_service->commandRequested(u"volume"_s, std::clamp(volume, 0.0, 1.0));
}

qlonglong MprisPlayerAdaptor::position() const
{
    return m_service->positionUs();
}

bool MprisPlayerAdaptor::canGoNext() const
{
    return m_service->state().canGoNext;
}

bool MprisPlayerAdaptor::canGoPrevious() const
{
    return m_service->state().canGoPrevious;
}

bool MprisPlayerAdaptor::canPlay() const
{
    return m_service->state().hasMedia();
}

bool MprisPlayerAdaptor::canPause() const
{
    return m_service->state().hasMedia();
}

bool MprisPlayerAdaptor::canSeek() const
{
    return m_service->state().hasMedia() && !m_service->state().live;
}

void MprisPlayerAdaptor::Next()
{
    Q_EMIT m_service->commandRequested(u"next"_s, 0);
}

void MprisPlayerAdaptor::Previous()
{
    Q_EMIT m_service->commandRequested(u"previous"_s, 0);
}

void MprisPlayerAdaptor::Pause()
{
    Q_EMIT m_service->commandRequested(u"pause"_s, 0);
}

void MprisPlayerAdaptor::PlayPause()
{
    Q_EMIT m_service->commandRequested(u"playPause"_s, 0);
}

void MprisPlayerAdaptor::Stop()
{
    Q_EMIT m_service->commandRequested(u"stop"_s, 0);
}

void MprisPlayerAdaptor::Play()
{
    Q_EMIT m_service->commandRequested(u"play"_s, 0);
}

void MprisPlayerAdaptor::Seek(qlonglong offsetUs)
{
    Q_EMIT m_service->commandRequested(u"seekBy"_s, static_cast<double>(offsetUs) / 1e6);
}

void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath& trackId, qlonglong positionUs)
{
    if (trackId.path() != m_service->trackId()) {
        return;
    }
    Q_EMIT m_service->commandRequested(u"seek"_s, static_cast<double>(positionUs) / 1e6);
}

void MprisPlayerAdaptor::OpenUri(const QString& uri)
{
    Q_EMIT m_service->openUriRequested(uri);
}

} // namespace red::platform::linux_
