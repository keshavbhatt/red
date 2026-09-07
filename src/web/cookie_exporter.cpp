#include "web/cookie_exporter.h"

#include "services/netscape_cookies.h"
#include "web/logging.h"

#include <QTemporaryFile>
#include <QWebEngineCookieStore>

using namespace Qt::StringLiterals;

namespace red::web {

CookieExporter::CookieExporter(QWebEngineCookieStore* store, QString databasePath, QObject* parent)
    : QObject(parent)
    , m_store(store)
    , m_databasePath(std::move(databasePath))
{
    connect(store, &QWebEngineCookieStore::cookieAdded, this, &CookieExporter::handleAdded);
    connect(store, &QWebEngineCookieStore::cookieRemoved, this, &CookieExporter::handleRemoved);
    reload();
}

QString CookieExporter::keyOf(const QNetworkCookie& cookie)
{
    return cookie.domain() + u'|' + cookie.path() + u'|' + QString::fromUtf8(cookie.name());
}

void CookieExporter::reload()
{
    m_persisted.clear();
    for (const QNetworkCookie& c : services::readChromiumCookieDatabase(m_databasePath)) {
        if (services::isYouTubeSessionCookie(c)) {
            m_persisted.insert(keyOf(c), c);
        }
    }
    updateSignedIn();
}

void CookieExporter::handleAdded(const QNetworkCookie& cookie)
{
    if (!services::isYouTubeSessionCookie(cookie)) {
        return;
    }
    m_cookies.insert(keyOf(cookie), cookie);
    Q_EMIT cookieChanged(cookie);
    updateSignedIn();
}

void CookieExporter::handleRemoved(const QNetworkCookie& cookie)
{
    const QString key = keyOf(cookie);
    const bool known = m_cookies.remove(key) > 0 || m_persisted.remove(key) > 0;
    if (!known) {
        return;
    }
    updateSignedIn();
}

void CookieExporter::updateSignedIn()
{
    const bool signedIn = looksSignedIn();
    if (signedIn != m_signedIn) {
        m_signedIn = signedIn;
        qCInfo(lcWeb) << "youtube session:" << (signedIn ? "signed in" : "signed out");
        Q_EMIT signedInChanged(signedIn);
    }
}

QString CookieExporter::cookieValue(const QString& domainSuffix, const QByteArray& name) const
{
    for (const QNetworkCookie& c : sessionCookies()) {
        if (c.name() == name && c.domain().endsWith(domainSuffix)) {
            return QString::fromUtf8(c.value());
        }
    }
    return {};
}

QList<QNetworkCookie> CookieExporter::sessionCookies() const
{
    // Persisted first, then whatever changed since (same key wins).
    QHash<QString, QNetworkCookie> merged = m_persisted;
    for (auto it = m_cookies.cbegin(); it != m_cookies.cend(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return merged.values();
}

int CookieExporter::deleteCookies(const QString& domainSuffix)
{
    int removed = 0;
    for (const QNetworkCookie& c : sessionCookies()) {
        const QString domain = c.domain().startsWith(u'.') ? c.domain().mid(1) : c.domain();
        if (domain == domainSuffix || domain.endsWith(u'.' + domainSuffix)) {
            m_store->deleteCookie(c);
            m_persisted.remove(keyOf(c));
            m_cookies.remove(keyOf(c));
            ++removed;
        }
    }
    updateSignedIn();
    return removed;
}

bool CookieExporter::looksSignedIn() const
{
    for (const QNetworkCookie& c : sessionCookies()) {
        const QByteArray name = c.name();
        if ((name == "SAPISID" || name == "SID") && c.domain().endsWith(u"youtube.com"_s)) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<QTemporaryFile> CookieExporter::writeTempFile()
{
    reload(); // catch what Chromium flushed since the last look
    return services::writeCookiesTempFile(sessionCookies());
}

} // namespace red::web
