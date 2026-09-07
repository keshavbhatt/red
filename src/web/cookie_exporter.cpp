#include "web/cookie_exporter.h"

#include "services/netscape_cookies.h"
#include "web/logging.h"

#include <QTemporaryFile>
#include <QWebEngineCookieStore>

using namespace Qt::StringLiterals;

namespace red::web {

CookieExporter::CookieExporter(QWebEngineCookieStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    connect(store, &QWebEngineCookieStore::cookieAdded, this, &CookieExporter::handleAdded);
    connect(store, &QWebEngineCookieStore::cookieRemoved, this, &CookieExporter::handleRemoved);
    store->loadAllCookies();
}

QString CookieExporter::keyOf(const QNetworkCookie& c)
{
    return c.domain() + u'|' + c.path() + u'|' + QString::fromUtf8(c.name());
}

void CookieExporter::handleAdded(const QNetworkCookie& cookie)
{
    if (!services::isYouTubeSessionCookie(cookie)) {
        return;
    }
    m_cookies.insert(keyOf(cookie), cookie);
    Q_EMIT cookieChanged(cookie);
    const bool signedIn = looksSignedIn();
    if (signedIn != m_signedIn) {
        m_signedIn = signedIn;
        qCInfo(lcWeb) << "youtube session:" << (signedIn ? "signed in" : "signed out");
        Q_EMIT signedInChanged(signedIn);
    }
}

void CookieExporter::handleRemoved(const QNetworkCookie& cookie)
{
    if (m_cookies.remove(keyOf(cookie)) == 0) {
        return;
    }
    const bool signedIn = looksSignedIn();
    if (signedIn != m_signedIn) {
        m_signedIn = signedIn;
        qCInfo(lcWeb) << "youtube session:" << (signedIn ? "signed in" : "signed out");
        Q_EMIT signedInChanged(signedIn);
    }
}

QString CookieExporter::cookieValue(const QString& domainSuffix, const QByteArray& name) const
{
    for (const QNetworkCookie& c : m_cookies) {
        if (c.name() == name && c.domain().endsWith(domainSuffix)) {
            return QString::fromUtf8(c.value());
        }
    }
    return {};
}

QList<QNetworkCookie> CookieExporter::sessionCookies() const
{
    return m_cookies.values();
}

int CookieExporter::deleteCookies(const QString& domainSuffix)
{
    int removed = 0;
    for (const QNetworkCookie& c : m_cookies.values()) {
        const QString domain = c.domain().startsWith(u'.') ? c.domain().mid(1) : c.domain();
        if (domain == domainSuffix || domain.endsWith(u'.' + domainSuffix)) {
            m_store->deleteCookie(c);
            ++removed;
        }
    }
    return removed;
}

bool CookieExporter::looksSignedIn() const
{
    for (const QNetworkCookie& c : m_cookies) {
        const QByteArray name = c.name();
        if ((name == "SAPISID" || name == "__Secure-3PAPISID" || name == "SID") &&
            c.domain().contains(u"youtube"_s)) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<QTemporaryFile> CookieExporter::writeTempFile() const
{
    return services::writeCookiesTempFile(sessionCookies());
}

} // namespace red::web
