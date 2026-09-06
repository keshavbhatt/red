#include "web/bridge.h"

#include "web/logging.h"

namespace red::web {

Bridge::Bridge(QObject* parent)
    : QObject(parent)
{}

void Bridge::scriptFailed(const QString& name, const QString& message)
{
    qCWarning(lcWebJs).noquote() << "script" << name << "failed:" << message;
    Q_EMIT scriptFailure(name, message);
}

void Bridge::log(const QString& message)
{
    qCDebug(lcWebJs).noquote() << "script:" << message;
}

void Bridge::mediaState(const QJsonObject& state)
{
    Q_EMIT mediaStateChanged(state);
}

void Bridge::requestSponsorSegments(const QString& videoId)
{
    Q_EMIT sponsorSegmentsRequested(videoId);
}

void Bridge::sponsorSkipped(const QString& category)
{
    qCDebug(lcWebJs) << "sponsorblock skipped" << category;
    Q_EMIT sponsorSegmentSkipped(category);
}

void Bridge::tvSignal(const QString& signal, const QString& argument)
{
    qCInfo(lcWebJs) << "tv signal" << signal << argument;
    Q_EMIT tvSignalReceived(signal, argument);
}

void Bridge::retry()
{
    qCInfo(lcWebJs) << "error page: retry requested";
    Q_EMIT retryRequested();
}

void Bridge::downloadRequested(const QString& url)
{
    qCInfo(lcWebJs) << "download requested from page:" << url;
    Q_EMIT downloadUrlRequested(url);
}

} // namespace red::web
