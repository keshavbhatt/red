#include "core/navigation_policy.h"

#include "core/youtube_url.h"

#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace red::core {

bool shouldOpenExternally(const QUrl& url)
{
    if (!url.isValid() || url.isEmpty()) {
        return false;
    }
    const QString scheme = url.scheme();
    if (scheme == u"data"_s || scheme == u"about"_s || scheme == u"blob"_s || scheme == u"qrc"_s) {
        return false;
    }
    if (scheme != u"http"_s && scheme != u"https"_s) {
        return true; // mailto:, magnet: … belong to the desktop
    }
    const QString host = url.host();
    return !isYouTubeHost(host) && !isGoogleServiceHost(host);
}

bool isInAppPopupUrl(const QUrl& url)
{
    if (url.isEmpty()) {
        return true; // about:blank pop-ups decide on their first navigation
    }
    return isGoogleServiceHost(url.host()) || isYouTubeHost(url.host());
}

QUrl unwrapRedirect(const QUrl& url)
{
    if (!isYouTubeHost(url.host()) || url.path() != u"/redirect"_s) {
        return url;
    }
    const QString target = QUrlQuery(url).queryItemValue(u"q"_s, QUrl::FullyDecoded);
    const QUrl unwrapped(target);
    return unwrapped.isValid() && !unwrapped.scheme().isEmpty() ? unwrapped : url;
}

} // namespace red::core
