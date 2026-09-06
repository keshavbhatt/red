#include "web/user_agent.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace red::web {

QString sanitizeUserAgent(const QString& defaultUserAgent)
{
    static const QRegularExpression kQtToken(u"\\s*QtWebEngine/[\\d.]+"_s);
    QString ua = defaultUserAgent;
    ua.remove(kQtToken);
    return ua.simplified();
}

QString firefoxUserAgent()
{
    return u"Mozilla/5.0 (X11; Linux x86_64; rv:141.0) Gecko/20100101 Firefox/141.0"_s;
}

QString tvNavigatorUserAgent(const QString& appVersion)
{
    return u"Mozilla/5.0 (PS4; Leanback Shell) Cobalt/19.lts.0-qa; compatible; Red/"_s + appVersion;
}

QString tvWireUserAgent(const QString& appVersion)
{
    return u"Mozilla/5.0 (PS4; Leanback Shell) Cobalt/25.lts.40.1035033; compatible; Red/"_s + appVersion;
}

QString tvGenericUserAgent(const QString& appVersion)
{
    return u"Red/"_s + appVersion;
}

QString effectiveUserAgent(const QString& engineDefault, const QString& userOverride, core::AppMode mode,
                           core::SignInUserAgent signIn, const QString& appVersion)
{
    const QString trimmed = userOverride.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }
    if (mode == core::AppMode::Tv) {
        return tvNavigatorUserAgent(appVersion);
    }
    if (signIn == core::SignInUserAgent::Firefox) {
        return firefoxUserAgent();
    }
    return sanitizeUserAgent(engineDefault);
}

} // namespace red::web
