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
    [[nodiscard]] bool looksSignedIn() const; ///< a SAPISID/SID cookie for youtube.com exists
    /// A fresh 0600 temp file, or nullptr when there is nothing to export.
    [[nodiscard]] std::unique_ptr<QTemporaryFile> writeTempFile() const;

Q_SIGNALS:
    void signedInChanged(bool signedIn);

private:
    static QString keyOf(const QNetworkCookie& cookie);
    void handleAdded(const QNetworkCookie& cookie);
    void handleRemoved(const QNetworkCookie& cookie);

    QHash<QString, QNetworkCookie> m_cookies;
    bool m_signedIn = false;
};

} // namespace red::web
