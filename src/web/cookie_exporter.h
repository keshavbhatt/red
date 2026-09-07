#pragma once

#include <QHash>
#include <QList>
#include <QNetworkCookie>
#include <QObject>

#include <memory>

class QTemporaryFile;
class QWebEngineCookieStore;

namespace red::web {

/// Mirrors the profile's cookie store for the YouTube/Google domains so a
/// Netscape cookie file can be produced for yt-dlp at any moment (ADR-006).
/// The mirror is kept live through cookieAdded/cookieRemoved.
class CookieExporter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CookieExporter)

public:
    explicit CookieExporter(QWebEngineCookieStore* store, QObject* parent = nullptr);
    ~CookieExporter() override = default;

    [[nodiscard]] QList<QNetworkCookie> sessionCookies() const;
    /// The current value of a cookie by name for a domain suffix, or empty.
    [[nodiscard]] QString cookieValue(const QString& domainSuffix, const QByteArray& name) const;
    [[nodiscard]] bool looksSignedIn() const; ///< a SAPISID/SID cookie for youtube.com exists
    /// A fresh 0600 temp file, or nullptr when there is nothing to export.
    [[nodiscard]] std::unique_ptr<QTemporaryFile> writeTempFile() const;
    /// Deletes every cookie whose domain matches the suffix (e.g. "google.com").
    /// Returns how many were removed.
    int deleteCookies(const QString& domainSuffix);

Q_SIGNALS:
    void signedInChanged(bool signedIn);
    /// A YouTube/Google cookie was added or changed.
    void cookieChanged(const QNetworkCookie& cookie);

private:
    static QString keyOf(const QNetworkCookie& cookie);
    void handleAdded(const QNetworkCookie& cookie);
    void handleRemoved(const QNetworkCookie& cookie);

    QWebEngineCookieStore* m_store = nullptr;
    QHash<QString, QNetworkCookie> m_cookies;
    bool m_signedIn = false;
};

} // namespace red::web
